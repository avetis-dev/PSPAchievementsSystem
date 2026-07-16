#!/usr/bin/env python3
"""Reject private/generated files that are tracked or present in a clean export."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

FORBIDDEN_DIR_NAMES = {
    ".patch-backups",
    "badge_cache",
    "captures",
    "logs",
    "profiles",
}

SKIP_DIR_NAMES = {
    ".git",
    ".idea",
    ".venv",
    ".vscode",
    "__pycache__",
    "dist",
    "release",
    "releases",
    "venv",
}

FORBIDDEN_EXACT_NAMES = {
    ".DS_Store",
    "Thumbs.db",
    "desktop.ini",
    "exports.c",
}

FORBIDDEN_SUFFIXES = {
    ".a",
    ".bak",
    ".dat",
    ".dmp",
    ".dump",
    ".elf",
    ".game.json",
    ".jsonl",
    ".key",
    ".log",
    ".map",
    ".o",
    ".pach",
    ".pbad",
    ".pbp",
    ".pem",
    ".prx",
    ".pyc",
    ".pyo",
    ".raw.json",
    ".sfo",
    ".tmp",
}


def is_forbidden_suffix(name: str) -> bool:
    lowered = name.lower()
    return any(lowered.endswith(suffix) for suffix in FORBIDDEN_SUFFIXES)


def git_tracked_paths(root: Path) -> list[Path] | None:
    if not (root / ".git").exists():
        return None

    result = subprocess.run(
        ["git", "-C", str(root), "ls-files", "-z"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        return None

    return [
        root / item.decode("utf-8")
        for item in result.stdout.split(b"\0")
        if item
    ]


def exported_paths(root: Path) -> list[Path]:
    paths: list[Path] = []
    for path in root.rglob("*"):
        relative_parts = path.relative_to(root).parts
        if any(part in SKIP_DIR_NAMES for part in relative_parts):
            continue
        paths.append(path)
    return paths


def scan(root: Path) -> list[str]:
    problems: list[str] = []
    paths = git_tracked_paths(root) or exported_paths(root)

    for path in sorted(paths):
        relative = path.relative_to(root)

        if any(part in FORBIDDEN_DIR_NAMES for part in relative.parts[:-1]):
            problems.append(f"private/generated path: {relative}")
            continue

        if path.is_dir():
            if path.name in FORBIDDEN_DIR_NAMES:
                problems.append(f"private/generated directory: {relative}")
            continue

        if path.name in FORBIDDEN_EXACT_NAMES or path.name.startswith("._"):
            problems.append(f"forbidden file: {relative}")
            continue

        if is_forbidden_suffix(path.name):
            problems.append(f"private/generated file: {relative}")

    return sorted(set(problems))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "root",
        nargs="?",
        default=Path(__file__).resolve().parents[1],
        type=Path,
    )
    args = parser.parse_args()
    root = args.root.resolve()

    problems = scan(root)
    if problems:
        print("Public tree validation failed:", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1

    print(f"Public tree validation passed: {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
