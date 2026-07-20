#!/usr/bin/env python3
"""Build a complete PSPAchievementsNG game bundle in one command.

The tool consumes a locally captured RetroAchievements JSONL file and its
matching raw JSON response. It produces:

- <GAME_ID>.pach          compiled achievement logic
- <GAME_ID>.pbad          optional badge artwork pack
- <GAME_ID>.game.json     unified validation/performance report
- <GAME_ID>.pach.report.json

All output is built in a staging directory and validated before existing files
are atomically replaced. Captured trigger definitions remain local.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import sys
import tempfile
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

import compile_ra_package as pach

TOOL_VERSION = "2.0.0"
REPORT_SCHEMA_VERSION = 1
GAME_ID_RE = re.compile(r"^[A-Z]{4}-[0-9]{5}$")
CAPTURE_RE = re.compile(
    r"^ra_(?P<ra_id>[0-9]+)_(?P<stamp>[0-9]{8}_[0-9]{6})"
    r"(?P<raw>\.raw)?(?P<extension>\.jsonl|\.json)$"
)
PACH_HEADER = struct.Struct("<4sHH12sIIIIIII")
PBAD_HEADER = struct.Struct("<4sHH12sHHHHIIIII")
PBAD_RECORD = struct.Struct("<III")
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619


class AddGameError(RuntimeError):
    """User-facing build failure."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while True:
            chunk = source.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def fnv1a(data: bytes, checksum: int = FNV_OFFSET) -> int:
    for value in data:
        checksum ^= value
        checksum = (checksum * FNV_PRIME) & 0xFFFFFFFF
    return checksum


def validate_game_id(game_id: str) -> None:
    if not GAME_ID_RE.fullmatch(game_id):
        raise AddGameError(
            "PSP Game ID must look like ULUS-10285 "
            "(four uppercase letters, hyphen, five digits)"
        )


def load_json_object(path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except OSError as error:
        raise AddGameError(f"Could not read {path}: {error}") from error
    except json.JSONDecodeError as error:
        raise AddGameError(f"Invalid JSON in {path}: {error}") from error

    if not isinstance(payload, dict):
        raise AddGameError(f"JSON root must be an object: {path}")
    return payload


def iter_raw_achievements(payload: dict[str, Any]) -> Iterable[dict[str, Any]]:
    sets = payload.get("Sets")
    if not isinstance(sets, list):
        return

    for achievement_set in sets:
        if not isinstance(achievement_set, dict):
            continue
        achievements = achievement_set.get("Achievements")
        if not isinstance(achievements, list):
            continue
        for achievement in achievements:
            if isinstance(achievement, dict):
                yield achievement


def infer_counterpart(path: Path, want_raw: bool) -> Path:
    name = path.name
    if want_raw:
        if name.endswith(".jsonl"):
            return path.with_name(name[:-6] + ".raw.json")
    else:
        if name.endswith(".raw.json"):
            return path.with_name(name[:-9] + ".jsonl")
    raise AddGameError(f"Cannot infer matching capture file from: {path}")


def discover_capture_pair(capture_dir: Path, ra_game_id: int) -> tuple[Path, Path]:
    if not capture_dir.is_dir():
        raise AddGameError(f"Capture directory does not exist: {capture_dir}")

    pairs: list[tuple[str, Path, Path]] = []
    prefix = f"ra_{ra_game_id}_"

    for jsonl_path in capture_dir.glob(prefix + "*.jsonl"):
        raw_path = infer_counterpart(jsonl_path, want_raw=True)
        if raw_path.is_file():
            match = CAPTURE_RE.fullmatch(jsonl_path.name)
            stamp = match.group("stamp") if match else jsonl_path.name
            pairs.append((stamp, jsonl_path, raw_path))

    if not pairs:
        raise AddGameError(
            f"No matching capture pair for RA game {ra_game_id} in {capture_dir}"
        )

    _, jsonl_path, raw_path = max(pairs, key=lambda item: item[0])
    return jsonl_path, raw_path


def resolve_capture_paths(args: argparse.Namespace) -> tuple[Path, Path]:
    jsonl_path: Path | None = args.jsonl
    raw_path: Path | None = args.raw_json

    if jsonl_path is None and raw_path is None:
        if args.ra_game_id is None:
            raise AddGameError(
                "Provide --jsonl and --raw-json, or use --ra-game-id "
                "to discover the latest capture pair"
            )
        jsonl_path, raw_path = discover_capture_pair(
            args.capture_dir.resolve(),
            args.ra_game_id,
        )
    elif jsonl_path is None:
        raw_path = raw_path.resolve()
        jsonl_path = infer_counterpart(raw_path, want_raw=False)
    elif raw_path is None:
        jsonl_path = jsonl_path.resolve()
        raw_path = infer_counterpart(jsonl_path, want_raw=True)
    else:
        jsonl_path = jsonl_path.resolve()
        raw_path = raw_path.resolve()

    if not jsonl_path.is_file():
        raise AddGameError(f"JSONL capture not found: {jsonl_path}")
    if not raw_path.is_file():
        raise AddGameError(f"Raw JSON capture not found: {raw_path}")

    return jsonl_path, raw_path


def validate_capture_pair(
    source_items: list[dict[str, Any]],
    raw_payload: dict[str, Any],
    requested_ra_game_id: int | None,
) -> dict[str, Any]:
    if raw_payload.get("Success") is False:
        raise AddGameError("Raw RetroAchievements response reports Success=false")

    try:
        ra_game_id = int(raw_payload.get("GameId"))
    except (TypeError, ValueError) as error:
        raise AddGameError("Raw JSON does not contain a valid GameId") from error

    if requested_ra_game_id is not None and ra_game_id != requested_ra_game_id:
        raise AddGameError(
            f"RA game mismatch: requested {requested_ra_game_id}, raw JSON is {ra_game_id}"
        )

    jsonl_ids = {int(item["id"]) for item in source_items}
    raw_items = list(iter_raw_achievements(raw_payload))
    raw_ids: set[int] = set()

    for item in raw_items:
        try:
            achievement_id = int(item.get("ID"))
        except (TypeError, ValueError):
            continue
        if achievement_id > 0:
            raw_ids.add(achievement_id)

    missing_metadata = sorted(jsonl_ids - raw_ids)
    extra_metadata = sorted(raw_ids - jsonl_ids)

    if missing_metadata:
        preview = ", ".join(str(value) for value in missing_metadata[:8])
        raise AddGameError(
            "The raw JSON does not match the JSONL capture. "
            f"Missing achievement metadata for: {preview}"
        )

    return {
        "ra_game_id": ra_game_id,
        "title": str(raw_payload.get("Title") or "Unknown game"),
        "console_id": raw_payload.get("ConsoleId"),
        "jsonl_achievement_count": len(jsonl_ids),
        "raw_achievement_count": len(raw_ids),
        "extra_raw_achievement_ids": extra_metadata,
    }


def count_memory_references(
    compiled: list[pach.CompiledAchievement],
) -> tuple[int, int, Counter[str], Counter[str]]:
    total = 0
    unique: set[tuple[int, int, int]] = set()
    operand_types: Counter[str] = Counter()
    flags: Counter[str] = Counter()

    type_names = {
        value: name.removeprefix("OPERAND_")
        for name, value in vars(pach).items()
        if name.startswith("OPERAND_") and isinstance(value, int)
    }
    flag_names = {
        value: (name or "STANDARD")
        for name, value in pach.CONDITION_FLAGS.items()
    }

    for achievement in compiled:
        for group in achievement.groups:
            for condition in group:
                flags[flag_names.get(condition.flag, str(condition.flag))] += 1
                for operand in (condition.left, condition.right):
                    if not operand.is_memory:
                        continue
                    total += 1
                    unique.add((operand.type, operand.state, operand.value))
                    operand_types[type_names.get(operand.type, str(operand.type))] += 1

    return total, len(unique), operand_types, flags


def performance_profile(condition_count: int, memory_references: int) -> dict[str, Any]:
    if condition_count <= 1024:
        return {
            "class": "standard",
            "scheduler": "full-rate",
            "reason": "condition_count <= 1024",
        }
    if condition_count <= 2400:
        return {
            "class": "heavy",
            "scheduler": "adaptive",
            "reason": "condition_count > 1024",
        }
    return {
        "class": "very-heavy",
        "scheduler": "adaptive",
        "reason": (
            "condition_count > 2400; test gameplay performance carefully"
        ),
        "memory_references": memory_references,
    }


def validate_pach(path: Path, expected_game_id: str) -> dict[str, Any]:
    data = path.read_bytes()
    if len(data) < PACH_HEADER.size:
        raise AddGameError("Generated PACH is smaller than its header")

    (
        magic,
        version,
        header_size,
        game_raw,
        achievement_count,
        group_count,
        condition_count,
        string_size,
        flags,
        package_id,
        reserved,
    ) = PACH_HEADER.unpack_from(data, 0)

    game_id = game_raw.split(b"\0", 1)[0].decode("ascii")
    expected_size = (
        header_size
        + achievement_count * pach.ACHIEVEMENT_SIZE
        + group_count * pach.GROUP_SIZE
        + condition_count * pach.CONDITION_SIZE
        + string_size
    )

    if magic != b"PACH":
        raise AddGameError(f"Generated package has invalid magic: {magic!r}")
    if version != pach.FORMAT_VERSION:
        raise AddGameError(f"Unexpected PACH version: {version}")
    if game_id != expected_game_id:
        raise AddGameError(
            f"Generated PACH Game ID mismatch: {game_id} != {expected_game_id}"
        )
    if len(data) != expected_size:
        raise AddGameError(
            f"Generated PACH size mismatch: {len(data)} != {expected_size}"
        )
    if package_id == 0:
        raise AddGameError("Generated PACH has a zero package ID")

    return {
        "version": version,
        "game_id": game_id,
        "package_id": f"0x{package_id:08X}",
        "achievement_count": achievement_count,
        "group_count": group_count,
        "condition_count": condition_count,
        "string_table_size": string_size,
        "flags": flags,
        "reserved": reserved,
        "size": len(data),
        "sha256": sha256_file(path),
    }


def validate_pbad(path: Path, expected_game_id: str) -> dict[str, Any]:
    data = path.read_bytes()
    if len(data) < PBAD_HEADER.size:
        raise AddGameError("Generated PBAD is smaller than its header")

    (
        magic,
        version,
        header_size,
        game_raw,
        width,
        height,
        pixel_format,
        record_size,
        badge_count,
        pixel_data_size,
        pack_id,
        stored_checksum,
        reserved,
    ) = PBAD_HEADER.unpack_from(data, 0)

    game_id = game_raw.split(b"\0", 1)[0].decode("ascii")
    records_size = badge_count * record_size
    expected_size = header_size + records_size + pixel_data_size

    if magic != b"PBAD":
        raise AddGameError(f"Generated badge pack has invalid magic: {magic!r}")
    if game_id != expected_game_id:
        raise AddGameError(
            f"Generated PBAD Game ID mismatch: {game_id} != {expected_game_id}"
        )
    if record_size != PBAD_RECORD.size:
        raise AddGameError(f"Unexpected PBAD record size: {record_size}")
    if len(data) != expected_size:
        raise AddGameError(
            f"Generated PBAD size mismatch: {len(data)} != {expected_size}"
        )

    checksum_header = bytearray(data[:header_size])
    checksum_header[40:44] = b"\0\0\0\0"
    calculated = fnv1a(bytes(checksum_header))
    calculated = fnv1a(data[header_size:], calculated)
    if calculated != stored_checksum:
        raise AddGameError(
            "Generated PBAD checksum mismatch: "
            f"0x{stored_checksum:08X} != 0x{calculated:08X}"
        )

    previous_id = 0
    for index in range(badge_count):
        offset = header_size + index * record_size
        achievement_id, pixel_offset, pixel_size = PBAD_RECORD.unpack_from(data, offset)
        if achievement_id <= previous_id:
            raise AddGameError("PBAD achievement IDs are not strictly increasing")
        if pixel_offset + pixel_size > pixel_data_size:
            raise AddGameError("PBAD pixel record points outside pixel data")
        previous_id = achievement_id

    return {
        "version": version,
        "game_id": game_id,
        "pack_id": f"0x{pack_id:08X}",
        "badge_count": badge_count,
        "image_width": width,
        "image_height": height,
        "pixel_format": pixel_format,
        "pixel_data_size": pixel_data_size,
        "reserved": reserved,
        "size": len(data),
        "sha256": sha256_file(path),
    }


def atomic_publish(staged: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".new")
    shutil.copyfile(staged, temporary)
    with temporary.open("rb") as source:
        os.fsync(source.fileno())
    os.replace(temporary, destination)


def copy_to_psp(files: list[Path], install_root: Path) -> list[dict[str, Any]]:
    games_dir = (
        install_root.resolve()
        / "SEPLUGINS"
        / "PSPAchievementsNG"
        / "games"
    )
    games_dir.mkdir(parents=True, exist_ok=True)

    installed: list[dict[str, Any]] = []
    for source in files:
        destination = games_dir / source.name
        atomic_publish(source, destination)
        source_hash = sha256_file(source)
        destination_hash = sha256_file(destination)
        if source_hash != destination_hash:
            raise AddGameError(f"PSP copy verification failed: {destination}")
        installed.append({
            "source": str(source),
            "destination": str(destination),
            "sha256": destination_hash,
        })
    return installed


def build_game(args: argparse.Namespace) -> dict[str, Any]:
    validate_game_id(args.game_id)
    jsonl_path, raw_path = resolve_capture_paths(args)
    raw_payload = load_json_object(raw_path)

    try:
        source_items = pach.read_jsonl(jsonl_path)
    except (OSError, pach.CompileError, ValueError) as error:
        raise AddGameError(str(error)) from error

    metadata = validate_capture_pair(source_items, raw_payload, args.ra_game_id)

    try:
        compiled, skipped = pach.compile_achievements(source_items, None)
    except (pach.CompileError, ValueError) as error:
        raise AddGameError(str(error)) from error

    if skipped:
        preview = "; ".join(
            f"{item['id']} {item['title']}: {item['reason']}"
            for item in skipped[:5]
        )
        raise AddGameError(
            f"Strict build rejected {len(skipped)} achievement(s): {preview}"
        )
    if not compiled:
        raise AddGameError("No achievements were compiled")

    try:
        package_data, package_report = pach.create_package(compiled, args.game_id)
    except (pach.CompileError, ValueError, struct.error) as error:
        raise AddGameError(str(error)) from error

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    total_points = sum(int(item.get("points", 0)) for item in source_items)
    type_counts = Counter(
        (
            "standard"
            if str(item.get("type") or "").strip().lower() in ("", "none")
            else str(item.get("type")).strip()
        )
        for item in source_items
    )
    memory_refs, unique_refs, operand_types, flag_counts = count_memory_references(
        compiled
    )

    with tempfile.TemporaryDirectory(
        prefix=f".{args.game_id}.",
        dir=output_dir,
    ) as temporary_name:
        staging = Path(temporary_name)
        staged_pach = staging / f"{args.game_id}.pach"
        staged_pach.write_bytes(package_data)
        pach_validation = validate_pach(staged_pach, args.game_id)

        package_report.update({
            "source_count": len(source_items),
            "compiled_count": len(compiled),
            "skipped_count": 0,
            "skipped": [],
            "point_total": total_points,
            "memory_reference_count": memory_refs,
            "unique_memory_reference_count": unique_refs,
        })
        staged_pach_report = staging / f"{args.game_id}.pach.report.json"
        staged_pach_report.write_text(
            json.dumps(package_report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        pbad_validation: dict[str, Any] | None = None
        staged_pbad: Path | None = None
        badge_failures = 0

        if not args.no_badges:
            try:
                import compile_badges as badges
            except SystemExit as error:
                raise AddGameError(str(error)) from error

            # Build artwork only for achievement IDs present in the strict
            # JSONL package. This prevents optional/bonus sets in raw JSON
            # from silently adding unrelated records to the PBAD file.
            compiled_ids = {int(item["id"]) for item in source_items}
            filtered_raw = dict(raw_payload)
            filtered_sets: list[dict[str, Any]] = []
            for achievement_set in raw_payload.get("Sets", []):
                if not isinstance(achievement_set, dict):
                    continue
                filtered_set = dict(achievement_set)
                filtered_achievements = []
                for item in achievement_set.get("Achievements", []):
                    if not isinstance(item, dict):
                        continue
                    try:
                        achievement_id = int(item.get("ID"))
                    except (TypeError, ValueError):
                        continue
                    if achievement_id in compiled_ids:
                        filtered_achievements.append(item)
                if filtered_achievements:
                    filtered_set["Achievements"] = filtered_achievements
                    filtered_sets.append(filtered_set)
            filtered_raw["Sets"] = filtered_sets
            staged_badge_source = staging / "badge_source.raw.json"
            staged_badge_source.write_text(
                json.dumps(filtered_raw, ensure_ascii=False) + "\n",
                encoding="utf-8",
            )

            staged_pbad = staging / f"{args.game_id}.pbad"
            try:
                _, badge_failures, _ = badges.build_pack(
                    raw_json_path=staged_badge_source,
                    game_id=args.game_id,
                    output_path=staged_pbad,
                    cache_dir=args.cache_dir.resolve(),
                    timeout=args.timeout,
                    refresh=args.refresh_badges,
                    allow_missing=args.allow_missing_badges,
                )
            except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as error:
                raise AddGameError(f"Badge build failed: {error}") from error
            pbad_validation = validate_pbad(staged_pbad, args.game_id)

        generated_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
        report: dict[str, Any] = {
            "schema_version": REPORT_SCHEMA_VERSION,
            "tool": "PSPAchievementsNG add_game.py",
            "tool_version": TOOL_VERSION,
            "generated_at_utc": generated_at,
            "status": "ready",
            "game": {
                "psp_game_id": args.game_id,
                "ra_game_id": metadata["ra_game_id"],
                "title": metadata["title"],
                "console_id": metadata["console_id"],
            },
            "sources": {
                "jsonl": str(jsonl_path),
                "jsonl_sha256": sha256_file(jsonl_path),
                "raw_json": str(raw_path),
                "raw_json_sha256": sha256_file(raw_path),
                "jsonl_achievement_count": metadata["jsonl_achievement_count"],
                "raw_achievement_count": metadata["raw_achievement_count"],
                "extra_raw_achievement_ids": metadata["extra_raw_achievement_ids"],
            },
            "achievements": {
                "count": len(compiled),
                "points": total_points,
                "types": dict(sorted(type_counts.items())),
                "groups": package_report["group_count"],
                "conditions": package_report["condition_count"],
                "memory_references": memory_refs,
                "unique_memory_references": unique_refs,
                "operand_types": dict(sorted(operand_types.items())),
                "condition_flags": dict(sorted(flag_counts.items())),
            },
            "performance": performance_profile(
                package_report["condition_count"],
                memory_refs,
            ),
            "package": {
                "filename": f"{args.game_id}.pach",
                **pach_validation,
            },
            "badges": (
                {
                    "enabled": True,
                    "filename": f"{args.game_id}.pbad",
                    "skipped": badge_failures,
                    **(pbad_validation or {}),
                }
                if staged_pbad is not None
                else {"enabled": False}
            ),
            "network_identity": {
                "psp_game_id": args.game_id,
                "ra_game_id": metadata["ra_game_id"],
                "package_id": pach_validation["package_id"],
                "badge_pack_id": (
                    pbad_validation["pack_id"] if pbad_validation else None
                ),
                "achievement_ids": package_report["achievement_ids"],
            },
            "install_destination": (
                f"/SEPLUGINS/PSPAchievementsNG/games/{args.game_id}.*"
            ),
            "published_files": [
                str(output_dir / f"{args.game_id}.pach"),
                *(
                    [str(output_dir / f"{args.game_id}.pbad")]
                    if staged_pbad is not None
                    else []
                ),
                str(output_dir / f"{args.game_id}.pach.report.json"),
                str(output_dir / f"{args.game_id}.game.json"),
            ],
        }

        staged_report = staging / f"{args.game_id}.game.json"
        staged_report.write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        final_pach = output_dir / staged_pach.name
        final_pach_report = output_dir / staged_pach_report.name
        final_report = output_dir / staged_report.name
        atomic_publish(staged_pach, final_pach)
        atomic_publish(staged_pach_report, final_pach_report)
        if staged_pbad is not None:
            atomic_publish(staged_pbad, output_dir / staged_pbad.name)
        atomic_publish(staged_report, final_report)

    published = [
        output_dir / f"{args.game_id}.pach",
        output_dir / f"{args.game_id}.pach.report.json",
        output_dir / f"{args.game_id}.game.json",
    ]
    if not args.no_badges:
        published.insert(1, output_dir / f"{args.game_id}.pbad")

    if args.install_root is not None:
        install_files = [output_dir / f"{args.game_id}.pach"]
        if not args.no_badges:
            install_files.append(output_dir / f"{args.game_id}.pbad")
        report["installed_files"] = copy_to_psp(install_files, args.install_root)
        report_path = output_dir / f"{args.game_id}.game.json"
        report_path.write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compile a complete PSPAchievementsNG game bundle from a local "
            "RetroAchievements capture."
        )
    )
    parser.add_argument(
        "--game-id",
        required=True,
        help="PSP disc ID, for example ULES-00151",
    )
    parser.add_argument(
        "--jsonl",
        type=Path,
        help="Captured ra_<id>_<timestamp>.jsonl file",
    )
    parser.add_argument(
        "--raw-json",
        type=Path,
        help="Matching ra_<id>_<timestamp>.raw.json file",
    )
    parser.add_argument(
        "--ra-game-id",
        type=int,
        help="RA Game ID; also enables latest-pair discovery when paths are omitted",
    )
    parser.add_argument(
        "--capture-dir",
        type=Path,
        default=Path("tools/ra_import/captures"),
        help="Capture directory used by --ra-game-id discovery",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("dist/games"),
        help="Destination for .pach, .pbad and reports",
    )
    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=Path("tools/ra_import/badge_cache"),
        help="Downloaded badge PNG cache",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=30.0,
        help="Badge download timeout per image in seconds",
    )
    parser.add_argument(
        "--refresh-badges",
        action="store_true",
        help="Redownload badge images even when cached",
    )
    parser.add_argument(
        "--allow-missing-badges",
        action="store_true",
        help="Allow a partial .pbad when individual images fail",
    )
    parser.add_argument(
        "--no-badges",
        action="store_true",
        help="Build only the .pach package and reports",
    )
    parser.add_argument(
        "--install-root",
        type=Path,
        help=(
            "Optional mounted Memory Stick root. Copies only .pach/.pbad to "
            "SEPLUGINS/PSPAchievementsNG/games"
        ),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        report = build_game(args)
    except (AddGameError, OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    game = report["game"]
    achievements = report["achievements"]
    performance = report["performance"]
    package = report["package"]
    badges = report["badges"]

    print()
    print("PSPAchievementsNG game bundle ready")
    print(f"Game: {game['title']}")
    print(f"PSP Game ID: {game['psp_game_id']}")
    print(f"RA Game ID: {game['ra_game_id']}")
    print(f"Achievements: {achievements['count']}")
    print(f"Points: {achievements['points']}")
    print(f"Groups: {achievements['groups']}")
    print(f"Conditions: {achievements['conditions']}")
    print(f"Memory references: {achievements['memory_references']}")
    print(f"Performance: {performance['class']} / {performance['scheduler']}")
    print(f"PACH: {package['filename']} ({package['package_id']})")
    if badges["enabled"]:
        print(
            f"PBAD: {badges['filename']} "
            f"({badges['badge_count']} badges, {badges['pack_id']})"
        )
    else:
        print("PBAD: skipped")
    print(f"Report: {args.output_dir.resolve() / (args.game_id + '.game.json')}")
    if args.install_root is not None:
        print(f"Installed to PSP root: {args.install_root.resolve()}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
