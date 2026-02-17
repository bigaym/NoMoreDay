#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Iterable


def load_manifest(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def collect_governed_structs(manifest: dict) -> list[str]:
    names: list[str] = []
    for entry in manifest.get("gpu_abi", {}).get("structs", []):
        name = entry.get("name")
        if isinstance(name, str) and name:
            names.append(name)
    material_name = manifest.get("material_abi", {}).get("struct_name")
    if isinstance(material_name, str) and material_name:
        names.append(material_name)
    return sorted(set(names))


def iter_shader_files(root: Path) -> Iterable[Path]:
    exts = {".vert", ".frag", ".comp", ".glsl", ".glslinc"}
    for path in root.rglob("*"):
        if path.suffix.lower() not in exts:
            continue
        if "generated" in path.parts:
            continue
        yield path


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]

    parser = argparse.ArgumentParser(
        description="Fail when governed ABI structs are manually declared in non-generated shader files."
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=repo_root / "tools/render_abi/abi_manifest.json",
    )
    parser.add_argument(
        "--shader-root",
        type=Path,
        default=repo_root / "assets/shaders",
    )
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    governed_names = collect_governed_structs(manifest)
    if not governed_names:
        print("[render_abi] no governed structs found in manifest")
        return 1

    pattern = re.compile(r"\bstruct\s+(" + "|".join(map(re.escape, governed_names)) + r")\b")
    violations: list[tuple[Path, str]] = []
    for shader in iter_shader_files(args.shader_root):
        text = shader.read_text(encoding="utf-8")
        for match in pattern.finditer(text):
            violations.append((shader, match.group(1)))

    if violations:
        print("[render_abi] manual ABI struct declarations detected:")
        for path, struct_name in violations:
            rel_path = path.relative_to(repo_root)
            print(f"  - {rel_path} -> struct {struct_name}")
        return 1

    print("[render_abi] manual ABI struct governance check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
