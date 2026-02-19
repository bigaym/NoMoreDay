#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def run(args: list[str]) -> None:
    result = subprocess.run(args, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"command failed: {' '.join(args)}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run V4 PBR offline toolchain for one sprite.")
    parser.add_argument("--albedo", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--name", required=True)
    args = parser.parse_args()

    out_dir = args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    height = out_dir / f"{args.name}_height.png"
    normal = out_dir / f"{args.name}_normal.png"
    ao = out_dir / f"{args.name}_ao.png"
    roughness = out_dir / f"{args.name}_roughness.png"
    metallic = out_dir / f"{args.name}_metallic.png"
    emission = out_dir / f"{args.name}_emission.png"
    mask = out_dir / f"{args.name}_mask.png"

    run([sys.executable, "scripts/pbr_build_height.py", str(args.albedo), str(height)])
    run([sys.executable, "scripts/pbr_build_normal.py", str(height), str(normal)])
    run([sys.executable, "scripts/pbr_build_ao.py", str(height), str(ao)])

    # Reuse AO as roughness baseline; keep metallic/emission neutral defaults.
    shutil.copyfile(ao, roughness)
    metallic.write_bytes(b"")
    emission.write_bytes(b"")

    run(
        [
            sys.executable,
            "scripts/pbr_pack_mask.py",
            "--roughness",
            str(roughness),
            "--metallic",
            str(metallic),
            "--blue",
            str(ao),
            "--emission",
            str(emission),
            "--output",
            str(mask),
        ]
    )
    print(f"[pbr] ci pipeline done: {args.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
