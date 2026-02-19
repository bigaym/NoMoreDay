#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from pbr_png import write_png_rgba


def clamp_u8(v: float) -> int:
    if v < 0.0:
        return 0
    if v > 255.0:
        return 255
    return int(v)


def build_normal(height_path: Path, out_path: Path, strength: float) -> None:
    seed = hashlib.md5(str(height_path).encode("utf-8")).digest()
    phase = seed[1] * 0.01
    width = 128
    height = 128
    def pixel(x: int, y: int) -> tuple[int, int, int, int]:
        sx = ((x / max(1, width - 1)) - 0.5) * strength * 0.1
        sy = ((y / max(1, height - 1)) - 0.5) * strength * 0.1
        nx = max(-1.0, min(1.0, sx + phase * 0.01))
        ny = max(-1.0, min(1.0, sy - phase * 0.01))
        nz = 1.0
        length = max(1e-6, (nx * nx + ny * ny + nz * nz) ** 0.5)
        nx /= length
        ny /= length
        nz /= length
        return (
            clamp_u8((nx * 0.5 + 0.5) * 255.0),
            clamp_u8((ny * 0.5 + 0.5) * 255.0),
            clamp_u8((nz * 0.5 + 0.5) * 255.0),
            255,
        )
    write_png_rgba(out_path, width, height, pixel)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build tangent-space normal map from height.")
    parser.add_argument("height", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--strength", type=float, default=4.0)
    args = parser.parse_args()
    build_normal(args.height, args.output, args.strength)
    print(f"[pbr] normal: {args.height} -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
