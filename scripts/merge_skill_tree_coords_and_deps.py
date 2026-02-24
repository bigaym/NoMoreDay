#!/usr/bin/env python3
import argparse
import glob
import json
from pathlib import Path
from typing import Dict, List, Tuple


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def dump_json(path: Path, data: dict) -> None:
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")


def build_skill_map(skills_data: dict) -> Dict[int, dict]:
    return {int(skill["id"]): skill for skill in skills_data.get("skills", [])}


def normalize_prerequisites(node: dict) -> List[dict]:
    prerequisites = node.get("prerequisites", [])
    if isinstance(prerequisites, list):
        return prerequisites
    return []


def merge_tree_into_skills(skills_json: dict, tree_json: dict) -> Tuple[int, int, int]:
    updated_skills = 0
    updated_nodes = 0
    missing_nodes = 0

    skills_by_id = build_skill_map(skills_json)
    tree_skills = tree_json.get("skills", [])

    for tree_skill in tree_skills:
        skill_id = int(tree_skill.get("id", -1))
        target_skill = skills_by_id.get(skill_id)
        if not target_skill:
            continue

        target_nodes = target_skill.get("talent_tree", [])
        target_nodes_by_id = {int(node["id"]): node for node in target_nodes if "id" in node}

        changed_in_skill = False
        for tree_node in tree_skill.get("talent_tree", []):
            node_id = int(tree_node.get("id", -1))
            target_node = target_nodes_by_id.get(node_id)
            if not target_node:
                missing_nodes += 1
                continue

            new_x = tree_node.get("x", target_node.get("x"))
            new_y = tree_node.get("y", target_node.get("y"))
            new_prereqs = normalize_prerequisites(tree_node)

            changed = False
            if target_node.get("x") != new_x:
                target_node["x"] = new_x
                changed = True
            if target_node.get("y") != new_y:
                target_node["y"] = new_y
                changed = True
            if target_node.get("prerequisites") != new_prereqs:
                target_node["prerequisites"] = new_prereqs
                changed = True

            if changed:
                updated_nodes += 1
                changed_in_skill = True

        if changed_in_skill:
            updated_skills += 1

    return updated_skills, updated_nodes, missing_nodes


def resolve_tree_files(tree_path: Path, tree_glob: str) -> List[Path]:
    if tree_path.exists():
        return [tree_path]
    matched = [Path(p) for p in sorted(glob.glob(tree_glob))]
    return matched


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Merge node prerequisites and coordinates from skill_x_tree.json into skills.json."
    )
    parser.add_argument(
        "--skills",
        type=Path,
        default=Path("assets/data/skills.json"),
        help="Target skills.json path.",
    )
    parser.add_argument(
        "--tree",
        type=Path,
        default=Path("assets/data/skill_x_tree.json"),
        help="Single tree file path. If missing, --tree-glob is used.",
    )
    parser.add_argument(
        "--tree-glob",
        default="assets/data/skill_*_tree.json",
        help="Glob pattern used when --tree file does not exist.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show merge summary without writing changes.",
    )
    args = parser.parse_args()

    if not args.skills.exists():
        raise FileNotFoundError(f"skills.json not found: {args.skills}")

    tree_files = resolve_tree_files(args.tree, args.tree_glob)
    if not tree_files:
        raise FileNotFoundError(
            f"No tree files found. Missing: {args.tree}; glob: {args.tree_glob}"
        )

    skills_json = load_json(args.skills)

    total_updated_skills = 0
    total_updated_nodes = 0
    total_missing_nodes = 0
    for tree_file in tree_files:
        tree_json = load_json(tree_file)
        us, un, mn = merge_tree_into_skills(skills_json, tree_json)
        total_updated_skills += us
        total_updated_nodes += un
        total_missing_nodes += mn
        print(f"[merge] {tree_file} -> updated_skills={us}, updated_nodes={un}, missing_nodes={mn}")

    print(
        "[summary] files={files}, updated_skills={skills}, updated_nodes={nodes}, missing_nodes={missing}".format(
            files=len(tree_files),
            skills=total_updated_skills,
            nodes=total_updated_nodes,
            missing=total_missing_nodes,
        )
    )

    if args.dry_run:
        print("[dry-run] no file written")
        return 0

    dump_json(args.skills, skills_json)
    print(f"[write] {args.skills}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
