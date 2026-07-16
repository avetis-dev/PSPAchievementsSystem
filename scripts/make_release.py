#!/usr/bin/env python3
"""Create a deterministic PSPAchievementsNG end-user release ZIP."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import shutil
import sys
import tempfile
import zipfile

RELEASE_NAME = "PSPAchievementsSystem"
PLUGIN_DIRECTORY = "PSPAchievementsNG"
SAFE_GAME_SUFFIXES = {".pach", ".pbad"}
FORBIDDEN_RELEASE_SUFFIXES = {
    ".bak",
    ".dat",
    ".jsonl",
    ".log",
    ".raw.json",
    ".tmp",
}
ZIP_TIMESTAMP = (2026, 1, 1, 0, 0, 0)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--game-data-dir", type=Path)
    return parser.parse_args()


def validate_version(version: str) -> None:
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:[-+][A-Za-z0-9.-]+)?", version):
        raise ValueError(f"invalid version: {version}")


def copy_required(source: Path, target: Path) -> None:
    if not source.is_file():
        raise FileNotFoundError(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def copy_game_data(source_dir: Path, target_dir: Path) -> int:
    if not source_dir.is_dir():
        raise FileNotFoundError(source_dir)

    count = 0
    for source in sorted(source_dir.iterdir()):
        if not source.is_file() or source.suffix.lower() not in SAFE_GAME_SUFFIXES:
            continue
        target_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target_dir / source.name)
        count += 1
    return count


def reject_forbidden_files(root: Path) -> None:
    problems: list[str] = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        lowered = path.name.lower()
        if any(lowered.endswith(suffix) for suffix in FORBIDDEN_RELEASE_SUFFIXES):
            problems.append(str(path.relative_to(root)))
    if problems:
        joined = "\n".join(f"  - {problem}" for problem in problems)
        raise RuntimeError(f"forbidden files in release:\n{joined}")


def add_file(archive: zipfile.ZipFile, source: Path, archive_name: str) -> None:
    info = zipfile.ZipInfo(archive_name, ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = (0o100644 & 0xFFFF) << 16
    archive.writestr(info, source.read_bytes())


def make_zip(source_root: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    with zipfile.ZipFile(output, "w") as archive:
        for source in sorted(source_root.rglob("*")):
            if not source.is_file():
                continue
            relative = source.relative_to(source_root.parent).as_posix()
            add_file(archive, source, relative)


def main() -> int:
    args = parse_args()
    validate_version(args.version)

    project_root = args.project_root.resolve()
    output_dir = (args.output_dir or project_root / "release").resolve()
    prx = project_root / "dist" / "PSPAchievementsNG.prx"

    if not prx.is_file():
        print(
            "Missing dist/PSPAchievementsNG.prx. Run 'make dist' first.",
            file=sys.stderr,
        )
        return 2

    logger_source = (project_root / "plugin/src/logger.c").read_text(encoding="utf-8")
    if f"PSPAchievementsNG {args.version}\\n" not in logger_source:
        print(
            f"Version mismatch: logger.c does not identify {args.version}.",
            file=sys.stderr,
        )
        return 2

    bundle_name = f"{RELEASE_NAME}-v{args.version}"

    with tempfile.TemporaryDirectory(prefix="pspng-release-") as temp_dir:
        staging_parent = Path(temp_dir)
        staging = staging_parent / bundle_name
        plugin_root = staging / "SEPLUGINS" / PLUGIN_DIRECTORY
        games_root = plugin_root / "games"

        copy_required(prx, plugin_root / "PSPAchievementsNG.prx")
        copy_required(project_root / "config.ini", plugin_root / "config.ini")
        copy_required(project_root / "release-data/README.txt", staging / "README.txt")
        copy_required(project_root / "release-data/games/README.txt", games_root / "README.txt")

        for document in (
            "README.md",
            "LICENSE_NOTICE.md",
            "CHANGELOG.md",
            "docs/INSTALLATION.md",
            "docs/LEGACY_MIGRATION.md",
            "docs/CONFIGURATION.md",
            "docs/SUPPORTED_GAMES.md",
            "docs/TROUBLESHOOTING.md",
        ):
            source = project_root / document
            target = staging / Path(document).name
            copy_required(source, target)

        (plugin_root / "profiles").mkdir(parents=True, exist_ok=True)
        (plugin_root / "logs").mkdir(parents=True, exist_ok=True)

        game_file_count = 0
        if args.game_data_dir is not None:
            game_file_count = copy_game_data(args.game_data_dir.resolve(), games_root)

        reject_forbidden_files(staging)

        output = output_dir / f"{bundle_name}.zip"
        make_zip(staging, output)
        digest = hashlib.sha256(output.read_bytes()).hexdigest()
        checksum = output.with_suffix(output.suffix + ".sha256")
        checksum.write_text(f"{digest}  {output.name}\n", encoding="ascii")

    print(f"Release: {output}")
    print(f"SHA-256: {digest}")
    print(f"External game files included: {game_file_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
