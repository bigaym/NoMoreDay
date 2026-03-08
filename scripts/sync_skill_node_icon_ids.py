"""
Sync talent node icon_id values in skill tree JSON files using FNV1a-32 hashes.

Rule:
  icon key = "skill_node_<node_id>"
  icon hash = FNV1a-32(icon key)

Icon discovery:
  assets/textures/skill_nodes/skill_nodes_<node_id>.png
  assets/textures/skill_nodes/skill_nodes_<node_id>_*.png

Usage:
  python scripts/sync_skill_node_icon_ids.py
  python scripts/sync_skill_node_icon_ids.py --check
  python scripts/sync_skill_node_icon_ids.py --json assets/data/mastery_skill_trees.json
"""

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SKILLS_JSON = ROOT / "assets" / "data" / "skills.json"
DEFAULT_MASTERY_SKILL_TREES_JSON = ROOT / "assets" / "data" / "mastery_skill_trees.json"
DEFAULT_JSON_FILES = (
    DEFAULT_SKILLS_JSON,
    DEFAULT_MASTERY_SKILL_TREES_JSON,
)
DEFAULT_ICON_DIR = ROOT / "assets" / "textures" / "skill_nodes"


def fnv1a_32(text: str) -> int:
    value = 0x811C9DC5
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * 0x01000193) & 0xFFFFFFFF
    return value


def collect_icon_hashes(icon_dir: Path) -> dict[int, int]:
    pattern = re.compile(r"^skill_nodes_(\d+)(?:_.*)?\.png$", re.IGNORECASE)
    mappings: dict[int, int] = {}
    duplicate_ids = set()

    for file in sorted(icon_dir.glob("skill_nodes_*.png")):
        match = pattern.match(file.name)
        if not match:
            continue
        node_id = int(match.group(1))
        if node_id in mappings:
            duplicate_ids.add(node_id)
            continue
        mappings[node_id] = fnv1a_32(f"skill_node_{node_id}")

    if duplicate_ids:
        ids = ", ".join(str(v) for v in sorted(duplicate_ids))
        print(
            f"Warning: duplicate icon files found for node IDs (keeping first): {ids}"
        )

    return mappings


def sync_icon_ids(
    json_path: Path, icon_hashes: dict[int, int], check_only: bool
) -> int:
    data = json.loads(json_path.read_text(encoding="utf-8"))
    changed = 0
    unchanged = 0
    missing_icon = 0
    total_nodes = 0

    for skill in data.get("skills", []):
        for node in skill.get("talent_tree", []):
            total_nodes += 1
            node_id = int(node.get("id", 0))
            if node_id not in icon_hashes:
                missing_icon += 1
                continue

            expected = icon_hashes[node_id]
            current = int(node.get("icon_id", 0))
            if current != expected:
                changed += 1
                if not check_only:
                    node["icon_id"] = expected
            else:
                unchanged += 1

    print(f"File: {json_path}")
    print(f"Total talent nodes: {total_nodes}")
    print(f"Nodes with icon files: {len(icon_hashes)}")
    print(f"Nodes updated: {changed}")
    print(f"Nodes unchanged: {unchanged}")
    print(f"Nodes missing icon files: {missing_icon}")

    if check_only:
        return 1 if changed > 0 else 0

    json_path.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    return 0


def sync_json_files(
    json_files: tuple[Path, ...] | list[Path],
    icon_hashes: dict[int, int],
    check_only: bool,
) -> int:
    status = 0
    for json_path in json_files:
        if not json_path.exists():
            raise FileNotFoundError(f"JSON file not found: {json_path}")
        status = max(status, sync_icon_ids(json_path, icon_hashes, check_only))
    return status


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Sync node icon_id hashes in skill tree JSON files from skill_nodes files."
    )
    parser.add_argument(
        "--json",
        dest="json_files",
        type=Path,
        nargs="+",
        default=list(DEFAULT_JSON_FILES),
        help="One or more JSON files to sync.",
    )
    parser.add_argument(
        "--icon-dir",
        type=Path,
        default=DEFAULT_ICON_DIR,
        help="Path to skill node icon directory",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check only, do not modify files (exit code 1 if changes needed).",
    )
    args = parser.parse_args()

    if not args.icon_dir.exists():
        raise FileNotFoundError(f"icon directory not found: {args.icon_dir}")

    icon_hashes = collect_icon_hashes(args.icon_dir)
    return sync_json_files(args.json_files, icon_hashes, args.check)


if __name__ == "__main__":
    raise SystemExit(main())
