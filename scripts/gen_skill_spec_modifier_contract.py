from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

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
DEFAULT_CANONICAL = (
    REPO_ROOT
    / "tests"
    / "fixtures"
    / "schema"
    / "modifier"
    / "skill_spec_modifiers.canonical.runtime.valid.json"
)
DEFAULT_OUTPUT = (
    REPO_ROOT / "assets" / "data" / "modifier_v2" / "skill_spec_modifiers.json"
)


def _load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def _serialize_json(payload: Any) -> str:
    return json.dumps(payload, ensure_ascii=False, indent=2) + "\n"


def _require_object(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{context} must be an object")
    return value


def _require_list(value: Any, context: str) -> list[Any]:
    if not isinstance(value, list):
        raise ValueError(f"{context} must be a list")
    return value


def _require_int(value: Any, context: str) -> int:
    if not isinstance(value, int):
        raise ValueError(f"{context} must be an integer")
    return value


def _require_string(value: Any, context: str) -> str:
    if not isinstance(value, str):
        raise ValueError(f"{context} must be a string")
    return value


def _validate_runtime_payload(runtime_payload: dict[str, Any], context: str) -> None:
    _require_int(runtime_payload.get("priority"), f"{context}.priority")
    _require_int(runtime_payload.get("profession_mask"), f"{context}.profession_mask")
    _require_int(
        runtime_payload.get("required_skill_tags_all"),
        f"{context}.required_skill_tags_all",
    )
    _require_int(
        runtime_payload.get("forbidden_skill_tags_any"),
        f"{context}.forbidden_skill_tags_any",
    )
    _require_int(
        runtime_payload.get("weapon_class_mask"), f"{context}.weapon_class_mask"
    )
    _require_int(runtime_payload.get("equip_slot_mask"), f"{context}.equip_slot_mask")
    _require_int(runtime_payload.get("exclusive_group"), f"{context}.exclusive_group")
    _require_int(runtime_payload.get("max_active"), f"{context}.max_active")
    _require_int(runtime_payload.get("param_u32"), f"{context}.param_u32")
    _require_string(runtime_payload.get("opcode"), f"{context}.opcode")
    _require_string(runtime_payload.get("target"), f"{context}.target")
    _require_string(runtime_payload.get("debug_name"), f"{context}.debug_name")
    _require_string(runtime_payload.get("debug_source"), f"{context}.debug_source")

    param_f32 = runtime_payload.get("param_f32")
    if not isinstance(param_f32, (int, float)):
        raise ValueError(f"{context}.param_f32 must be numeric")

    skill_ids = _require_list(
        runtime_payload.get("skill_id_whitelist"), f"{context}.skill_id_whitelist"
    )
    node_ids = _require_list(
        runtime_payload.get("node_id_whitelist"), f"{context}.node_id_whitelist"
    )
    for index, value in enumerate(skill_ids):
        _require_int(value, f"{context}.skill_id_whitelist[{index}]")
    for index, value in enumerate(node_ids):
        _require_int(value, f"{context}.node_id_whitelist[{index}]")


def generate_runtime_document(
    schema: dict[str, Any],
    canonical_doc: dict[str, Any],
) -> dict[str, Any]:
    records = _require_list(canonical_doc.get("records"), "canonical.records")
    runtime_records: list[dict[str, Any]] = []

    for record_index, item in enumerate(records):
        item_obj = _require_object(item, f"canonical.records[{record_index}]")
        canonical_record = _require_object(
            item_obj.get("record"),
            f"canonical.records[{record_index}].record",
        )
        runtime_payload = _require_object(
            item_obj.get("runtime"),
            f"canonical.records[{record_index}].runtime",
        )

        schema_errors = validate_canonical_schema.validate_instance(
            schema=schema,
            instance=canonical_record,
        )
        if schema_errors:
            raise ValueError(
                "canonical schema validation failed for "
                f"canonical.records[{record_index}].record: {schema_errors[0]}"
            )

        _validate_runtime_payload(
            runtime_payload,
            context=f"canonical.records[{record_index}].runtime",
        )

        domain = _require_string(canonical_record.get("domain"), "record.domain")
        if domain != "skill_spec":
            raise ValueError(
                f"canonical.records[{record_index}].record.domain must be 'skill_spec'"
            )

        runtime_records.append(
            {
                "id": _require_int(
                    canonical_record.get("modifier_id"),
                    f"canonical.records[{record_index}].record.modifier_id",
                ),
                "domain": domain,
                "priority": _require_int(
                    runtime_payload.get("priority"),
                    f"canonical.records[{record_index}].runtime.priority",
                ),
                "filters": {
                    "profession_mask": runtime_payload["profession_mask"],
                    "skill_id_whitelist": runtime_payload["skill_id_whitelist"],
                    "required_skill_tags_all": runtime_payload[
                        "required_skill_tags_all"
                    ],
                    "forbidden_skill_tags_any": runtime_payload[
                        "forbidden_skill_tags_any"
                    ],
                    "weapon_class_mask": runtime_payload["weapon_class_mask"],
                    "equip_slot_mask": runtime_payload["equip_slot_mask"],
                    "node_id_whitelist": runtime_payload["node_id_whitelist"],
                },
                "constraints": {
                    "exclusive_group": runtime_payload["exclusive_group"],
                    "max_active": runtime_payload["max_active"],
                },
                "ops": [
                    {
                        "opcode": runtime_payload["opcode"],
                        "target": runtime_payload["target"],
                        "param_u32": runtime_payload["param_u32"],
                        "param_f32": float(runtime_payload["param_f32"]),
                    }
                ],
                "debug": {
                    "name": runtime_payload["debug_name"],
                    "source": runtime_payload["debug_source"],
                },
            }
        )

    return {
        "schema_version": 2,
        "domain": "skill_spec",
        "records": runtime_records,
    }


def run(
    *,
    schema_path: Path,
    canonical_path: Path,
    output_path: Path,
    check_only: bool,
) -> int:
    schema = _load_json(schema_path)
    canonical_doc = _load_json(canonical_path)
    runtime_doc = generate_runtime_document(
        schema=_require_object(schema, str(schema_path)),
        canonical_doc=_require_object(canonical_doc, str(canonical_path)),
    )
    runtime_text = _serialize_json(runtime_doc)

    if check_only:
        current_text = output_path.read_text(encoding="utf-8")
        if current_text != runtime_text:
            print(
                "[FAIL] skill_spec runtime contract drift detected. "
                "Run: python scripts/gen_skill_spec_modifier_contract.py"
            )
            return 1
        print("[OK] skill_spec runtime contract is up to date.")
        return 0

    output_path.write_text(runtime_text, encoding="utf-8")
    print(f"[OK] generated {output_path}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate skill_spec runtime modifier contract from canonical fixture.",
    )
    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA)
    parser.add_argument("--canonical", type=Path, default=DEFAULT_CANONICAL)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    try:
        return run(
            schema_path=args.schema.resolve(),
            canonical_path=args.canonical.resolve(),
            output_path=args.output.resolve(),
            check_only=args.check,
        )
    except ValueError as exc:
        print(f"[FAIL] {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
