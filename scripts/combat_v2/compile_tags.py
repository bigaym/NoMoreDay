#!/usr/bin/env python3
"""Validate and compile combat_v2 tag data into runtime mapping JSON."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any


def _load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict):
        raise ValueError("Top-level JSON must be an object")
    return payload


def _normalize_tags(payload: dict[str, Any]) -> list[dict[str, Any]]:
    tags = payload.get("tags")
    if not isinstance(tags, list):
        raise ValueError("Field 'tags' must be an array")

    normalized: list[dict[str, Any]] = []
    seen_ids: set[int] = set()
    seen_names: set[str] = set()
    for entry in tags:
        if not isinstance(entry, dict):
            raise ValueError("Each tag entry must be an object")

        raw_id = entry.get("id")
        raw_name = entry.get("name")
        if isinstance(raw_id, bool) or not isinstance(raw_id, int):
            raise ValueError("Each tag must include integer 'id'")
        if raw_id < 0 or raw_id > 255:
            raise ValueError(f"Tag id {raw_id} out of supported range [0, 255]")
        if not isinstance(raw_name, str) or not raw_name:
            raise ValueError("Each tag must include non-empty string 'name'")

        if raw_id in seen_ids:
            raise ValueError(f"Duplicate tag id: {raw_id}")
        if raw_name in seen_names:
            raise ValueError(f"Duplicate tag name: {raw_name}")

        seen_ids.add(raw_id)
        seen_names.add(raw_name)
        normalized.append({"id": raw_id, "name": raw_name})

    return sorted(normalized, key=lambda item: (item["id"], item["name"]))


def _normalize_aliases(payload: dict[str, Any], tag_names: set[str]) -> dict[str, str]:
    aliases = payload.get("aliases", {})
    if not isinstance(aliases, dict):
        raise ValueError("Field 'aliases' must be an object")

    normalized: dict[str, str] = {}
    for alias, canonical in aliases.items():
        if not isinstance(alias, str) or not alias:
            raise ValueError("Alias key must be a non-empty string")
        if not isinstance(canonical, str) or not canonical:
            raise ValueError("Alias value must be a non-empty canonical tag name")
        if canonical not in tag_names:
            raise ValueError(f"Alias target '{canonical}' does not exist")
        if alias in tag_names:
            raise ValueError(f"Alias '{alias}' collides with canonical tag name")
        if alias in normalized:
            raise ValueError(f"Duplicate alias name: {alias}")
        normalized[alias] = canonical

    return dict(sorted(normalized.items(), key=lambda item: item[0]))


def compile_tags(input_path: Path, output_path: Path) -> None:
    payload = _load_json(input_path)

    tags = _normalize_tags(payload)
    tag_names = {entry["name"] for entry in tags}
    aliases = _normalize_aliases(payload, tag_names)
    name_to_id = {entry["name"]: entry["id"] for entry in tags}

    runtime_name_to_id = dict(sorted(name_to_id.items(), key=lambda item: item[0]))
    for alias, canonical in aliases.items():
        runtime_name_to_id[alias] = name_to_id[canonical]
    runtime_name_to_id = dict(
        sorted(runtime_name_to_id.items(), key=lambda item: item[0])
    )

    runtime_payload = {
        "version": 2,
        "canonical_tags": tags,
        "aliases": aliases,
        "name_to_id": runtime_name_to_id,
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(runtime_payload, handle, indent=2, sort_keys=False)
        handle.write("\n")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("assets/data/combat_v2/tags.json"),
        help="Input tags source JSON",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("assets/data/combat_v2/tags.runtime.json"),
        help="Output compiled runtime JSON",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    compile_tags(args.input, args.output)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(2)
