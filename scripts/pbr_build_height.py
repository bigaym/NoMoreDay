#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from pbr_png import write_png_gray


def build_height(albedo_path: Path, out_path: Path, blur: float) -> None:
    seed = hashlib.md5(str(albedo_path).encode("utf-8")).digest()
    base = seed[0]
    width = 128
    height = 128
    write_png_gray(
        out_path,
        width,
        height,
        lambda x, y: (base + (x * 3 + y * 5) + int(blur * 10.0)) % 256,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Build grayscale height map from albedo.")
    parser.add_argument("albedo", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--blur", type=float, default=1.0)
    args = parser.parse_args()
    build_height(args.albedo, args.output, args.blur)
    print(f"[pbr] height: {args.albedo} -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
