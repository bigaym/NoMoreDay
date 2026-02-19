#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from pbr_png import write_png_rgba


def ensure_default_textures() -> None:
    base = Path("assets/textures/defaults")
    base.mkdir(parents=True, exist_ok=True)
    write_png_rgba(base / "albedo_white.png", 128, 128, lambda _x, _y: (255, 255, 255, 255))
    write_png_rgba(base / "normal_flat.png", 128, 128, lambda _x, _y: (128, 128, 255, 255))
    write_png_rgba(base / "mask_neutral.png", 128, 128, lambda _x, _y: (153, 0, 255, 0))
    write_png_rgba(base / "detail_flat.png", 128, 128, lambda _x, _y: (128, 128, 255, 255))


def make_albedo(path: Path, fill: tuple[int, int, int, int], accent: tuple[int, int, int, int]) -> None:
    def pixel(x: int, y: int) -> tuple[int, int, int, int]:
        if 16 <= x <= 112 and (y == 16 or y == 112):
            return accent
        if 16 <= y <= 112 and (x == 16 or x == 112):
            return accent
        dx = x - 64
        dy = y - 64
        if dx * dx + dy * dy <= 28 * 28:
            return accent
        return fill
    write_png_rgba(path, 128, 128, pixel)


def main() -> int:
    ensure_default_textures()
    base = Path("assets/textures/pbr_v4")
    make_albedo(base / "player/player_albedo.png", (90, 120, 160, 255), (220, 220, 235, 255))
    make_albedo(base / "monster/monster_albedo.png", (120, 70, 70, 255), (200, 120, 90, 255))
    make_albedo(base / "environment/pillar_albedo.png", (95, 95, 100, 255), (150, 150, 160, 255))
    print("[pbr] sample albedo generated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
