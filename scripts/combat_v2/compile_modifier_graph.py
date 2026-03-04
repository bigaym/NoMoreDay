#!/usr/bin/env python3
"""Compile combat_v2 modifier graph records into deterministic runtime JSON."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any


VALID_STAGES = {"pre_hit", "hit", "post_hit", "dot_tick"}
VALID_OPS = {
    "flat",
    "increased",
    "more",
    "convert",
    "gain_extra",
    "clamp_min",
    "clamp_max",
}

STAGE_RANK = {
    "pre_hit": 0,
    "hit": 1,
    "post_hit": 2,
    "dot_tick": 3,
}

UINT32_MAX = 4294967295
UINT16_MAX = 65535


def _expect_int(value: Any, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"{field} must be an integer")
    return value


def _expect_float(value: Any, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{field} must be numeric")
    numeric = float(value)
    if not math.isfinite(numeric):
        raise ValueError(f"{field} must be finite")
    return numeric


def _expect_str(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field} must be a non-empty string")
    return value


def _expect_int_list(value: Any, field: str) -> list[int]:
    if value is None:
        return []
    if not isinstance(value, list):
        raise ValueError(f"{field} must be an array")
    output: list[int] = []
    seen: set[int] = set()
    for raw in value:
        parsed = _expect_int(raw, field)
        if parsed in seen:
            raise ValueError(f"{field} contains duplicate value {parsed}")
        seen.add(parsed)
        output.append(parsed)
    return output


def _expect_uint32(value: Any, field: str) -> int:
    parsed = _expect_int(value, field)
    if parsed < 0 or parsed > UINT32_MAX:
        raise ValueError(f"{field} must be in range [0, {UINT32_MAX}]")
    return parsed


def _expect_uint16(value: Any, field: str) -> int:
    parsed = _expect_int(value, field)
    if parsed < 0 or parsed > UINT16_MAX:
        raise ValueError(f"{field} must be in range [0, {UINT16_MAX}]")
    return parsed


def _load_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise FileNotFoundError(f"input file not found: {path}")
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON in {path}: {exc}") from exc
    if not isinstance(payload, dict):
        raise ValueError("root must be an object")
    return payload


def _compile_records(payload: dict[str, Any]) -> dict[str, Any]:
    records = payload.get("records", [])
    if not isinstance(records, list):
        raise ValueError("records must be an array")

    compiled: list[dict[str, Any]] = []
    seen_node_ids: set[int] = set()
    for idx, raw in enumerate(records):
        field_prefix = f"records[{idx}]"
        if not isinstance(raw, dict):
            raise ValueError(f"{field_prefix} must be an object")

        node_id = _expect_uint32(raw.get("node_id"), f"{field_prefix}.node_id")
        if node_id in seen_node_ids:
            raise ValueError(f"duplicate node_id detected: {node_id}")
        seen_node_ids.add(node_id)

        stage = _expect_str(raw.get("stage"), f"{field_prefix}.stage")
        if stage not in VALID_STAGES:
            raise ValueError(f"{field_prefix}.stage invalid: {stage}")

        op = _expect_str(raw.get("op"), f"{field_prefix}.op")
        if op not in VALID_OPS:
            raise ValueError(f"{field_prefix}.op invalid: {op}")

        compiled.append(
            {
                "node_id": node_id,
                "stage": stage,
                "op": op,
                "value": _expect_float(raw.get("value"), f"{field_prefix}.value"),
                "condition_program_id": _expect_uint32(
                    raw.get("condition_program_id", 0),
                    f"{field_prefix}.condition_program_id",
                ),
                "priority": _expect_uint16(
                    raw.get("priority", 0), f"{field_prefix}.priority"
                ),
                "source_id": _expect_uint32(
                    raw.get("source_id", 0), f"{field_prefix}.source_id"
                ),
                "forbidden_filter_ids": sorted(
                    _expect_uint32(raw_filter, f"{field_prefix}.forbidden_filter_ids")
                    for raw_filter in _expect_int_list(
                        raw.get("forbidden_filter_ids", []),
                        f"{field_prefix}.forbidden_filter_ids",
                    )
                ),
                "node_whitelist": sorted(
                    _expect_uint32(raw_node, f"{field_prefix}.node_whitelist")
                    for raw_node in _expect_int_list(
                        raw.get("node_whitelist", []), f"{field_prefix}.node_whitelist"
                    )
                ),
            }
        )

    compiled.sort(
        key=lambda item: (
            STAGE_RANK[item["stage"]],
            item["priority"],
            item["source_id"],
            item["node_id"],
        )
    )
    return {
        "schema_version": payload.get("schema_version", 1),
        "record_count": len(compiled),
        "records": compiled,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path("assets/data/combat_v2/modifier_graph.json"),
        help="canonical modifier graph input JSON",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("assets/data/combat_v2/modifier_graph.runtime.json"),
        help="compiled runtime output JSON",
    )
    args = parser.parse_args(argv)

    payload = _load_json(args.input)
    compiled = _compile_records(payload)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(compiled, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"compiled records: {compiled['record_count']}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (FileNotFoundError, PermissionError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(2)
