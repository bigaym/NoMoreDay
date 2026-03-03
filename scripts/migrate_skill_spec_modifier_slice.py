from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, cast

import validate_canonical_schema


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SCHEMA = (
    REPO_ROOT
    / "assets"
    / "data"
    / "modifier_v2"
    / "canonical"
    / "skill_spec_modifier_record.schema.json"
)
DEFAULT_RUNTIME_INPUT = (
    REPO_ROOT / "assets" / "data" / "modifier_v2" / "skill_spec_modifiers.json"
)
DEFAULT_CANONICAL_OUTPUT = (
    REPO_ROOT
    / "assets"
    / "data"
    / "modifier_v2"
    / "canonical"
    / "skill_spec_modifiers.canonical.json"
)
DEFAULT_REPORT_OUTPUT = (
    REPO_ROOT
    / "docs"
    / "reports"
    / "four-pillars"
    / "phase-4"
    / "D4-3"
    / "artifacts"
    / "migration-report.json"
)
DEFAULT_DROP_LIST_OUTPUT = (
    REPO_ROOT
    / "docs"
    / "reports"
    / "four-pillars"
    / "phase-4"
    / "D4-3"
    / "artifacts"
    / "drop-list.json"
)

SUPPORTED_OPCODE_TO_OPERATION = {
    "ADD_STAT_FLAT": "add",
    "ADD_STAT_PERCENT_MULT": "mul",
}

TARGET_PATTERN = re.compile(r"^[a-z][a-z0-9_.]*$")


def _load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def _serialize_json(payload: Any) -> str:
    return json.dumps(payload, ensure_ascii=False, indent=2) + "\n"


def _is_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _is_number(value: Any) -> bool:
    return (isinstance(value, int) or isinstance(value, float)) and not isinstance(
        value, bool
    )


def _derive_tags(stat_path: str) -> list[str]:
    first_token = stat_path.split(".")[0]
    if TARGET_PATTERN.match(first_token) and 3 <= len(first_token) <= 24:
        return [first_token]
    return ["misc"]


def _build_conditions(skill_id_whitelist: list[int]) -> dict[str, Any]:
    unique_ids = list(dict.fromkeys(skill_id_whitelist))
    return {
        "all_skill_ids": unique_ids,
        "min_player_level": 1,
    }


def _as_repo_relative(path: Path) -> str:
    resolved = path.resolve()
    try:
        relative = resolved.relative_to(REPO_ROOT)
    except ValueError:
        return resolved.as_posix()
    return relative.as_posix()


def _migrate_record(
    index: int,
    record: Any,
    schema: dict[str, Any],
) -> tuple[dict[str, Any] | None, dict[str, Any] | None]:
    dropped: dict[str, Any] = {
        "index": index,
        "record_id": None,
        "reasons": [],
    }

    if not isinstance(record, dict):
        dropped["reasons"].append("record must be an object")
        return None, dropped

    record_id = record.get("id")
    if isinstance(record_id, int) and not isinstance(record_id, bool):
        dropped["record_id"] = record_id
        if record_id < 1:
            dropped["reasons"].append("id must be an integer >= 1")
    else:
        dropped["reasons"].append("id must be an integer >= 1")

    domain = record.get("domain")
    if domain != "skill_spec":
        dropped["reasons"].append("domain must be 'skill_spec'")

    priority = record.get("priority")
    if not _is_int(priority):
        dropped["reasons"].append("priority must be an integer")

    filters = record.get("filters")
    if not isinstance(filters, dict):
        dropped["reasons"].append("filters must be an object")
        return None, dropped

    required_filter_fields = [
        "profession_mask",
        "required_skill_tags_all",
        "forbidden_skill_tags_any",
        "weapon_class_mask",
        "equip_slot_mask",
        "skill_id_whitelist",
        "node_id_whitelist",
    ]
    for key in required_filter_fields:
        if key not in filters:
            dropped["reasons"].append(f"filters.{key} is required")

    integer_filter_fields = [
        "profession_mask",
        "required_skill_tags_all",
        "forbidden_skill_tags_any",
        "weapon_class_mask",
        "equip_slot_mask",
    ]
    for key in integer_filter_fields:
        if key in filters and not _is_int(filters[key]):
            dropped["reasons"].append(f"filters.{key} must be an integer")

    skill_id_whitelist = filters.get("skill_id_whitelist")
    if not isinstance(skill_id_whitelist, list):
        dropped["reasons"].append("filters.skill_id_whitelist must be an array")
    else:
        if len(skill_id_whitelist) == 0:
            dropped["reasons"].append("filters.skill_id_whitelist must not be empty")
        for value in skill_id_whitelist:
            if not _is_int(value) or value < 1:
                dropped["reasons"].append(
                    "filters.skill_id_whitelist must contain integers >= 1"
                )
                break

    constraints = record.get("constraints")
    if not isinstance(constraints, dict):
        dropped["reasons"].append("constraints must be an object")
        return None, dropped

    if not _is_int(constraints.get("exclusive_group")):
        dropped["reasons"].append("constraints.exclusive_group must be an integer")
    if not _is_int(constraints.get("max_active")):
        dropped["reasons"].append("constraints.max_active must be an integer")

    ops = record.get("ops")
    if not isinstance(ops, list):
        dropped["reasons"].append("ops must be an array")
        return None, dropped
    if len(ops) != 1:
        dropped["reasons"].append("ops must contain exactly one operation")
        return None, dropped

    op = ops[0]
    if not isinstance(op, dict):
        dropped["reasons"].append("ops[0] must be an object")
        return None, dropped

    opcode = op.get("opcode")
    operation: str | None = None
    if isinstance(opcode, str):
        operation = SUPPORTED_OPCODE_TO_OPERATION.get(opcode)
    if operation is None:
        dropped["reasons"].append(
            "unsupported opcode; expected one of "
            + ", ".join(sorted(SUPPORTED_OPCODE_TO_OPERATION.keys()))
        )

    stat_path = op.get("target")
    if not isinstance(stat_path, str) or not TARGET_PATTERN.match(stat_path):
        dropped["reasons"].append(
            "ops[0].target must match canonical stat_path pattern"
        )

    stacks = op.get("param_u32")
    if isinstance(stacks, int) and not isinstance(stacks, bool):
        if stacks < 1 or stacks > 99:
            dropped["reasons"].append("ops[0].param_u32 must be in [1, 99]")
    else:
        dropped["reasons"].append("ops[0].param_u32 must be an integer")

    value = op.get("param_f32")
    value_number = 0.0
    if not _is_number(value):
        dropped["reasons"].append("ops[0].param_f32 must be numeric")
    else:
        value_number = float(cast(int | float, value))

    debug = record.get("debug")
    if not isinstance(debug, dict):
        dropped["reasons"].append("debug must be an object")
        return None, dropped
    debug_name = debug.get("name")
    debug_source = debug.get("source")
    if not isinstance(debug_name, str):
        dropped["reasons"].append("debug.name must be a string")
    if not isinstance(debug_source, str):
        dropped["reasons"].append("debug.source must be a string")

    if dropped["reasons"]:
        return None, dropped

    assert isinstance(skill_id_whitelist, list)
    assert isinstance(stat_path, str)
    assert isinstance(stacks, int)
    assert isinstance(opcode, str)
    assert isinstance(debug_name, str)
    assert isinstance(debug_source, str)
    assert operation is not None
    assert _is_int(record_id)
    assert _is_int(priority)

    record_id_int = cast(int, record_id)
    priority_int = cast(int, priority)
    stacks_int = cast(int, stacks)

    canonical_record = {
        "schema_version": 1,
        "record_type": "modifier",
        "modifier_id": record_id_int,
        "domain": "skill_spec",
        "operation": operation,
        "stat_path": stat_path,
        "value": value_number,
        "stacks": stacks_int,
        "tags": _derive_tags(stat_path),
        "conditions": _build_conditions(skill_id_whitelist),
    }

    schema_errors = validate_canonical_schema.validate_instance(
        schema=schema,
        instance=canonical_record,
    )
    if schema_errors:
        dropped["reasons"].append(
            "canonical schema validation failed: " + schema_errors[0]
        )
        return None, dropped

    runtime_payload = {
        "priority": priority_int,
        "profession_mask": filters["profession_mask"],
        "skill_id_whitelist": skill_id_whitelist,
        "required_skill_tags_all": filters["required_skill_tags_all"],
        "forbidden_skill_tags_any": filters["forbidden_skill_tags_any"],
        "weapon_class_mask": filters["weapon_class_mask"],
        "equip_slot_mask": filters["equip_slot_mask"],
        "node_id_whitelist": filters["node_id_whitelist"],
        "exclusive_group": constraints["exclusive_group"],
        "max_active": constraints["max_active"],
        "opcode": opcode,
        "target": stat_path,
        "param_u32": stacks_int,
        "param_f32": value_number,
        "debug_name": debug_name,
        "debug_source": debug_source,
    }
    return {
        "record": canonical_record,
        "runtime": runtime_payload,
    }, None


def migrate_runtime_document(
    schema: dict[str, Any],
    runtime_doc: dict[str, Any],
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    records = runtime_doc.get("records")
    if not isinstance(records, list):
        raise ValueError("runtime.records must be an array")

    canonical_records: list[dict[str, Any]] = []
    dropped_records: list[dict[str, Any]] = []
    for index, record in enumerate(records):
        migrated, dropped = _migrate_record(index=index, record=record, schema=schema)
        if migrated is not None:
            canonical_records.append(migrated)
        if dropped is not None:
            dropped_records.append(dropped)

    return {
        "schema_version": 1,
        "records": canonical_records,
    }, dropped_records


def build_migration_report(
    *,
    runtime_input_path: Path,
    canonical_output_path: Path,
    total_records: int,
    migrated_records: int,
    dropped_records: int,
) -> dict[str, Any]:
    return {
        "slice": "skill_spec_modifier_canonical",
        "source_of_truth_policy": {
            "canonical_input": _as_repo_relative(canonical_output_path),
            "runtime_generated_by": "scripts/gen_skill_spec_modifier_contract.py",
            "migration_path": "runtime_to_canonical",
        },
        "runtime_input": _as_repo_relative(runtime_input_path),
        "totals": {
            "records": total_records,
            "migrated": migrated_records,
            "dropped": dropped_records,
        },
    }


def _write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(_serialize_json(payload), encoding="utf-8")


def _check_exact(path: Path, expected_text: str, label: str) -> bool:
    if not path.exists():
        print(f"[FAIL] {label} missing: {path}")
        return False
    current = path.read_text(encoding="utf-8")
    if current != expected_text:
        print(f"[FAIL] {label} drift: {path}")
        return False
    return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Migrate skill_spec runtime modifier config to canonical paired input "
            "with migration and drop reports."
        )
    )
    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA)
    parser.add_argument("--runtime-input", type=Path, default=DEFAULT_RUNTIME_INPUT)
    parser.add_argument(
        "--canonical-output", type=Path, default=DEFAULT_CANONICAL_OUTPUT
    )
    parser.add_argument("--report-output", type=Path, default=DEFAULT_REPORT_OUTPUT)
    parser.add_argument(
        "--drop-list-output", type=Path, default=DEFAULT_DROP_LIST_OUTPUT
    )
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--fail-on-drop", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        schema = _load_json(args.schema.resolve())
        runtime_doc = _load_json(args.runtime_input.resolve())
    except (OSError, json.JSONDecodeError) as exc:
        print(f"[FAIL] failed to load input: {exc}")
        return 1

    try:
        canonical_doc, dropped_records = migrate_runtime_document(
            schema=schema,
            runtime_doc=runtime_doc,
        )
    except ValueError as exc:
        print(f"[FAIL] {exc}")
        return 1

    total_records = len(runtime_doc.get("records", []))
    report = build_migration_report(
        runtime_input_path=args.runtime_input.resolve(),
        canonical_output_path=args.canonical_output.resolve(),
        total_records=total_records,
        migrated_records=len(canonical_doc["records"]),
        dropped_records=len(dropped_records),
    )
    drop_list = {
        "runtime_input": _as_repo_relative(args.runtime_input.resolve()),
        "dropped": dropped_records,
    }

    canonical_text = _serialize_json(canonical_doc)
    report_text = _serialize_json(report)
    drop_text = _serialize_json(drop_list)

    if args.check:
        ok = True
        ok = (
            _check_exact(
                args.canonical_output.resolve(),
                canonical_text,
                "canonical migration output",
            )
            and ok
        )
        ok = (
            _check_exact(
                args.report_output.resolve(),
                report_text,
                "migration report",
            )
            and ok
        )
        ok = (
            _check_exact(
                args.drop_list_output.resolve(),
                drop_text,
                "drop list",
            )
            and ok
        )
        if args.fail_on_drop and dropped_records:
            print(
                "[FAIL] migration dropped unsupported records "
                f"({len(dropped_records)}); inspect drop-list artifact"
            )
            return 1
        if not ok:
            print(
                "[FAIL] migration artifacts drift detected. "
                "Run: python scripts/migrate_skill_spec_modifier_slice.py"
            )
            return 1
        print(
            "[OK] skill_spec runtime->canonical migration artifacts are up to date "
            f"(migrated={len(canonical_doc['records'])}, dropped={len(dropped_records)})."
        )
        return 0

    _write_json(args.canonical_output.resolve(), canonical_doc)
    _write_json(args.report_output.resolve(), report)
    _write_json(args.drop_list_output.resolve(), drop_list)
    print(
        "[OK] migrated skill_spec runtime->canonical "
        f"(migrated={len(canonical_doc['records'])}, dropped={len(dropped_records)})"
    )

    if args.fail_on_drop and dropped_records:
        print(
            "[FAIL] migration wrote outputs but dropped unsupported records; "
            "inspect drop-list artifact"
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
