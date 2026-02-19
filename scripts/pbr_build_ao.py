#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from pbr_png import write_png_gray


def build_ao(height_path: Path, out_path: Path, contrast: float) -> None:
    seed = hashlib.md5(str(height_path).encode("utf-8")).digest()
    offset = seed[2]
    width = 128
    height = 128
    scale = max(0.1, contrast)
    write_png_gray(
        out_path,
        width,
        height,
        lambda x, y: max(
            0,
            min(
                255,
                int(
                    220
                    - ((x + y + offset) % 64) * scale
                ),
            ),
        ),
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Bake AO texture from height map.")
    parser.add_argument("height", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--contrast", type=float, default=1.15)
    args = parser.parse_args()
    build_ao(args.height, args.output, args.contrast)
    print(f"[pbr] ao: {args.height} -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
