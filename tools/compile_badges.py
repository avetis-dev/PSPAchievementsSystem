#!/usr/bin/env python3
"""Compile RetroAchievements badge images into a PSP-friendly PBAD pack.

The output contains 32x32 RGB565 images keyed by achievement ID. It is
intentionally separate from the .pach trigger package, so badge artwork can be
added, removed, or rebuilt without changing profile/package IDs.
"""

from __future__ import annotations

import argparse
import binascii
import json
import os
import struct
import sys
import tempfile
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

try:
    from PIL import Image, ImageOps
except ImportError as error:  # pragma: no cover - user environment message
    raise SystemExit(
        "Pillow is required. Install it with: python3 -m pip install --user Pillow"
    ) from error

MAGIC = b"PBAD"
FORMAT_VERSION = 1
HEADER_SIZE = 48
RECORD_SIZE = 12
IMAGE_WIDTH = 32
IMAGE_HEIGHT = 32
PIXEL_FORMAT_RGB565 = 1
PIXEL_BYTES = IMAGE_WIDTH * IMAGE_HEIGHT * 2
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619
GAME_ID_LENGTH = 10
GAME_ID_FIELD_SIZE = 12
MAX_BADGES = 192
USER_AGENT = "PSPAchievementsNG/2.0.0 badge compiler"


@dataclass(frozen=True)
class AchievementBadge:
    achievement_id: int
    badge_name: str
    url: str
    title: str


def fnv1a(data: bytes, checksum: int = FNV_OFFSET) -> int:
    for value in data:
        checksum ^= value
        checksum = (checksum * FNV_PRIME) & 0xFFFFFFFF
    return checksum


def validate_game_id(game_id: str) -> None:
    if (
        len(game_id) != GAME_ID_LENGTH
        or not game_id[:4].isalpha()
        or not game_id[:4].isupper()
        or game_id[4] != "-"
        or not game_id[5:].isdigit()
    ):
        raise ValueError(
            "Game ID must look like ULUS-10285 (four uppercase letters, hyphen, five digits)"
        )


def iter_achievements(payload: dict[str, Any]) -> Iterable[dict[str, Any]]:
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


def extract_badges(payload: dict[str, Any]) -> list[AchievementBadge]:
    result: dict[int, AchievementBadge] = {}

    for item in iter_achievements(payload):
        achievement_id = item.get("ID")
        badge_name = item.get("BadgeName")
        url = item.get("BadgeURL")
        title = item.get("Title") or "Achievement"

        try:
            achievement_id = int(achievement_id)
        except (TypeError, ValueError):
            continue

        if achievement_id <= 0:
            continue

        badge_name = str(badge_name or "").strip()
        url = str(url or "").strip()

        if not url and badge_name:
            url = f"https://media.retroachievements.org/Badge/{badge_name}.png"

        if not badge_name or not url:
            continue

        result[achievement_id] = AchievementBadge(
            achievement_id=achievement_id,
            badge_name=badge_name,
            url=url,
            title=str(title),
        )

    badges = [result[key] for key in sorted(result)]

    if not badges:
        raise ValueError("No achievement badge metadata was found in the raw JSON")

    if len(badges) > MAX_BADGES:
        raise ValueError(f"Badge count {len(badges)} exceeds limit {MAX_BADGES}")

    return badges


def download_badge(
    badge: AchievementBadge,
    cache_dir: Path,
    timeout: float,
    refresh: bool,
) -> Path:
    cache_dir.mkdir(parents=True, exist_ok=True)
    cache_path = cache_dir / f"{badge.badge_name}.png"

    if cache_path.is_file() and cache_path.stat().st_size > 0 and not refresh:
        return cache_path

    request = urllib.request.Request(
        badge.url,
        headers={"User-Agent": USER_AGENT, "Accept": "image/png,image/*"},
    )

    temporary_path = cache_path.with_suffix(".png.tmp")

    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            data = response.read()
    except (urllib.error.URLError, TimeoutError, OSError) as error:
        raise RuntimeError(
            f"Could not download badge {badge.badge_name} for "
            f"{badge.achievement_id} ({badge.title}): {error}"
        ) from error

    if not data:
        raise RuntimeError(f"Badge download returned no data: {badge.url}")

    temporary_path.write_bytes(data)
    os.replace(temporary_path, cache_path)
    return cache_path


def image_to_rgb565(path: Path) -> bytes:
    try:
        with Image.open(path) as source:
            source.load()
            rgba = source.convert("RGBA")
    except Exception as error:
        raise RuntimeError(f"Could not decode image {path}: {error}") from error

    background = Image.new("RGBA", rgba.size, (12, 16, 23, 255))
    background.alpha_composite(rgba)
    rgb = background.convert("RGB")
    rgb = ImageOps.fit(
        rgb,
        (IMAGE_WIDTH, IMAGE_HEIGHT),
        method=Image.Resampling.LANCZOS,
        centering=(0.5, 0.5),
    )

    output = bytearray(PIXEL_BYTES)
    position = 0
    pixels = rgb.load()

    for y in range(IMAGE_HEIGHT):
        for x in range(IMAGE_WIDTH):
            red, green, blue = pixels[x, y]
            value = (
                (red >> 3)
                | ((green >> 2) << 5)
                | ((blue >> 3) << 11)
            )
            struct.pack_into("<H", output, position, value)
            position += 2

    return bytes(output)


def build_pack(
    raw_json_path: Path,
    game_id: str,
    output_path: Path,
    cache_dir: Path,
    timeout: float,
    refresh: bool,
    allow_missing: bool,
) -> tuple[int, int, int]:
    validate_game_id(game_id)

    payload = json.loads(raw_json_path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError("Raw JSON root must be an object")

    badges = extract_badges(payload)
    compiled: list[tuple[AchievementBadge, bytes]] = []
    failures: list[str] = []

    for index, badge in enumerate(badges, start=1):
        print(
            f"[{index:02d}/{len(badges):02d}] "
            f"{badge.achievement_id}: {badge.title}"
        )

        try:
            image_path = download_badge(
                badge,
                cache_dir,
                timeout,
                refresh,
            )
            pixels = image_to_rgb565(image_path)
        except Exception as error:
            if not allow_missing:
                raise
            failures.append(str(error))
            print(f"  skipped: {error}", file=sys.stderr)
            continue

        compiled.append((badge, pixels))

    if not compiled:
        raise RuntimeError("No badges were compiled")

    records = bytearray()
    pixel_data = bytearray()

    for badge, pixels in compiled:
        offset = len(pixel_data)
        records += struct.pack(
            "<III",
            badge.achievement_id,
            offset,
            len(pixels),
        )
        pixel_data += pixels

    pack_id = binascii.crc32(records + pixel_data) & 0xFFFFFFFF
    if pack_id == 0:
        pack_id = 1

    game_id_field = game_id.encode("ascii") + b"\0\0"
    if len(game_id_field) != GAME_ID_FIELD_SIZE:
        raise AssertionError("Unexpected game ID field length")

    header = bytearray(HEADER_SIZE)
    struct.pack_into(
        "<4sHH12sHHHHIIIII",
        header,
        0,
        MAGIC,
        FORMAT_VERSION,
        HEADER_SIZE,
        game_id_field,
        IMAGE_WIDTH,
        IMAGE_HEIGHT,
        PIXEL_FORMAT_RGB565,
        RECORD_SIZE,
        len(compiled),
        len(pixel_data),
        pack_id,
        0,  # checksum is zero while hashing
        0,  # reserved
    )

    checksum = fnv1a(bytes(header))
    checksum = fnv1a(bytes(records), checksum)
    checksum = fnv1a(bytes(pixel_data), checksum)
    struct.pack_into("<I", header, 40, checksum)

    output_path.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.NamedTemporaryFile(
        mode="wb",
        prefix=output_path.name + ".",
        suffix=".tmp",
        dir=output_path.parent,
        delete=False,
    ) as temporary:
        temporary_path = Path(temporary.name)
        temporary.write(header)
        temporary.write(records)
        temporary.write(pixel_data)
        temporary.flush()
        os.fsync(temporary.fileno())

    os.replace(temporary_path, output_path)

    print()
    print(f"PBAD v{FORMAT_VERSION}: {output_path}")
    print(f"Pack ID: 0x{pack_id:08X}")
    print(f"Badges: {len(compiled)}")
    print(f"Skipped: {len(failures)}")
    print(f"Image: {IMAGE_WIDTH}x{IMAGE_HEIGHT} RGB565")
    print(f"Size: {output_path.stat().st_size} bytes")
    print(f"Cache: {cache_dir}")

    return len(compiled), len(failures), pack_id


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compile RA badge artwork into a PSPAchievementsNG .pbad pack."
    )
    parser.add_argument("raw_json", type=Path, help="Captured RA raw JSON")
    parser.add_argument("game_id", help="PSP disc ID, for example ULUS-10285")
    parser.add_argument("output", type=Path, help="Output .pbad path")
    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=Path("tools/ra_import/badge_cache"),
        help="Downloaded PNG cache directory",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=30.0,
        help="Download timeout per image in seconds",
    )
    parser.add_argument(
        "--refresh",
        action="store_true",
        help="Redownload images even when cached",
    )
    parser.add_argument(
        "--allow-missing",
        action="store_true",
        help="Build a partial pack when individual badges fail",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        build_pack(
            raw_json_path=args.raw_json.resolve(),
            game_id=args.game_id,
            output_path=args.output.resolve(),
            cache_dir=args.cache_dir.resolve(),
            timeout=args.timeout,
            refresh=args.refresh,
            allow_missing=args.allow_missing,
        )
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
