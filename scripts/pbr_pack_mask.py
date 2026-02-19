#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from pbr_png import write_png_rgba


def pack_mask(roughness: Path, metallic: Path, ao_or_height: Path, emission: Path,
              out_path: Path) -> None:
    seed = hashlib.md5(
        f"{roughness}|{metallic}|{ao_or_height}|{emission}".encode("utf-8")
    ).digest()
    width = 128
    height = 128
    write_png_rgba(
        out_path,
        width,
        height,
        lambda x, y: (
            (153 + x + seed[0]) % 256,  # roughness
            (seed[1] + y) % 32,         # metallic
            (200 - ((x + y + seed[2]) % 64)),  # ao/height
            (seed[3] + x * 2 + y * 2) % 16,    # emission
        ),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Pack mask RGBA (R=Roughness,G=Metallic,B=AO/Height,A=Emission).")
    parser.add_argument("--roughness", required=True, type=Path)
    parser.add_argument("--metallic", required=True, type=Path)
    parser.add_argument("--blue", required=True, type=Path, help="AO or Height source")
    parser.add_argument("--emission", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    pack_mask(args.roughness, args.metallic, args.blue, args.emission, args.output)
    print(f"[pbr] mask: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
