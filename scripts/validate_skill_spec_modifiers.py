"""技能专精修饰器离线门禁（UMR-SKILL-BATCH-1 设计 §4.1 / §4.3 / §5.1-5）。

汇总 6 项断言，任一失败即以非 0 退出；也可由 SkillSpecBatch1GateTest 按项调用：
  1. 记录 ID 解码：(id - 2_000_000) / 10 == node_id_whitelist[0]（历史例外 2002103）；
  2. SKILL_PROJECTILES_ADD 的 param_f32 必须为非负整数（整数算子静态截断与负值防护）；
  3. OpCode 30..40 仅允许出现在 debug_source == "skill_spec_node" 的记录上（SkillDelivery 类别防蔓延）；
  4. canonical ↔ skill_mechanics 迁移等价（按算子逐条给出期望关系，节点 201 为绝对平减不套用 /100）；
  5. 退役键不回潮（覆盖设计 §4.3 删除清单）；
  6. 反向登记：退役键必须有 canonical 替代记录，且已迁移节点下不得残留未登记键。
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any

import gen_modifier_runtime_v2


REPO_ROOT = Path(__file__).resolve().parent.parent
MODIFIER_V2_DIR = REPO_ROOT / "assets" / "data" / "modifier_v2"
CANONICAL_PATH = (
    MODIFIER_V2_DIR / "canonical" / "skill_spec_modifiers.canonical.json"
)
MODIFIER_CATALOG_PATH = MODIFIER_V2_DIR / "modifier_catalog.json"
SKILL_MECHANICS_PATH = REPO_ROOT / "assets" / "data" / "skill_mechanics.json"

ID_BASE = 2_000_000
ID_STRIDE = 10
# 历史例外：2002103 的白名单实为 node 213，按设计 §4.1 作为唯一豁免登记。
ID_DECODE_EXEMPT_IDS = frozenset({2002103})

PROJECTILE_OPCODE = "SKILL_PROJECTILES_ADD"
SKILL_DELIVERY_OPCODE_MIN = 30
SKILL_DELIVERY_OPCODE_MAX = 40
SKILL_SPEC_DEBUG_SOURCE = "skill_spec_node"

# canonical ↔ skill_mechanics 迁移等价登记表（设计 §4.3）。
# relation: "percent" -> canonical = mechanics / 100；"flat_negate" -> canonical = -mechanics；
#           "raw" -> canonical = mechanics（无 /100）；"literal" -> 无 mechanics 源（Baker 字面量
#           迁移），此时 mechanics_key 为 None。
# expected_value 为设计冻结常量，即便 mechanics 键退役后仍持续校验 canonical 数值不漂移。
MIGRATION_EQUIVALENCE = (
    # (record_id, skill_id, node_id, mechanics_key, relation, expected_value)
    (2002000, 2, 200, "area_radius_pct_per_point", "percent", 0.1),
    (2002001, 2, 200, "range_pct_per_point", "percent", 0.1),
    (2002010, 2, 201, "mana_reduction_per_point", "flat_negate", -1.0),
    (2002020, 2, 202, "phys_damage_pct_per_point", "percent", 0.1),
    # 210 的投射物增量为 Baker 字面量迁移，mechanics 只保留非线性惩罚键。
    (2002100, 2, 210, None, "literal", 1.0),
    (2003000, 3, 300, "swords_per_point", "raw", 1.0),
    (2003020, 3, 302, "phys_damage_pct_per_point", "percent", 0.1),
    (2003100, 3, 310, "range_pct_per_point", "percent", 0.2),
    (2003120, 3, 312, "cost_reduction_pct_per_point", "percent", 0.05),
    (2003310, 3, 331, "crit_chance_per_point", "percent", 0.05),
    (2003320, 3, 332, "crit_damage_per_point", "percent", 0.25),
)

# 已迁移节点下仍保留的 mechanics 键（非线性后处理或本批显式不迁移），用于反向登记校验。
KEPT_MECHANICS_KEYS = frozenset(
    {
        (2, 210, "damage_penalty_pt1"),
        (2, 210, "damage_penalty_pt2"),
        (2, 210, "damage_penalty_pt3"),
        (3, 300, "duration"),
        (3, 312, "mana_regen_per_sword_per_point"),
        (3, 331, "splash_radius"),
    }
)

FLOAT_TOLERANCE = 1e-6


def _load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def _is_close(actual: float, expected: float) -> bool:
    return math.isclose(actual, expected, rel_tol=0.0, abs_tol=FLOAT_TOLERANCE)


def _skill_delivery_opcode_names() -> set[str]:
    # 直接复用运行时生成器的 OpCode 字典，避免两处字典漂移导致门禁漏检。
    return {
        name
        for name, value in gen_modifier_runtime_v2.OPCODE_VALUES.items()
        if SKILL_DELIVERY_OPCODE_MIN <= value <= SKILL_DELIVERY_OPCODE_MAX
    }


def load_canonical_records() -> list[dict[str, Any]]:
    document = _load_json(CANONICAL_PATH)
    records = document.get("records")
    if not isinstance(records, list):
        raise ValueError("canonical.records must be a list")
    return records


def load_skill_mechanics() -> dict[str, Any]:
    mechanics = _load_json(SKILL_MECHANICS_PATH)
    if not isinstance(mechanics, dict):
        raise ValueError("skill_mechanics.json must be an object")
    return mechanics


def check_record_id_decode(
    records: list[dict[str, Any]],
    exempt_ids: frozenset[int] = ID_DECODE_EXEMPT_IDS,
) -> list[str]:
    """断言记录 ID 解码结果等于 node_id_whitelist[0]。"""
    failures: list[str] = []
    for entry in records:
        record = entry.get("record", {})
        runtime = entry.get("runtime", {})
        record_id = record.get("modifier_id")
        if record_id in exempt_ids:
            continue
        node_ids = runtime.get("node_id_whitelist")
        if not isinstance(record_id, int) or isinstance(record_id, bool):
            failures.append("record missing integer modifier_id")
            continue
        if not isinstance(node_ids, list) or not node_ids:
            failures.append(f"record {record_id}: node_id_whitelist must be non-empty")
            continue
        decoded = (record_id - ID_BASE) // ID_STRIDE
        if decoded != node_ids[0]:
            failures.append(
                f"record {record_id}: decoded node {decoded} != "
                f"node_id_whitelist[0] {node_ids[0]}"
            )
    return failures


def check_projectile_value_integer(
    records: list[dict[str, Any]],
) -> list[str]:
    """断言 SKILL_PROJECTILES_ADD 的 param_f32 为非负整数值。"""
    failures: list[str] = []
    for entry in records:
        runtime = entry.get("runtime", {})
        if runtime.get("opcode") != PROJECTILE_OPCODE:
            continue
        value = runtime.get("param_f32")
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            failures.append(
                f"record {entry.get('record', {}).get('modifier_id')}: "
                f"{PROJECTILE_OPCODE} param_f32 must be numeric"
            )
            continue
        # 算子按 static_cast<int>(param_f32 * 点数) 截断，负值会产生负弹道数。
        if float(value) < 0:
            failures.append(
                f"record {entry.get('record', {}).get('modifier_id')}: "
                f"{PROJECTILE_OPCODE} param_f32 {value} must be non-negative"
            )
        elif not float(value).is_integer():
            failures.append(
                f"record {entry.get('record', {}).get('modifier_id')}: "
                f"{PROJECTILE_OPCODE} param_f32 {value} must be an integer"
            )
    return failures


def check_skill_delivery_domain(
    catalog_path: Path = MODIFIER_CATALOG_PATH,
) -> list[str]:
    """断言 OpCode 30..40 仅出现在 debug_source == skill_spec_node 的记录上。"""
    failures: list[str] = []
    opcode_names = _skill_delivery_opcode_names()
    catalog = _load_json(catalog_path)
    entries = catalog.get("entries", [])
    if not isinstance(entries, list):
        raise ValueError("modifier catalog entries must be a list")
    for entry in entries:
        relative_path = entry.get("path")
        if not isinstance(relative_path, str):
            continue
        document = _load_json((catalog_path.parent / relative_path).resolve())
        records = document.get("records", [])
        if not isinstance(records, list):
            continue
        for record in records:
            ops = record.get("ops", [])
            if not isinstance(ops, list):
                continue
            source = record.get("debug", {}).get("source")
            for op in ops:
                opcode = op.get("opcode")
                if opcode in opcode_names and source != SKILL_SPEC_DEBUG_SOURCE:
                    failures.append(
                        f"{relative_path} record {record.get('id')}: opcode "
                        f"{opcode} requires debug.source='{SKILL_SPEC_DEBUG_SOURCE}' "
                        f"(got {source!r})"
                    )
    return failures


def _canonical_index(
    records: list[dict[str, Any]],
) -> dict[int, dict[str, Any]]:
    index: dict[int, dict[str, Any]] = {}
    for entry in records:
        record = entry.get("record", {})
        runtime = entry.get("runtime", {})
        record_id = record.get("modifier_id")
        node_ids = runtime.get("node_id_whitelist")
        skill_ids = runtime.get("skill_id_whitelist")
        index[record_id] = {
            # 归属技能以 runtime 白名单为准：record.stacks 是叠层数（迁移期恰好
            # 等于交付算子的 param_u32=技能 id），对非交付记录会退化为目标 stat id。
            "skill_id": skill_ids[0] if isinstance(skill_ids, list) and skill_ids else None,
            "node_id": node_ids[0] if isinstance(node_ids, list) and node_ids else None,
            "value": record.get("value"),
        }
    return index


def _mechanics_node(
    mechanics: dict[str, Any], skill_id: int, node_id: int
) -> dict[str, Any]:
    skill_table = mechanics.get(str(skill_id), {})
    if not isinstance(skill_table, dict):
        return {}
    node_table = skill_table.get(str(node_id), {})
    return node_table if isinstance(node_table, dict) else {}


def check_migration_equivalence(
    records: list[dict[str, Any]],
    mechanics: dict[str, Any],
) -> list[str]:
    """断言 canonical 数值与 skill_mechanics 迁移源在共存期严格等价。

    mechanics 键退役后不再存在，此时该键的缺失由 check_retired_keys_absent 负责；
    这里始终用设计冻结常量校验 canonical 数值，保证退役后门禁不空转。
    """
    failures: list[str] = []
    index = _canonical_index(records)
    for record_id, skill_id, node_id, key, relation, expected in MIGRATION_EQUIVALENCE:
        entry = index.get(record_id)
        if entry is None:
            failures.append(f"record {record_id} missing from canonical")
            continue
        if entry["skill_id"] != skill_id or entry["node_id"] != node_id:
            failures.append(
                f"record {record_id} registry mismatch: expected skill/node "
                f"{skill_id}/{node_id}, got {entry['skill_id']}/{entry['node_id']}"
            )
            continue
        value = entry["value"]
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            failures.append(f"record {record_id}: value must be numeric")
            continue
        if not _is_close(float(value), expected):
            failures.append(
                f"record {record_id}: canonical value {value} != frozen expected "
                f"{expected}"
            )
            continue
        if key is None:
            continue  # literal 迁移无 mechanics 源，仅由上面的冻结常量守护。
        node_table = _mechanics_node(mechanics, skill_id, node_id)
        if key in node_table:
            mechanics_value = node_table[key]
            if not isinstance(mechanics_value, (int, float)) or isinstance(
                mechanics_value, bool
            ):
                failures.append(
                    f"mechanics {skill_id}/{node_id}.{key} must be numeric"
                )
                continue
            if relation == "percent":
                expected_from_mechanics = mechanics_value / 100.0
            elif relation == "flat_negate":
                expected_from_mechanics = -mechanics_value
            elif relation == "raw":
                expected_from_mechanics = mechanics_value
            else:  # pragma: no cover - registry 只登记已定义关系
                failures.append(f"unknown migration relation {relation!r}")
                continue
            if not _is_close(float(value), float(expected_from_mechanics)):
                failures.append(
                    f"record {record_id}: canonical {value} not equivalent to "
                    f"mechanics {skill_id}/{node_id}.{key}={mechanics_value} "
                    f"({relation})"
                )
    return failures


def check_retired_keys_absent(mechanics: dict[str, Any]) -> list[str]:
    """断言退役键不再出现于 skill_mechanics.json（防回潮）。"""
    failures: list[str] = []
    for record_id, skill_id, node_id, key, _relation, _expected in (
        MIGRATION_EQUIVALENCE
    ):
        if key is None:
            continue  # literal 迁移无退役键。
        node_table = _mechanics_node(mechanics, skill_id, node_id)
        if key in node_table:
            failures.append(
                f"retired mechanics key still present: {skill_id}/{node_id}.{key} "
                f"(replaced by canonical record {record_id})"
            )
    return failures


def check_registry_reverse(
    records: list[dict[str, Any]],
    mechanics: dict[str, Any],
) -> list[str]:
    """反向登记：退役键必须有 canonical 替代记录，且已迁移节点不得出现未登记键。"""
    failures: list[str] = []
    index = _canonical_index(records)
    retired_by_node: dict[tuple[int, int], set[str]] = {}
    for record_id, skill_id, node_id, key, _relation, _expected in (
        MIGRATION_EQUIVALENCE
    ):
        # literal 迁移（key=None）不产生退役键，但仍登记该已迁移节点，
        # 以便下方对残留的未登记键报错。
        retired_by_node.setdefault((skill_id, node_id), set())
        if key is not None:
            retired_by_node[(skill_id, node_id)].add(key)
        entry = index.get(record_id)
        source = (
            f"{skill_id}/{node_id}.{key}"
            if key is not None
            else f"literal skill/node {skill_id}/{node_id}"
        )
        if entry is None:
            failures.append(f"migration source {source} has no canonical record {record_id}")
        elif entry["skill_id"] != skill_id or entry["node_id"] != node_id:
            failures.append(
                f"canonical record {record_id} does not match source {source}"
            )

    for (skill_id, node_id), retired_keys in retired_by_node.items():
        node_table = _mechanics_node(mechanics, skill_id, node_id)
        for key in node_table:
            if key in retired_keys:
                continue
            if (skill_id, node_id, key) in KEPT_MECHANICS_KEYS:
                continue
            failures.append(
                f"unregistered mechanics key under migrated node: "
                f"{skill_id}/{node_id}.{key}"
            )
    return failures


def run_all_checks() -> dict[str, list[str]]:
    records = load_canonical_records()
    mechanics = load_skill_mechanics()
    return {
        "id_decode": check_record_id_decode(records),
        "projectile_integer": check_projectile_value_integer(records),
        "skill_delivery_domain": check_skill_delivery_domain(),
        "migration_equivalence": check_migration_equivalence(records, mechanics),
        "retired_keys_absent": check_retired_keys_absent(mechanics),
        "registry_reverse": check_registry_reverse(records, mechanics),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Validate skill_spec modifier offline gates (id decode, projectile "
            "integer, SkillDelivery domain, migration equivalence, retired keys, "
            "registry reverse)."
        ),
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="门禁校验（默认行为，保留该参数以对齐其他生成器习惯）。",
    )
    parser.parse_args()

    results = run_all_checks()
    failed = False
    for name, failures in results.items():
        if failures:
            failed = True
            print(f"[FAIL] {name}: {len(failures)} issue(s)")
            for failure in failures:
                print(f"  - {failure}")
        else:
            print(f"[OK] {name}")
    if failed:
        print("[FAIL] skill_spec modifier offline gates detected violations.")
        return 1
    print("[OK] skill_spec modifier offline gates passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
