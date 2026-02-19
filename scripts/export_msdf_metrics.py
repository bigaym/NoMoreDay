#!/usr/bin/env python3
"""Export compact glyph metrics from msdf-atlas-gen JSON.

Input:  msdf-atlas-gen JSON (with glyph.unicode/advance/planeBounds/atlasBounds)
Output: compact JSON + binary table for runtime loading.
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


MAGIC = b"MSGM"
VERSION = 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export compact MSDF metrics JSON/BIN.")
    parser.add_argument("--input", required=True, help="Input msdf-atlas-gen JSON path.")
    parser.add_argument("--output-json", default=None, help="Output compact JSON path.")
    parser.add_argument("--output-bin", default=None, help="Output compact BIN path.")
    return parser.parse_args()


def glyph_to_record(glyph: dict, atlas_w: float, atlas_h: float) -> tuple[int, tuple[float, ...]]:
    cp = int(glyph["unicode"])
    pb = glyph["planeBounds"]
    ab = glyph["atlasBounds"]

    u0 = float(ab["left"]) / atlas_w
    v0 = float(ab["bottom"]) / atlas_h
    u1 = float(ab["right"]) / atlas_w
    v1 = float(ab["top"]) / atlas_h

    bearing_x = float(pb["left"])
    bearing_y = float(pb["bottom"])
    size_x = float(pb["right"]) - float(pb["left"])
    size_y = float(pb["top"]) - float(pb["bottom"])
    advance = float(glyph["advance"])

    payload = (
        u0,
        v0,
        u1,
        v1,
        bearing_x,
        bearing_y,
        size_x,
        size_y,
        advance,
        0.0,
    )
    return cp, payload


def main() -> int:
    args = parse_args()
    in_path = Path(args.input).resolve()
    data = json.loads(in_path.read_text(encoding="utf-8"))

    atlas = data.get("atlas", {})
    atlas_w = float(atlas.get("width", 1))
    atlas_h = float(atlas.get("height", 1))
    distance_range = float(atlas.get("distanceRange", 0.0))

    glyphs = data.get("glyphs", [])
    records: list[tuple[int, tuple[float, ...]]] = []
    for glyph in glyphs:
        if "unicode" not in glyph or "planeBounds" not in glyph or "atlasBounds" not in glyph:
            continue
        records.append(glyph_to_record(glyph, atlas_w, atlas_h))

    records.sort(key=lambda item: item[0])

    base = in_path.with_suffix("")
    out_json = Path(args.output_json).resolve() if args.output_json else base.with_suffix(".metrics.json")
    out_bin = Path(args.output_bin).resolve() if args.output_bin else base.with_suffix(".metrics.bin")
    out_json.parent.mkdir(parents=True, exist_ok=True)
    out_bin.parent.mkdir(parents=True, exist_ok=True)

    compact = {
        "version": VERSION,
        "atlas": {
            "width": int(atlas_w),
            "height": int(atlas_h),
            "distanceRange": distance_range,
        },
        "glyphCount": len(records),
        "glyphs": [
            {
                "cp": cp,
                "uvRect": [p[0], p[1], p[2], p[3]],
                "bearing": [p[4], p[5]],
                "size": [p[6], p[7]],
                "advance": p[8],
            }
            for cp, p in records
        ],
    }
    out_json.write_text(json.dumps(compact, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")

    with out_bin.open("wb") as f:
        header = struct.pack("<4sIIIff", MAGIC, VERSION, len(records), 44, atlas_w, atlas_h)
        f.write(header)
        f.write(struct.pack("<f", distance_range))
        for cp, payload in records:
            f.write(struct.pack("<I10f", cp, *payload))

    print(f"input_glyphs={len(glyphs)} exported={len(records)}")
    print(f"json={out_json}")
    print(f"bin={out_bin}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
