#!/usr/bin/env python3
"""
NoMoreDay Procedural VFX Texture Generator
Usage: python scripts/gen_blade_vfx_assets.py
"""

from __future__ import annotations

import math
import random
import struct
import zlib
from pathlib import Path

import numpy as np


SEED = 20260222
RNG = np.random.default_rng(SEED)
random.seed(SEED)


def clamp_u8(value: float) -> int:
    return int(max(0.0, min(255.0, value)))


def blend_pixel(data: np.ndarray, x: int, y: int, color: tuple[int, int, int, int]) -> None:
    h, w = data.shape[0], data.shape[1]
    if x < 0 or y < 0 or x >= w or y >= h:
        return
    src_a = color[3] / 255.0
    dst = data[y, x]
    dst_a = dst[3] / 255.0
    out_a = src_a + dst_a * (1.0 - src_a)
    if out_a <= 1e-6:
        data[y, x] = [0, 0, 0, 0]
        return
    out_rgb = [
        clamp_u8((color[c] * src_a + dst[c] * dst_a * (1.0 - src_a)) / out_a)
        for c in range(3)
    ]
    data[y, x, 0] = out_rgb[0]
    data[y, x, 1] = out_rgb[1]
    data[y, x, 2] = out_rgb[2]
    data[y, x, 3] = clamp_u8(out_a * 255.0)


def draw_line(
    data: np.ndarray,
    x0: float,
    y0: float,
    x1: float,
    y1: float,
    color: tuple[int, int, int, int],
    width: int = 1,
) -> None:
    dx = x1 - x0
    dy = y1 - y0
    steps = int(max(abs(dx), abs(dy), 1.0))
    for i in range(steps + 1):
        t = i / steps
        x = x0 + dx * t
        y = y0 + dy * t
        half = max(0, width // 2)
        for oy in range(-half, half + 1):
            for ox in range(-half, half + 1):
                blend_pixel(data, int(round(x + ox)), int(round(y + oy)), color)


def save_rgba(data: np.ndarray, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if data.dtype != np.uint8:
        data = data.astype(np.uint8)
    h, w, c = data.shape
    if c != 4:
        raise ValueError("RGBA image expected")

    def chunk(tag: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + tag
            + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
        )

    raw_rows = b"".join(b"\x00" + data[y].tobytes() for y in range(h))
    compressed = zlib.compress(raw_rows, level=9)
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)

    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", compressed)
        + chunk(b"IEND", b"")
    )
    path.write_bytes(png)
    print(f"Generated {path.as_posix()}")


def gen_trail_smooth(path: Path, size: tuple[int, int] = (256, 64)) -> None:
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    for x in range(size[0]):
        alpha_x = pow(x / max(1, size[0] - 1), 0.55)
        for y in range(size[1]):
            ny = y / max(1, size[1] - 1)
            alpha_y = pow(math.sin(ny * math.pi), 0.5)
            a = clamp_u8(255.0 * alpha_x * alpha_y)
            data[y, x] = [255, 255, 255, a]
    save_rgba(data, path)


def gen_noise_cloud(path: Path, size: tuple[int, int] = (256, 256)) -> None:
    noise = RNG.integers(0, 256, size, dtype=np.uint8).astype(np.float32)
    # Deterministic coarse-to-fine blur substitute.
    small = noise.reshape(32, size[1] // 32, 32, size[0] // 32).mean(axis=(1, 3))
    expanded = np.kron(small, np.ones((size[1] // 32, size[0] // 32), dtype=np.float32))
    expanded = np.clip(expanded, 0, 255).astype(np.uint8)
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    data[..., 0] = expanded
    data[..., 1] = expanded
    data[..., 2] = expanded
    data[..., 3] = expanded
    save_rgba(data, path)


def gen_circle_shockwave(path: Path, size: tuple[int, int] = (256, 256)) -> None:
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    cx, cy = size[0] * 0.5, size[1] * 0.5
    max_r = min(cx, cy)
    mu = 0.8
    sigma = 0.1
    for y in range(size[1]):
        for x in range(size[0]):
            dx = x - cx
            dy = y - cy
            norm = math.sqrt(dx * dx + dy * dy) / max(1e-5, max_r)
            if norm > 1.0:
                continue
            ring = math.exp(-((norm - mu) ** 2) / (2.0 * sigma * sigma))
            a = clamp_u8(ring * 255.0)
            data[y, x] = [255, 255, 255, a]
    save_rgba(data, path)


def gen_scratch_mask(path: Path, size: tuple[int, int] = (256, 256)) -> None:
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    for _ in range(56):
        y = random.randint(0, size[1] - 1)
        length = random.randint(20, 110)
        x_start = random.randint(0, max(0, size[0] - length - 1))
        alpha = random.randint(90, 255)
        draw_line(data, x_start, y, x_start + length, y, (255, 255, 255, alpha), width=1)
    save_rgba(data, path)


def gen_rune_array(path: Path, size: tuple[int, int] = (256, 256)) -> None:
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    cx, cy = size[0] * 0.5, size[1] * 0.5
    outer = size[0] * 0.5 - 10.0
    inner = size[0] * 0.5 - 50.0
    for y in range(size[1]):
        for x in range(size[0]):
            d = math.sqrt((x - cx) ** 2 + (y - cy) ** 2)
            if abs(d - outer) <= 1.5:
                data[y, x] = [0, 255, 255, 255]
            elif abs(d - inner) <= 1.0:
                data[y, x] = [0, 255, 255, 128]
    pts = []
    radius = size[0] * 0.5 - 20
    for i in range(3):
        angle = -math.pi * 0.5 + i * (2.0 * math.pi / 3.0)
        pts.append((cx + radius * math.cos(angle), cy + radius * math.sin(angle)))
    draw_line(data, pts[0][0], pts[0][1], pts[1][0], pts[1][1], (0, 255, 255, 200), 2)
    draw_line(data, pts[1][0], pts[1][1], pts[2][0], pts[2][1], (0, 255, 255, 200), 2)
    draw_line(data, pts[2][0], pts[2][1], pts[0][0], pts[0][1], (0, 255, 255, 200), 2)
    save_rgba(data, path)


def gen_element_texture(path: Path, core_rgb: tuple[int, int, int], glow_rgb: tuple[int, int, int]) -> None:
    size = (256, 256)
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    cx, cy = size[0] * 0.5, size[1] * 0.5
    max_r = min(cx, cy)
    for y in range(size[1]):
        for x in range(size[0]):
            dx = x - cx
            dy = y - cy
            dist = math.sqrt(dx * dx + dy * dy) / max_r
            if dist > 1.0:
                continue
            wave = 0.5 + 0.5 * math.sin((dx + dy) * 0.08)
            radial = pow(max(0.0, 1.0 - dist), 1.6)
            alpha = clamp_u8((0.35 + 0.65 * wave) * radial * 255.0)
            mix_t = min(1.0, dist * 1.1)
            r = clamp_u8(core_rgb[0] * (1.0 - mix_t) + glow_rgb[0] * mix_t)
            g = clamp_u8(core_rgb[1] * (1.0 - mix_t) + glow_rgb[1] * mix_t)
            b = clamp_u8(core_rgb[2] * (1.0 - mix_t) + glow_rgb[2] * mix_t)
            data[y, x] = [r, g, b, alpha]
    save_rgba(data, path)


def gen_resist_crack(path: Path, size: tuple[int, int] = (256, 256)) -> None:
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    cx, cy = size[0] * 0.5, size[1] * 0.5
    for _ in range(20):
        angle = RNG.uniform(0.0, math.pi * 2.0)
        length = RNG.uniform(60.0, 120.0)
        branch = RNG.uniform(-0.35, 0.35)
        x1 = cx + math.cos(angle) * length
        y1 = cy + math.sin(angle) * length
        x2 = x1 + math.cos(angle + branch) * RNG.uniform(12.0, 36.0)
        y2 = y1 + math.sin(angle + branch) * RNG.uniform(12.0, 36.0)
        alpha = int(RNG.integers(150, 245))
        draw_line(data, cx, cy, x1, y1, (255, 255, 255, alpha), width=2)
        draw_line(data, x1, y1, x2, y2, (255, 255, 255, max(0, alpha - 30)), width=1)
    save_rgba(data, path)


def gen_frost_spread(path: Path, size: tuple[int, int] = (256, 256)) -> None:
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    cx, cy = size[0] * 0.5, size[1] * 0.5
    max_r = min(cx, cy)
    for y in range(size[1]):
        for x in range(size[0]):
            dx = x - cx
            dy = y - cy
            dist = math.sqrt(dx * dx + dy * dy) / max_r
            if dist > 1.0:
                continue
            angle = math.atan2(dy, dx)
            spokes = 0.5 + 0.5 * math.cos(angle * 6.0)
            alpha = clamp_u8(pow(max(0.0, 1.0 - dist), 1.4) * (0.25 + 0.75 * spokes) * 255.0)
            data[y, x] = [170, 225, 255, alpha]
    save_rgba(data, path)


def gen_ember_trail(path: Path, size: tuple[int, int] = (256, 128)) -> None:
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    for y in range(size[1]):
        ny = y / max(1, size[1] - 1)
        cross = pow(math.sin(ny * math.pi), 0.7)
        for x in range(size[0]):
            nx = x / max(1, size[0] - 1)
            flame = pow(max(0.0, 1.0 - nx), 0.45)
            jitter = 0.7 + 0.3 * RNG.random()
            alpha = clamp_u8(cross * flame * jitter * 255.0)
            r = 255
            g = clamp_u8(120.0 + 90.0 * (1.0 - nx))
            b = clamp_u8(32.0 + 25.0 * (1.0 - nx))
            data[y, x] = [r, g, b, alpha]
    save_rgba(data, path)


def gen_electric_arc(path: Path, size: tuple[int, int] = (256, 256)) -> None:
    data = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    points: list[tuple[float, float]] = []
    x = 16.0
    y = size[1] * 0.5
    while x < size[0] - 16:
        points.append((x, y))
        x += RNG.uniform(14.0, 26.0)
        y += RNG.uniform(-28.0, 28.0)
        y = max(20.0, min(size[1] - 20.0, y))
    points.append((size[0] - 16.0, size[1] * 0.5))
    for i in range(len(points) - 1):
        draw_line(
            data,
            points[i][0],
            points[i][1],
            points[i + 1][0],
            points[i + 1][1],
            (225, 190, 255, 230),
            width=3,
        )
    for p in points[1:-1]:
        bx = p[0] + RNG.uniform(-18.0, 18.0)
        by = p[1] + RNG.uniform(-18.0, 18.0)
        draw_line(data, p[0], p[1], bx, by, (255, 245, 255, 175), width=1)
    save_rgba(data, path)


def main() -> int:
    vfx_dir = Path("assets/textures/vfx")
    vfx_dir.mkdir(parents=True, exist_ok=True)

    gen_trail_smooth(vfx_dir / "vfx_trail_smooth.png")
    gen_noise_cloud(vfx_dir / "vfx_noise_cloud.png")
    gen_circle_shockwave(vfx_dir / "vfx_circle_shockwave.png")
    gen_scratch_mask(vfx_dir / "vfx_scratch_mask.png")
    gen_rune_array(vfx_dir / "vfx_rune_array.png")

    # Element/debuff placeholders for transmutation track.
    gen_element_texture(vfx_dir / "vfx_element_fire.png", (255, 107, 53), (255, 200, 120))
    gen_element_texture(vfx_dir / "vfx_element_ice.png", (126, 200, 227), (212, 241, 249))
    gen_element_texture(vfx_dir / "vfx_element_lightning.png", (199, 125, 255), (224, 170, 255))
    gen_element_texture(vfx_dir / "vfx_element_void.png", (92, 0, 153), (157, 78, 221))
    gen_resist_crack(vfx_dir / "vfx_resist_crack.png")
    gen_frost_spread(vfx_dir / "vfx_frost_spread.png")
    gen_ember_trail(vfx_dir / "vfx_ember_trail.png")
    gen_electric_arc(vfx_dir / "vfx_electric_arc.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
