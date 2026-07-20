#!/usr/bin/env python3
"""Compile locally captured RetroAchievements JSONL into PACH v3.

PACH v3 compiles RetroAchievements condition logic into a compact bytecode.
Supported features include:
- core and alternative groups
- little and big-endian integer memory reads
- bit, nibble, bit-count, float32 and pointer reads
- current, Delta and Prior values
- all six comparisons and hit counts
- PauseIf, ResetIf, ResetNextIf
- AddSource, SubSource and AddAddress
- AddHits and SubHits
- AndNext, OrNext, Measured, MeasuredIf and Trigger

Definitions remain local. The generated package contains compiled conditions,
not the original source JSONL.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

FORMAT_VERSION = 3
HEADER_SIZE = 48
ACHIEVEMENT_SIZE = 32
GROUP_SIZE = 8
CONDITION_SIZE = 20

MAX_ACHIEVEMENTS = 192
MAX_GROUPS = 512
MAX_CONDITIONS = 12288
MAX_STRING_TABLE = 32768

FNV_OFFSET = 2166136261
FNV_PRIME = 16777619

OPERAND_VALUE_U32 = 0
OPERAND_VALUE_FLOAT = 1
OPERAND_MEMORY_U8 = 2
OPERAND_MEMORY_U32 = 3
OPERAND_MEMORY_FLOAT = 4
OPERAND_MEMORY_BIT0 = 5
OPERAND_MEMORY_U16 = 6
OPERAND_MEMORY_U24 = 7
OPERAND_MEMORY_U16_BE = 8
OPERAND_MEMORY_U24_BE = 9
OPERAND_MEMORY_U32_BE = 10
OPERAND_MEMORY_BIT1 = 11
OPERAND_MEMORY_BIT2 = 12
OPERAND_MEMORY_BIT3 = 13
OPERAND_MEMORY_BIT4 = 14
OPERAND_MEMORY_BIT5 = 15
OPERAND_MEMORY_BIT6 = 16
OPERAND_MEMORY_BIT7 = 17
OPERAND_MEMORY_LOWER4 = 18
OPERAND_MEMORY_UPPER4 = 19
OPERAND_MEMORY_BITCOUNT = 20

STATE_CURRENT = 0
STATE_DELTA = 1
STATE_PRIOR = 2

COMPARE_NONE = 0xFF
COMPARISONS = {
    "=": 0,
    "!=": 1,
    "<": 2,
    "<=": 3,
    ">": 4,
    ">=": 5,
}

CONDITION_FLAGS = {
    "": 0,
    "P:": 1,
    "R:": 2,
    "Z:": 3,
    "A:": 4,
    "I:": 5,
    "N:": 6,
    "O:": 7,
    "M:": 8,
    "Q:": 9,
    "T:": 10,
    "B:": 11,
    "C:": 12,
    "D:": 13,
}

MODIFIER_FLAGS = {"A:", "B:", "I:"}
LINK_FLAGS = {"N:", "O:"}

TYPE_CODES = {
    "": 0,
    "None": 0,
    "none": 0,
    "progression": 1,
    "missable": 2,
    "win_condition": 3,
}

GAME_ID_RE = re.compile(r"^[A-Z]{4}-[0-9]{5}$")
MEMORY_RE = re.compile(
    r"^(?P<state>[dp]?)(?P<kind>"
    r"0xM|0xN|0xO|0xP|0xQ|0xR|0xS|0xT|"
    r"0xL|0xU|0xH|0x\s|0xW|0xX|0xI|0xJ|0xG|0xK|fF"
    r")(?P<address>[0-9A-Fa-f]{1,8})"
    r"(?:/(?P<divisor>[0-9]+))?$"
)
FLOAT_RE = re.compile(
    r"^f(?P<value>[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)$"
)
INTEGER_RE = re.compile(r"^[+-]?\d+$")
HEX_INTEGER_RE = re.compile(r"^H([0-9A-Fa-f]+)$")
CONDITION_RE = re.compile(r"^(.*?)(!=|<=|>=|=|<|>)(.*)$")
HIT_COUNT_RE = re.compile(r"\.(?P<count>\d+)\.$")


class CompileError(ValueError):
    pass


@dataclass(frozen=True)
class Operand:
    type: int
    state: int
    value: int
    modifier: int = 0xFFFF

    @property
    def is_float(self) -> bool:
        return self.type in (OPERAND_VALUE_FLOAT, OPERAND_MEMORY_FLOAT)

    @property
    def is_memory(self) -> bool:
        return OPERAND_MEMORY_U8 <= self.type <= OPERAND_MEMORY_BITCOUNT


@dataclass(frozen=True)
class Condition:
    left: Operand
    right: Operand
    comparison: int
    flag: int
    hit_target: int


@dataclass
class CompiledAchievement:
    source: dict[str, Any]
    groups: list[list[Condition]]


class StringTable:
    def __init__(self) -> None:
        self.data = bytearray(b"\0")
        self.offsets: dict[str, int] = {"": 0}

    def add(self, text: Any) -> int:
        value = "" if text is None else str(text)
        if value in self.offsets:
            return self.offsets[value]

        encoded = value.encode("utf-8")
        offset = len(self.data)
        self.data.extend(encoded)
        self.data.append(0)
        self.offsets[value] = offset
        return offset


def fnv1a(data: bytes) -> int:
    value = FNV_OFFSET
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & 0xFFFFFFFF
    return value


def float_bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def parse_operand(text: str) -> Operand:
    match = MEMORY_RE.fullmatch(text)
    if match:
        state = {
            "": STATE_CURRENT,
            "d": STATE_DELTA,
            "p": STATE_PRIOR,
        }[match.group("state")]

        operand_type = {
            "0xH": OPERAND_MEMORY_U8,
            "0x ": OPERAND_MEMORY_U16,
            "0xW": OPERAND_MEMORY_U24,
            "0xX": OPERAND_MEMORY_U32,
            "0xI": OPERAND_MEMORY_U16_BE,
            "0xJ": OPERAND_MEMORY_U24_BE,
            "0xG": OPERAND_MEMORY_U32_BE,
            "0xM": OPERAND_MEMORY_BIT0,
            "0xN": OPERAND_MEMORY_BIT1,
            "0xO": OPERAND_MEMORY_BIT2,
            "0xP": OPERAND_MEMORY_BIT3,
            "0xQ": OPERAND_MEMORY_BIT4,
            "0xR": OPERAND_MEMORY_BIT5,
            "0xS": OPERAND_MEMORY_BIT6,
            "0xT": OPERAND_MEMORY_BIT7,
            "0xL": OPERAND_MEMORY_LOWER4,
            "0xU": OPERAND_MEMORY_UPPER4,
            "0xK": OPERAND_MEMORY_BITCOUNT,
            "fF": OPERAND_MEMORY_FLOAT,
        }[match.group("kind")]

        divisor_text = match.group("divisor")
        modifier = 0xFFFF

        if divisor_text is not None:
            divisor = int(divisor_text, 10)
            if not 2 <= divisor <= 0x7FFF:
                raise CompileError(
                    f"memory divisor outside supported range 2..32767: {divisor}"
                )
            if operand_type in (OPERAND_MEMORY_FLOAT,):
                raise CompileError("float memory division is not supported")
            modifier = divisor

        return Operand(
            operand_type,
            state,
            int(match.group("address"), 16),
            modifier,
        )

    match = FLOAT_RE.fullmatch(text)
    if match:
        return Operand(
            OPERAND_VALUE_FLOAT,
            STATE_CURRENT,
            float_bits(float(match.group("value"))),
        )

    if INTEGER_RE.fullmatch(text):
        value = int(text, 10)
        if not -(1 << 31) <= value <= 0xFFFFFFFF:
            raise CompileError(f"integer constant outside 32-bit range: {text}")
        return Operand(OPERAND_VALUE_U32, STATE_CURRENT, value & 0xFFFFFFFF)

    match = HEX_INTEGER_RE.fullmatch(text)
    if match:
        value = int(match.group(1), 16)
        if value > 0xFFFFFFFF:
            raise CompileError(f"hex constant outside 32-bit range: {text}")
        return Operand(OPERAND_VALUE_U32, STATE_CURRENT, value)

    raise CompileError(f"unsupported operand: {text}")


def split_flag(token: str) -> tuple[str, str]:
    if len(token) >= 2 and token[1] == ":":
        prefix = token[:2]
        if prefix not in CONDITION_FLAGS:
            raise CompileError(f"condition flag is not supported: {prefix}")
        return prefix, token[2:]

    return "", token


def remove_hit_count(body: str) -> tuple[str, int]:
    match = HIT_COUNT_RE.search(body)
    if not match:
        return body, 0

    count = int(match.group("count"))
    if not 1 <= count <= 0xFFFF:
        raise CompileError(f"hit target outside uint16: {count}")

    return body[: match.start()], count


def parse_condition(token: str) -> Condition:
    if not token:
        raise CompileError("empty condition")

    prefix, body = split_flag(token)
    body, hit_target = remove_hit_count(body)
    flag = CONDITION_FLAGS[prefix]

    if prefix in MODIFIER_FLAGS:
        if hit_target:
            raise CompileError(f"{prefix} cannot have a hit count")

        left = parse_operand(body)

        if prefix == "I:" and left.is_float:
            raise CompileError("AddAddress requires an integer operand")

        return Condition(
            left=left,
            right=Operand(OPERAND_VALUE_U32, STATE_CURRENT, 0),
            comparison=COMPARE_NONE,
            flag=flag,
            hit_target=0,
        )

    match = CONDITION_RE.fullmatch(body)
    if not match:
        raise CompileError(f"cannot parse condition: {token}")

    left = parse_operand(match.group(1))
    right = parse_operand(match.group(3))

    if left.is_float != right.is_float:
        # RA allows an integer literal on the other side of a float operand
        # (for example, fF...=0). Interpret that literal as a float value.
        if left.is_float and right.type == OPERAND_VALUE_U32:
            signed = right.value if right.value < 0x80000000 else right.value - 0x100000000
            right = Operand(
                OPERAND_VALUE_FLOAT,
                STATE_CURRENT,
                float_bits(float(signed)),
            )
        elif right.is_float and left.type == OPERAND_VALUE_U32:
            signed = left.value if left.value < 0x80000000 else left.value - 0x100000000
            left = Operand(
                OPERAND_VALUE_FLOAT,
                STATE_CURRENT,
                float_bits(float(signed)),
            )
        else:
            raise CompileError("mixed float/integer comparison is not supported")

    return Condition(
        left=left,
        right=right,
        comparison=COMPARISONS[match.group(2)],
        flag=flag,
        hit_target=hit_target,
    )


def validate_group(group: list[Condition]) -> None:
    cursor = 0
    hit_chain_active = False
    modifier_flags = (
        CONDITION_FLAGS["A:"],
        CONDITION_FLAGS["B:"],
        CONDITION_FLAGS["I:"],
    )
    link_flags = (
        CONDITION_FLAGS["N:"],
        CONDITION_FLAGS["O:"],
    )
    hit_modifier_flags = (
        CONDITION_FLAGS["C:"],
        CONDITION_FLAGS["D:"],
    )

    while cursor < len(group):
        while cursor < len(group) and group[cursor].flag in modifier_flags:
            cursor += 1

        if cursor >= len(group):
            raise CompileError("group ends with a modifier")

        while group[cursor].flag in link_flags:
            cursor += 1

            while cursor < len(group) and group[cursor].flag in modifier_flags:
                cursor += 1

            if cursor >= len(group):
                raise CompileError("logical chain has no final condition")

        hit_chain_active = group[cursor].flag in hit_modifier_flags
        cursor += 1

    if hit_chain_active:
        raise CompileError("group ends with AddHits/SubHits")



def split_groups(definition: str) -> list[str]:
    groups: list[str] = []
    start = 0

    for index, character in enumerate(definition):
        if character != "S":
            continue

        # `0xS` is the Bit6 memory-size prefix, not an alternative-group
        # separator. No other serialized operand contains an uppercase S.
        if index >= 2 and definition[index - 2:index] == "0x":
            continue

        groups.append(definition[start:index])
        start = index + 1

    groups.append(definition[start:])
    return groups


def parse_definition(definition: str) -> list[list[Condition]]:
    if not definition:
        raise CompileError("empty definition")

    raw_groups = split_groups(definition)

    # A trailing S is a terminator in serialized RA definitions, not an
    # additional empty alternative group. Leading empty core groups remain
    # significant and are preserved.
    while len(raw_groups) > 1 and raw_groups[-1] == "":
        raw_groups.pop()

    groups: list[list[Condition]] = []

    for raw_group in raw_groups:
        if raw_group == "":
            groups.append([])
            continue

        tokens = [token for token in raw_group.split("_") if token]
        group = [parse_condition(token) for token in tokens]
        validate_group(group)
        groups.append(group)

    if not groups:
        raise CompileError("definition has no groups")

    return groups


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    achievements: list[dict[str, Any]] = []
    seen_ids: set[int] = set()

    with path.open("r", encoding="utf-8") as source:
        for line_number, line in enumerate(source, start=1):
            if not line.strip():
                continue

            try:
                item = json.loads(line)
            except json.JSONDecodeError as error:
                raise CompileError(
                    f"line {line_number}: invalid JSON: {error}"
                ) from error

            if not isinstance(item, dict):
                raise CompileError(f"line {line_number}: expected an object")

            achievement_id = item.get("id")
            if not isinstance(achievement_id, int) or achievement_id <= 0:
                raise CompileError(f"line {line_number}: invalid achievement id")
            if achievement_id in seen_ids:
                raise CompileError(
                    f"line {line_number}: duplicate achievement id {achievement_id}"
                )

            seen_ids.add(achievement_id)
            achievements.append(item)

    return achievements


def compile_achievements(
    source_items: list[dict[str, Any]],
    include_ids: set[int] | None,
) -> tuple[list[CompiledAchievement], list[dict[str, Any]]]:
    compiled: list[CompiledAchievement] = []
    skipped: list[dict[str, Any]] = []

    for item in source_items:
        achievement_id = int(item["id"])
        if include_ids is not None and achievement_id not in include_ids:
            continue

        try:
            groups = parse_definition(str(item.get("definition", "")))
            compiled.append(CompiledAchievement(item, groups))
        except CompileError as error:
            skipped.append({
                "id": achievement_id,
                "title": str(item.get("title", "")),
                "reason": str(error),
            })

    return compiled, skipped


def pack_operand(operand: Operand) -> bytes:
    return struct.pack(
        "<BBHI",
        operand.type,
        operand.state,
        operand.modifier,
        operand.value,
    )


def create_package(
    achievements: list[CompiledAchievement],
    game_id: str,
) -> tuple[bytes, dict[str, Any]]:
    strings = StringTable()
    achievement_records = bytearray()
    group_records = bytearray()
    condition_records = bytearray()

    group_count = 0
    condition_count = 0

    for compiled in achievements:
        item = compiled.source
        first_group = group_count

        for group in compiled.groups:
            first_condition = condition_count

            for condition in group:
                condition_records.extend(pack_operand(condition.left))
                condition_records.extend(pack_operand(condition.right))
                condition_records.extend(struct.pack(
                    "<BBH",
                    condition.comparison,
                    condition.flag,
                    condition.hit_target,
                ))
                condition_count += 1

            group_records.extend(struct.pack(
                "<IHH",
                first_condition,
                len(group),
                0,
            ))
            group_count += 1

        title_offset = strings.add(item.get("title", ""))
        description_offset = strings.add(item.get("desc", ""))
        author_offset = strings.add(item.get("author", ""))

        type_name = "" if item.get("type") is None else str(item.get("type"))
        type_code = TYPE_CODES.get(type_name, 0)

        try:
            badge_id = int(str(item.get("badge", "0")) or "0")
        except ValueError:
            badge_id = 0

        points = int(item.get("points", 0))
        if not 0 <= points <= 0xFFFF:
            raise CompileError(f"achievement {item['id']}: points outside uint16")

        achievement_records.extend(struct.pack(
            "<IHBBHHIIIII",
            int(item["id"]),
            points,
            type_code,
            0,
            first_group,
            len(compiled.groups),
            title_offset,
            description_offset,
            badge_id,
            author_offset,
            0,
        ))

    if len(achievements) > MAX_ACHIEVEMENTS:
        raise CompileError(f"too many achievements: {len(achievements)}")
    if group_count > MAX_GROUPS:
        raise CompileError(f"too many groups: {group_count}")
    if condition_count > MAX_CONDITIONS:
        raise CompileError(f"too many conditions: {condition_count}")
    if len(strings.data) > MAX_STRING_TABLE:
        raise CompileError(f"string table is too large: {len(strings.data)} bytes")

    body = bytes(
        achievement_records +
        group_records +
        condition_records +
        strings.data
    )

    package_id = fnv1a(body) or 1
    game_field = game_id.encode("ascii") + b"\0\0"

    header = struct.pack(
        "<4sHH12sIIIIIII",
        b"PACH",
        FORMAT_VERSION,
        HEADER_SIZE,
        game_field,
        len(achievements),
        group_count,
        condition_count,
        len(strings.data),
        0,
        package_id,
        0,
    )

    if len(header) != HEADER_SIZE:
        raise AssertionError("internal header size mismatch")
    if len(achievement_records) != len(achievements) * ACHIEVEMENT_SIZE:
        raise AssertionError("internal achievement record size mismatch")
    if len(group_records) != group_count * GROUP_SIZE:
        raise AssertionError("internal group record size mismatch")
    if len(condition_records) != condition_count * CONDITION_SIZE:
        raise AssertionError("internal condition record size mismatch")

    report = {
        "format_version": FORMAT_VERSION,
        "game_id": game_id,
        "package_id": f"0x{package_id:08X}",
        "achievement_count": len(achievements),
        "group_count": group_count,
        "condition_count": condition_count,
        "string_table_size": len(strings.data),
        "package_size": len(header) + len(body),
        "achievement_ids": [int(item.source["id"]) for item in achievements],
    }

    return header + body, report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compile captured RA JSONL into a PSPAchievementsNG PACH v3 package."
    )
    parser.add_argument("input", type=Path, help="Captured RA .jsonl file")
    parser.add_argument("game_id", help="PSP disc ID, for example ULUS-10285")
    parser.add_argument("output", type=Path, help="Output .pach path")
    parser.add_argument(
        "--include-id",
        type=int,
        action="append",
        default=None,
        help="Compile only this RA achievement ID; may be repeated",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Fail if any selected achievement is unsupported",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if not GAME_ID_RE.fullmatch(args.game_id):
        print(f"error: invalid PSP game ID: {args.game_id}", file=sys.stderr)
        return 2

    try:
        source_items = read_jsonl(args.input)
        include_ids = set(args.include_id) if args.include_id else None
        compiled, skipped = compile_achievements(source_items, include_ids)

        if not compiled:
            raise CompileError("no supported achievements were selected")
        if args.strict and skipped:
            raise CompileError(f"{len(skipped)} selected achievements are unsupported")

        package_data, report = create_package(compiled, args.game_id)

        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(package_data)

        report.update({
            "source_count": len(source_items),
            "compiled_count": len(compiled),
            "skipped_count": len(skipped),
            "skipped": skipped,
        })

        report_path = args.output.with_suffix(args.output.suffix + ".report.json")
        report_path.write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    except (OSError, CompileError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"PACH v{FORMAT_VERSION}: {args.output}")
    print(f"Package ID: {report['package_id']}")
    print(f"Compiled: {len(compiled)}")
    print(f"Skipped: {len(skipped)}")
    print(f"Groups: {report['group_count']}")
    print(f"Conditions: {report['condition_count']}")
    print(f"Size: {len(package_data)} bytes")
    print(f"Report: {report_path}")

    if skipped:
        print("Unsupported achievements were listed in the report and were not approximated.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
