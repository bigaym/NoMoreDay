#!/usr/bin/env python3
"""Generate MSDF atlas assets via msdf-atlas-gen (offline pipeline entry).

This script integrates Track v4_gpu_text_rendering Task 1.1 by providing
an explicit, reproducible command wrapper for msdf-atlas-gen.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def resolve_executable(explicit: str | None) -> Path:
    if explicit:
        exe = Path(explicit).expanduser().resolve()
        if exe.is_file():
            return exe
        raise FileNotFoundError(f"msdf-atlas-gen not found: {exe}")

    env_path = os.environ.get("MSDF_ATLAS_GEN", "").strip()
    if env_path:
        exe = Path(env_path).expanduser().resolve()
        if exe.is_file():
            return exe
        raise FileNotFoundError(f"MSDF_ATLAS_GEN points to missing file: {exe}")

    which = shutil.which("msdf-atlas-gen")
    if which:
        return Path(which).resolve()

    raise FileNotFoundError(
        "msdf-atlas-gen not found. Set --msdf-atlas-gen or MSDF_ATLAS_GEN."
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate MSDF atlas/png+json using msdf-atlas-gen."
    )
    parser.add_argument("--font", required=True, help="Input font file path (ttf/otf/ttc).")
    parser.add_argument(
        "--charset",
        default="scripts/msdf_charset_v4.txt",
        help="Charset text file path (UTF-8).",
    )
    parser.add_argument(
        "--output-dir",
        default="assets/textures/fonts/msdf",
        help="Output directory for atlas + metrics json.",
    )
    parser.add_argument("--name", default="v4_popup_msdf_4096", help="Output base name.")
    parser.add_argument("--width", type=int, default=4096, help="Atlas width.")
    parser.add_argument("--height", type=int, default=4096, help="Atlas height.")
    parser.add_argument("--pxrange", type=float, default=6.0, help="MSDF pixel range.")
    parser.add_argument(
        "--allglyphs",
        action="store_true",
        help="Use all glyphs in font (recommended for CJK superset fonts).",
    )
    parser.add_argument("--size", type=float, default=None, help="Fixed glyph em size in atlas.")
    parser.add_argument(
        "--minsize",
        type=float,
        default=18.0,
        help="Minimum glyph em size (engine will auto-fit as large as possible).",
    )
    parser.add_argument(
        "--msdf-atlas-gen",
        default=None,
        help="Path to msdf-atlas-gen executable (optional).",
    )
    parser.add_argument(
        "--run",
        action="store_true",
        help="Execute command. If omitted, prints command only (dry run).",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    font_path = Path(args.font).expanduser().resolve()
    charset_path = Path(args.charset).expanduser().resolve()
    out_dir = Path(args.output_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not font_path.is_file():
        print(f"[error] font not found: {font_path}", file=sys.stderr)
        return 2
    if not args.allglyphs and not charset_path.is_file():
        print(f"[error] charset file not found: {charset_path}", file=sys.stderr)
        return 2

    png_path = out_dir / f"{args.name}.png"
    json_path = out_dir / f"{args.name}.json"

    try:
        exe = resolve_executable(args.msdf_atlas_gen)
    except FileNotFoundError as exc:
        print(f"[error] {exc}", file=sys.stderr)
        return 3

    cmd = [str(exe), "-font", str(font_path)]
    if args.allglyphs:
        cmd.append("-allglyphs")
    else:
        cmd.extend(["-charset", str(charset_path)])
    cmd.extend(
        [
            "-type",
            "msdf",
            "-dimensions",
            str(args.width),
            str(args.height),
            "-pxrange",
            str(args.pxrange),
            "-json",
            str(json_path),
            "-imageout",
            str(png_path),
            "-coloringstrategy",
            "distance",
            "-potr",
        ]
    )

    if args.size is not None:
        cmd.extend(["-size", str(args.size)])
    else:
        cmd.extend(["-minsize", str(args.minsize)])

    print("[msdf] command:")
    print(" ".join(f'"{part}"' if " " in part else part for part in cmd))

    if not args.run:
        print("[msdf] dry-run mode. Use --run to execute.")
        return 0

    result = subprocess.run(cmd, check=False, text=True)
    if result.returncode != 0:
        print(f"[error] msdf-atlas-gen failed with exit code {result.returncode}", file=sys.stderr)
        return result.returncode

    print(f"[ok] generated atlas: {png_path}")
    print(f"[ok] generated metrics: {json_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
