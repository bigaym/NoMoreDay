import argparse
import json
import struct
import zlib
from pathlib import Path
from typing import Any


HEADER_STRUCT = struct.Struct("<IHHIIIIIIIII20s")
RECORD_STRUCT = struct.Struct("<IIIIII")
FILTER_STRUCT = struct.Struct("<QQQIIIIII")
OP_STRUCT = struct.Struct("<HHIf")
RUNTIME_MAGIC = 0x4D444D4E
FORMAT_VERSION = 2

OPCODE_VALUES: dict[str, int] = {
    "ADD_STAT_FLAT": 0,
    "ADD_STAT_PERCENT_ADD": 1,
    "ADD_STAT_PERCENT_MULT": 2,
    "ADD_SKILL_LEVEL": 3,
    "MANA_COST_MULT": 4,
    "MONSTER_EVENT_ON_UPDATE": 5,
    "MONSTER_EVENT_ON_HIT": 6,
    "MONSTER_EVENT_ON_DEATH": 7,
    "MONSTER_BEHAVIOR_MOLTEN_UPDATE": 8,
    "MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT": 9,
    "MONSTER_BEHAVIOR_TELEPORTER_UPDATE": 10,
    "MONSTER_BEHAVIOR_FROZEN_UPDATE": 11,
    "MONSTER_BEHAVIOR_NULLIFIER_ON_HIT": 12,
    "MONSTER_BEHAVIOR_ENTANGLER_ON_HIT": 13,
    "MONSTER_BEHAVIOR_TOXIC_ON_DEATH": 14,
}


def read_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as file_stream:
        return json.load(file_stream)


def load_records(input_dir: Path) -> list[dict[str, Any]]:
    catalog_path = input_dir / "modifier_catalog.json"
    catalog = read_json(catalog_path)
    entries = catalog.get("entries", [])
    if not isinstance(entries, list):
        raise ValueError("modifier catalog entries must be an array")

    records: list[dict[str, Any]] = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("modifier catalog entry must be an object")
        relative_path = entry.get("path")
        if not isinstance(relative_path, str):
            raise ValueError("modifier catalog entry missing string path")

        payload = read_json(input_dir / relative_path)
        payload_records = payload.get("records", [])
        if not isinstance(payload_records, list):
            raise ValueError(f"records must be an array: {relative_path}")

        for record in payload_records:
            if not isinstance(record, dict):
                raise ValueError(f"record must be an object: {relative_path}")
            records.append(record)

    return records


def _int_field(record: dict[str, Any], key: str, default: int = 0) -> int:
    value = record.get(key, default)
    if not isinstance(value, int):
        raise ValueError(f"record field '{key}' must be an int")
    return value


def _list_int(values: Any, key: str) -> list[int]:
    if values is None:
        return []
    if not isinstance(values, list):
        raise ValueError(f"field '{key}' must be a list")
    parsed: list[int] = []
    for value in values:
        if not isinstance(value, int):
            raise ValueError(f"field '{key}' must contain ints")
        parsed.append(value)
    return parsed


def _float_field(op: dict[str, Any], key: str, default: float = 0.0) -> float:
    value = op.get(key, default)
    if not isinstance(value, (int, float)):
        raise ValueError(f"op field '{key}' must be numeric")
    return float(value)


def _parse_opcode(opcode_name: Any) -> int:
    if not isinstance(opcode_name, str):
        raise ValueError("op 'opcode' must be a string")
    if opcode_name not in OPCODE_VALUES:
        raise ValueError(f"unsupported opcode '{opcode_name}'")
    return OPCODE_VALUES[opcode_name]


def compile_runtime_blob(
    records: list[dict[str, Any]],
) -> tuple[bytes, list[dict[str, Any]]]:
    sorted_records = sorted(
        records, key=lambda record: (record.get("priority", 0), record.get("id", 0))
    )

    record_rows: list[bytes] = []
    filter_rows: list[bytes] = []
    op_rows: list[bytes] = []
    index_rows: list[int] = []

    for record in sorted_records:
        record_id = _int_field(record, "id")
        priority = _int_field(record, "priority", 0)

        filters = record.get("filters", {})
        if not isinstance(filters, dict):
            raise ValueError("record filters must be an object")

        skill_whitelist = _list_int(
            filters.get("skill_id_whitelist", []), "skill_id_whitelist"
        )
        node_whitelist = _list_int(
            filters.get("node_id_whitelist", []), "node_id_whitelist"
        )

        skill_offset = len(index_rows)
        index_rows.extend(skill_whitelist)
        node_offset = len(index_rows)
        index_rows.extend(node_whitelist)

        filter_index = len(filter_rows)
        filter_rows.append(
            FILTER_STRUCT.pack(
                _int_field(filters, "profession_mask", 0),
                _int_field(filters, "required_skill_tags_all", 0),
                _int_field(filters, "forbidden_skill_tags_any", 0),
                _int_field(filters, "weapon_class_mask", 0xFFFFFFFF),
                _int_field(filters, "equip_slot_mask", 0),
                skill_offset,
                len(skill_whitelist),
                node_offset,
                len(node_whitelist),
            )
        )

        ops = record.get("ops", [])
        if not isinstance(ops, list):
            raise ValueError("record ops must be a list")
        op_offset = len(op_rows)
        for op in ops:
            if not isinstance(op, dict):
                raise ValueError("op must be an object")
            op_rows.append(
                OP_STRUCT.pack(
                    _parse_opcode(op.get("opcode")),
                    0,
                    _int_field(op, "param_u32", 0),
                    _float_field(op, "param_f32", 0.0),
                )
            )

        record_rows.append(
            RECORD_STRUCT.pack(
                record_id,
                priority,
                filter_index,
                op_offset,
                len(ops),
                0,
            )
        )

    records_bytes = b"".join(record_rows)
    filters_bytes = b"".join(filter_rows)
    ops_bytes = b"".join(op_rows)
    index_bytes = b"".join(struct.pack("<I", value) for value in index_rows)

    records_offset = HEADER_STRUCT.size
    filters_offset = records_offset + len(records_bytes)
    ops_offset = filters_offset + len(filters_bytes)
    index_offset = ops_offset + len(ops_bytes)
    payload = records_bytes + filters_bytes + ops_bytes + index_bytes
    crc32 = zlib.crc32(payload) & 0xFFFFFFFF

    header = HEADER_STRUCT.pack(
        RUNTIME_MAGIC,
        FORMAT_VERSION,
        1,
        len(sorted_records),
        len(filter_rows),
        len(op_rows),
        len(index_rows),
        records_offset,
        filters_offset,
        ops_offset,
        index_offset,
        crc32,
        b"\x00" * 20,
    )

    return header + payload, sorted_records


def compile_from_input(input_dir: Path) -> tuple[bytes, list[dict[str, Any]]]:
    records = load_records(input_dir)
    return compile_runtime_blob(records)


def write_outputs(
    blob: bytes,
    sorted_records: list[dict[str, Any]],
    output_bin: Path,
    output_debug: Path,
) -> None:
    output_bin.parent.mkdir(parents=True, exist_ok=True)
    output_debug.parent.mkdir(parents=True, exist_ok=True)

    output_bin.write_bytes(blob)
    debug_payload = {
        "schema_version": FORMAT_VERSION,
        "record_count": len(sorted_records),
        "records": sorted_records,
    }
    output_debug.write_text(
        json.dumps(debug_payload, indent=2, ensure_ascii=True), encoding="utf-8"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compile Modifier Runtime V2 binary.")
    parser.add_argument("--input-dir", default="assets/data/modifier_v2")
    parser.add_argument(
        "--output-bin", default="assets/generated/modifier_runtime_v2.bin"
    )
    parser.add_argument(
        "--output-debug", default="assets/generated/modifier_runtime_v2.debug.json"
    )
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--check-determinism", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    input_dir = Path(args.input_dir)
    output_bin = Path(args.output_bin)
    output_debug = Path(args.output_debug)

    if not args.check and not args.check_determinism:
        args.check = True

    if args.check:
        blob, sorted_records = compile_from_input(input_dir)
        write_outputs(blob, sorted_records, output_bin, output_debug)
        print(f"compiled records: {len(sorted_records)}")

    if args.check_determinism:
        blob_a, _ = compile_from_input(input_dir)
        blob_b, _ = compile_from_input(input_dir)
        if blob_a != blob_b:
            print("determinism: failed")
            return 1
        print("determinism: ok")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
