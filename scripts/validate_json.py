import json
import os
import sys
from pathlib import Path
from typing import Any

import validate_canonical_schema


_MODIFIER_TOP_LEVEL_REQUIRED = {"schema_version", "domain", "records"}


def _is_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _is_number(value: Any) -> bool:
    return (isinstance(value, int) or isinstance(value, float)) and not isinstance(
        value, bool
    )


def _validate_u32_array(value: Any, path: str) -> list[str]:
    errors: list[str] = []
    if not isinstance(value, list):
        return [f"{path} must be an array"]
    for index, entry in enumerate(value):
        if not _is_int(entry):
            errors.append(f"{path}[{index}] must be an integer")
    return errors


def _validate_modifier_record(index: int, record: Any, domain: str) -> list[str]:
    prefix = f"records[{index}]"
    errors: list[str] = []
    if not isinstance(record, dict):
        return [f"{prefix} must be an object"]

    if not _is_int(record.get("id")):
        errors.append(f"{prefix}.id must be an integer")

    record_domain = record.get("domain")
    if not isinstance(record_domain, str):
        errors.append(f"{prefix}.domain must be a string")
    elif record_domain != domain:
        errors.append(f"{prefix}.domain must match top-level domain")

    if not _is_int(record.get("priority")):
        errors.append(f"{prefix}.priority must be an integer")

    filters = record.get("filters")
    if not isinstance(filters, dict):
        errors.append(f"{prefix}.filters must be an object")
    else:
        if not _is_int(filters.get("profession_mask")):
            errors.append(f"{prefix}.filters.profession_mask must be an integer")
        if not _is_int(filters.get("required_skill_tags_all")):
            errors.append(
                f"{prefix}.filters.required_skill_tags_all must be an integer"
            )
        if not _is_int(filters.get("forbidden_skill_tags_any")):
            errors.append(
                f"{prefix}.filters.forbidden_skill_tags_any must be an integer"
            )
        if not _is_int(filters.get("weapon_class_mask")):
            errors.append(f"{prefix}.filters.weapon_class_mask must be an integer")
        if not _is_int(filters.get("equip_slot_mask")):
            errors.append(f"{prefix}.filters.equip_slot_mask must be an integer")
        errors.extend(
            _validate_u32_array(
                filters.get("skill_id_whitelist"),
                f"{prefix}.filters.skill_id_whitelist",
            )
        )
        errors.extend(
            _validate_u32_array(
                filters.get("node_id_whitelist"),
                f"{prefix}.filters.node_id_whitelist",
            )
        )

    constraints = record.get("constraints")
    if not isinstance(constraints, dict):
        errors.append(f"{prefix}.constraints must be an object")
    else:
        if not _is_int(constraints.get("exclusive_group")):
            errors.append(f"{prefix}.constraints.exclusive_group must be an integer")
        if not _is_int(constraints.get("max_active")):
            errors.append(f"{prefix}.constraints.max_active must be an integer")

    ops = record.get("ops")
    if not isinstance(ops, list):
        errors.append(f"{prefix}.ops must be an array")
    else:
        for op_index, op in enumerate(ops):
            op_prefix = f"{prefix}.ops[{op_index}]"
            if not isinstance(op, dict):
                errors.append(f"{op_prefix} must be an object")
                continue
            if not isinstance(op.get("opcode"), str):
                errors.append(f"{op_prefix}.opcode must be a string")
            if not isinstance(op.get("target"), str):
                errors.append(f"{op_prefix}.target must be a string")
            if not _is_int(op.get("param_u32")):
                errors.append(f"{op_prefix}.param_u32 must be an integer")
            if not _is_number(op.get("param_f32")):
                errors.append(f"{op_prefix}.param_f32 must be a number")

    return errors


def _validate_modifier_catalog(path: Path, payload: Any) -> list[str]:
    errors: list[str] = []
    if not isinstance(payload, dict):
        return ["catalog must be a JSON object"]

    schema_version = payload.get("schema_version")
    if not _is_int(schema_version):
        errors.append("catalog schema_version must be an integer")
    elif schema_version != 2:
        errors.append("catalog schema_version must be 2")

    entries = payload.get("entries")
    if not isinstance(entries, list):
        errors.append("catalog entries must be an array")
        return errors

    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            errors.append(f"entries[{index}] must be an object")
            continue
        for key in ("domain", "path"):
            if key not in entry:
                errors.append(f"entries[{index}] missing '{key}'")

        if "domain" in entry and not isinstance(entry.get("domain"), str):
            errors.append(f"entries[{index}].domain must be a string")
        if "path" in entry and not isinstance(entry.get("path"), str):
            errors.append(f"entries[{index}].path must be a string")

        relative_path = entry.get("path")
        if isinstance(relative_path, str):
            target_path = path.parent / relative_path
            if not target_path.exists():
                errors.append(f"entries[{index}] path does not exist: {relative_path}")

    return errors


def _validate_modifier_record_file(path: Path, payload: Any) -> list[str]:
    errors: list[str] = []
    if not isinstance(payload, dict):
        return ["modifier runtime file must be a JSON object"]

    missing = _MODIFIER_TOP_LEVEL_REQUIRED.difference(payload.keys())
    for key in sorted(missing):
        errors.append(f"missing '{key}'")

    schema_version = payload.get("schema_version")
    if not _is_int(schema_version):
        errors.append("schema_version must be an integer")
    elif schema_version != 2:
        errors.append("schema_version must be 2")

    domain = payload.get("domain")
    if not isinstance(domain, str):
        errors.append("domain must be a string")

    records = payload.get("records")
    if "records" in payload and not isinstance(records, list):
        errors.append("records must be an array")
        return errors

    if isinstance(records, list) and isinstance(domain, str):
        for index, record in enumerate(records):
            errors.extend(_validate_modifier_record(index, record, domain))

    return errors


def _validate_modifier_canonical_file(path: Path, payload: Any) -> list[str]:
    if not path.name.endswith(".canonical.json"):
        return []

    errors: list[str] = []
    if not isinstance(payload, dict):
        return ["canonical modifier file must be a JSON object"]

    records = payload.get("records")
    if not isinstance(records, list):
        return ["canonical file 'records' must be an array"]

    schema_path = path.parent / "skill_spec_modifier_record.schema.json"
    if not schema_path.exists():
        return [f"canonical schema not found: {schema_path}"]

    try:
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    except Exception as exc:  # pylint: disable=broad-except
        return [f"failed to load canonical schema '{schema_path}': {exc}"]

    for index, entry in enumerate(records):
        if not isinstance(entry, dict):
            errors.append(f"records[{index}] must be an object")
            continue

        record = entry.get("record")
        if not isinstance(record, dict):
            errors.append(f"records[{index}].record must be an object")
            continue

        schema_errors = validate_canonical_schema.validate_instance(
            schema=schema,
            instance=record,
        )
        for schema_error in schema_errors:
            errors.append(f"records[{index}].record {schema_error}")

    return errors


def _validate_modifier_v2(path: Path, payload: Any) -> list[str]:
    if "modifier_v2" not in path.parts:
        return []
    if "canonical" in path.parts:
        if path.name.endswith(".schema.json"):
            return []
        return _validate_modifier_canonical_file(path, payload)
    if path.name.endswith(".schema.json"):
        return []
    if path.name == "modifier_catalog.json":
        return _validate_modifier_catalog(path, payload)
    return _validate_modifier_record_file(path, payload)


def validate_json_files(data_dir: str) -> bool:
    failed = False
    print(f"[JSON Validator] Scanning directory: {data_dir}")

    for root, _, files in os.walk(data_dir):
        for file_name in files:
            if not file_name.endswith(".json"):
                continue

            file_path = Path(root) / file_name
            try:
                with file_path.open("r", encoding="utf-8-sig") as file_stream:
                    payload = json.load(file_stream)
            except json.JSONDecodeError as err:
                print(f"FAILED: {file_path}")
                print(f"  Error: {err}")
                failed = True
                continue
            except OSError as err:
                print(f"ERROR reading {file_path}: {err}")
                failed = True
                continue

            modifier_errors = _validate_modifier_v2(file_path, payload)
            if modifier_errors:
                print(f"FAILED: {file_path}")
                for err in modifier_errors:
                    print(f"  Error: {err}")
                failed = True

    return not failed


if __name__ == "__main__":
    target_dir = "assets/data"
    if len(sys.argv) > 1:
        target_dir = sys.argv[1]

    if validate_json_files(target_dir):
        print("[JSON Validator] All JSON files are valid.")
        sys.exit(0)

    print("[JSON Validator] Some JSON files failed validation.")
    sys.exit(1)
