#!/usr/bin/env python3
"""Validate and compile Combat V2 condition fixtures into a runtime artifact."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT = ROOT / "assets" / "data" / "combat_v2" / "condition_fixtures.json"
DEFAULT_TAGS_RUNTIME = ROOT / "assets" / "data" / "combat_v2" / "tags.runtime.json"
DEFAULT_OUTPUT = (
    ROOT / "assets" / "data" / "combat_v2" / "condition_fixtures.runtime.json"
)

UNARY_OPS = {"not"}
ARRAY_OPS = {"all", "any", "none", "has_tags_all", "has_tags_any"}


@dataclass(frozen=True)
class FixtureResult:
    name: str
    condition: dict[str, Any] | None = None
    error: str | None = None


def _read_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def _normalize_str_list(value: Any, field_name: str) -> list[str]:
    if not isinstance(value, list) or len(value) == 0:
        raise ValueError(f"{field_name} must be a non-empty array")

    normalized: list[str] = []
    for index, entry in enumerate(value):
        if not isinstance(entry, str) or not entry.strip():
            raise ValueError(f"{field_name}[{index}] must be a non-empty string")
        normalized.append(entry.strip())

    return normalized


def _validate_condition(
    condition: Any, known_tags: set[str], path: str = "condition"
) -> dict[str, Any]:
    if not isinstance(condition, dict):
        raise ValueError(f"{path} must be an object")
    if len(condition) != 1:
        raise ValueError(f"{path} must contain exactly one operator")

    op, raw_value = next(iter(condition.items()))
    if not isinstance(op, str):
        raise ValueError(f"{path} operator key must be a string")

    node_path = f"{path}.{op}"

    if op in ARRAY_OPS:
        if op in {"has_tags_all", "has_tags_any"}:
            tags = _normalize_str_list(raw_value, node_path)
            unknown = sorted(tag for tag in tags if tag not in known_tags)
            if unknown:
                raise ValueError(
                    f"{node_path} contains unknown tags: {', '.join(unknown)}"
                )
            return {op: tags}

        children_raw = raw_value
        if not isinstance(children_raw, list) or len(children_raw) == 0:
            raise ValueError(f"{node_path} must be a non-empty array")

        children = [
            _validate_condition(child, known_tags, f"{node_path}[{index}]")
            for index, child in enumerate(children_raw)
        ]
        return {op: children}

    if op in UNARY_OPS:
        child = _validate_condition(raw_value, known_tags, node_path)
        return {op: child}

    raise ValueError(f"{node_path} is not a supported operator")


def _load_known_tags(path: Path) -> set[str]:
    data = _read_json(path)
    if not isinstance(data, dict):
        raise ValueError("tags runtime root must be an object")

    name_to_id = data.get("name_to_id")
    if not isinstance(name_to_id, dict) or len(name_to_id) == 0:
        raise ValueError("tags runtime must contain a non-empty name_to_id map")

    known_tags: set[str] = set()
    for name in name_to_id.keys():
        if not isinstance(name, str) or not name.strip():
            raise ValueError("tags runtime contains invalid tag key")
        known_tags.add(name.strip())
    return known_tags


def _compile_fixtures(
    fixtures_data: Any, known_tags: set[str]
) -> tuple[list[FixtureResult], list[FixtureResult]]:
    if not isinstance(fixtures_data, dict):
        raise ValueError("condition fixtures root must be an object")

    fixtures = fixtures_data.get("fixtures")
    if not isinstance(fixtures, list):
        raise ValueError("fixtures must be an array")

    seen_names: set[str] = set()
    valid_results: list[FixtureResult] = []
    invalid_results: list[FixtureResult] = []

    for index, fixture in enumerate(fixtures):
        entry_path = f"fixtures[{index}]"
        if not isinstance(fixture, dict):
            raise ValueError(f"{entry_path} must be an object")

        name = fixture.get("name")
        if not isinstance(name, str) or not name.strip():
            raise ValueError(f"{entry_path}.name must be a non-empty string")
        fixture_name = name.strip()
        if fixture_name in seen_names:
            raise ValueError(f"Duplicate fixture name '{fixture_name}'")
        seen_names.add(fixture_name)

        if "condition" not in fixture:
            raise ValueError(f"{entry_path}.condition is required")

        try:
            compiled = _validate_condition(fixture["condition"], known_tags)
        except ValueError as exc:
            if fixture_name.startswith("invalid_"):
                invalid_results.append(FixtureResult(name=fixture_name, error=str(exc)))
                continue
            raise ValueError(f"{entry_path}: {exc}") from exc

        if fixture_name.startswith("invalid_"):
            raise ValueError(
                f"{entry_path}: expected invalid fixture '{fixture_name}' to fail compilation"
            )

        valid_results.append(FixtureResult(name=fixture_name, condition=compiled))

    valid_results.sort(key=lambda item: item.name)
    invalid_results.sort(key=lambda item: item.name)
    return valid_results, invalid_results


def _write_runtime(
    path: Path, valid_results: list[FixtureResult], invalid_results: list[FixtureResult]
) -> None:
    runtime = {
        "version": 1,
        "fixtures": [
            {"name": item.name, "condition": item.condition} for item in valid_results
        ],
        "invalid_fixtures": [
            {"name": item.name, "error": item.error} for item in invalid_results
        ],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(runtime, handle, indent=2, sort_keys=True)
        handle.write("\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compile Combat V2 condition fixtures")
    parser.add_argument(
        "--input",
        type=Path,
        default=DEFAULT_INPUT,
        help="Source condition fixtures JSON",
    )
    parser.add_argument(
        "--tags-runtime",
        type=Path,
        default=DEFAULT_TAGS_RUNTIME,
        help="Compiled tags runtime JSON used for tag validation",
    )
    parser.add_argument(
        "--output", type=Path, default=DEFAULT_OUTPUT, help="Output runtime JSON path"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        known_tags = _load_known_tags(args.tags_runtime)
        fixtures_data = _read_json(args.input)
        valid_results, invalid_results = _compile_fixtures(fixtures_data, known_tags)
        _write_runtime(args.output, valid_results, invalid_results)
        print(
            f"Compiled {len(valid_results)} condition fixture(s) and "
            f"recorded {len(invalid_results)} invalid fixture(s): {args.output}"
        )
        return 0
    except FileNotFoundError as exc:
        print(f"ERROR: Missing file: {exc}", file=sys.stderr)
    except PermissionError as exc:
        print(f"ERROR: Permission denied: {exc}", file=sys.stderr)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
