#!/usr/bin/env python3
"""Generate a charset spec for msdf-atlas-gen: GB2312 + ASCII printable."""

from __future__ import annotations

from pathlib import Path


def merge_ranges(codepoints: list[int]) -> list[tuple[int, int]]:
    if not codepoints:
        return []
    ranges: list[tuple[int, int]] = []
    start = codepoints[0]
    prev = codepoints[0]
    for cp in codepoints[1:]:
        if cp == prev + 1:
            prev = cp
            continue
        ranges.append((start, prev))
        start = cp
        prev = cp
    ranges.append((start, prev))
    return ranges


def generate_gb2312_codepoints() -> set[int]:
    codepoints: set[int] = set()
    for lead in range(0xA1, 0xF8):
        for trail in range(0xA1, 0xFF):
            raw = bytes([lead, trail])
            try:
                ch = raw.decode("gb2312")
            except UnicodeDecodeError:
                continue
            codepoints.add(ord(ch))
    return codepoints


def main() -> int:
    codepoints = set(range(0x20, 0x7F))
    codepoints.update(generate_gb2312_codepoints())

    ordered = sorted(codepoints)
    ranges = merge_ranges(ordered)
    entries: list[str] = []
    for lo, hi in ranges:
        if lo == hi:
            entries.append(f"0x{lo:X}")
        else:
            entries.append(f"[0x{lo:X}, 0x{hi:X}]")

    output = Path("scripts/msdf_charset_gb2312.txt")
    output.write_text(",\n".join(entries) + "\n", encoding="utf-8")
    print(
        f"generated: {output} (codepoints={len(ordered)}, ranges={len(ranges)})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
