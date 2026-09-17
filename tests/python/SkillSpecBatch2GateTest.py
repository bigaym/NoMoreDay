import copy
import json
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = REPO_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

import gen_modifier_runtime_v2  # noqa: E402
import migrate_skill_spec_modifier_slice  # noqa: E402
import validate_skill_spec_modifiers  # noqa: E402


# 本批新增的 19 条技能专精修饰器记录（设计 §4.2）。
# 元组 = (modifier_id, skill_id, node_id, opcode, value)。
BATCH2_RECORDS = (
    (2004020, 4, 402, "SKILL_MANA_COST_MULT", 0.15),
    (2004710, 4, 471, "SKILL_MORE_DAMAGE_MULT", 0.2),
    (2005000, 5, 500, "SKILL_MANA_COST_MULT", 0.1),
    (2005020, 5, 502, "SKILL_MORE_DAMAGE_MULT", 0.1),
    (2005100, 5, 510, "SKILL_MANA_COST_MULT", -0.3),
    (2005110, 5, 511, "SKILL_RANGE_MULT", 0.15),
    (2005111, 5, 511, "SKILL_SPEED_MULT", 0.25),
    (2005330, 5, 533, "SKILL_MORE_DAMAGE_MULT", 1.5),
    (2005540, 5, 554, "SKILL_BONUS_CRIT", 1.0),
    (2005550, 5, 555, "SKILL_BONUS_CRIT_DAMAGE", 0.2),
    (2006000, 6, 600, "SKILL_DURATION_FLAT", 0.5),
    (2006010, 6, 601, "SKILL_AREA_MULT", 0.15),
    (2006020, 6, 602, "SKILL_MORE_DAMAGE_MULT", 0.1),
    (2006030, 6, 603, "SKILL_MANA_COST_MULT", 0.05),
    (2006031, 6, 603, "SKILL_RANGE_MULT", 0.1),
    (2006100, 6, 610, "SKILL_MORE_DAMAGE_MULT", -0.15),
    (2006110, 6, 611, "SKILL_MANA_COST_MULT", -0.3),
    (2006340, 6, 634, "SKILL_AREA_MULT", -0.3),
    (2006530, 6, 653, "SKILL_MORE_DAMAGE_MULT", -0.5),
)

# 本批独立死键（设计 §4.3）：无 canonical 替代记录，必须保持退役（解 G-8）。
BATCH2_DEAD_KEYS = (
    (5, 533, "sword_count_mult"),
    (5, 533, "size_bonus_pct"),
    (5, 533, "impact_radius"),
    (6, 610, "max_arrays_bonus"),
    (6, 611, "max_arrays_bonus"),
)

# 已迁移节点下须保留的 mechanics 键（设计 §6.3）。
BATCH2_KEPT_KEYS = (
    (4, 470, "counter_swords"),
    (5, 502, "splash_radius"),
    (5, 510, "lock_range"),
    (5, 533, "giant_radius"),
    (5, 554, "intent_cost"),
)


def _canonical_index() -> dict[int, dict]:
    records = validate_skill_spec_modifiers.load_canonical_records()
    return {entry["record"]["modifier_id"]: entry for entry in records}


class SkillSpecBatch2GateTest(unittest.TestCase):
    def test_new_opcodes_registered_in_both_directions(self) -> None:
        expected_opcodes = {
            "SKILL_DURATION_FLAT": 41,
            "SKILL_SPEED_MULT": 42,
        }
        for name, value in expected_opcodes.items():
            self.assertEqual(gen_modifier_runtime_v2.OPCODE_VALUES[name], value)

        expected_operations = {
            "SKILL_DURATION_FLAT": "add",
            "SKILL_SPEED_MULT": "mul",
        }
        for name, operation in expected_operations.items():
            self.assertEqual(
                migrate_skill_spec_modifier_slice.SUPPORTED_OPCODE_TO_OPERATION[name],
                operation,
            )

    def test_skill_delivery_opcode_max_covers_batch2(self) -> None:
        self.assertEqual(validate_skill_spec_modifiers.SKILL_DELIVERY_OPCODE_MAX, 42)
        for _record_id, _skill, _node, opcode, _value in BATCH2_RECORDS:
            self.assertLessEqual(
                gen_modifier_runtime_v2.OPCODE_VALUES[opcode],
                validate_skill_spec_modifiers.SKILL_DELIVERY_OPCODE_MAX,
            )

    def test_batch2_records_are_committed(self) -> None:
        index = _canonical_index()
        for record_id, _skill, _node, _opcode, _value in BATCH2_RECORDS:
            self.assertIn(record_id, index, f"缺少 canonical 记录 {record_id}")

    def test_batch2_record_ids_decode_to_skill_node(self) -> None:
        index = _canonical_index()
        seen: set[int] = set()
        for record_id, _skill, node, _opcode, _value in BATCH2_RECORDS:
            self.assertNotIn(record_id, seen, f"记录 {record_id} 重复")
            seen.add(record_id)
            # ID = 2000000 + node*10 + op_index，node 由高位回解，op_index 取个位。
            self.assertEqual((record_id - 2000000) // 10, node)
            # op_index 为 ID 个位，设计仅允许 0..1（同一节点至多两条记录）。
            self.assertIn((record_id - 2000000) % 10, (0, 1))
            self.assertEqual(index[record_id]["runtime"]["node_id_whitelist"], [node])

    def test_batch2_records_match_design_table(self) -> None:
        index = _canonical_index()
        for record_id, skill, _node, opcode, value in BATCH2_RECORDS:
            entry = index[record_id]
            record = entry["record"]
            runtime = entry["runtime"]
            self.assertEqual(runtime["opcode"], opcode, record_id)
            self.assertEqual(runtime["target"], record["stat_path"], record_id)
            self.assertAlmostEqual(float(record["value"]), value, places=6)
            self.assertAlmostEqual(float(runtime["param_f32"]), value, places=6)
            # 不变量：stacks == runtime.param_u32 == skill_id。
            self.assertEqual(record["stacks"], skill, record_id)
            self.assertEqual(runtime["param_u32"], skill, record_id)
            self.assertEqual(runtime["skill_id_whitelist"], [skill], record_id)
            self.assertEqual(record["conditions"]["all_skill_ids"], [skill], record_id)
            self.assertEqual(record["tags"], ["skill"], record_id)
            self.assertEqual(
                record["operation"],
                migrate_skill_spec_modifier_slice.SUPPORTED_OPCODE_TO_OPERATION[opcode],
                record_id,
            )

    def test_runtime_contract_contains_batch2_records(self) -> None:
        runtime_path = REPO_ROOT / "assets" / "data" / "modifier_v2" / "skill_spec_modifiers.json"
        payload = json.loads(runtime_path.read_text(encoding="utf-8"))
        committed = {record["id"] for record in payload["records"]}
        for record_id, _skill, _node, _opcode, _value in BATCH2_RECORDS:
            self.assertIn(record_id, committed, f"运行时契约缺少 {record_id}")

    def test_record_id_decode_gate(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        self.assertEqual(
            validate_skill_spec_modifiers.check_record_id_decode(records), []
        )

    def test_skill_delivery_domain_gate(self) -> None:
        self.assertEqual(
            validate_skill_spec_modifiers.check_skill_delivery_domain(), []
        )

    def test_migration_equivalence_gate(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        self.assertEqual(
            validate_skill_spec_modifiers.check_migration_equivalence(records, mechanics),
            [],
        )

    def test_retired_keys_absent_gate(self) -> None:
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        self.assertEqual(
            validate_skill_spec_modifiers.check_retired_keys_absent(mechanics), []
        )

    def test_kept_mechanics_keys_remain(self) -> None:
        # 防止删键越界：本批明确保留的键必须仍在机制表中（设计 §6.3）。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH2_KEPT_KEYS:
            self.assertIn(
                key,
                mechanics[str(skill_id)][str(node_id)],
                f"应保留 {skill_id}/{node_id}.{key}",
            )
        for kept in BATCH2_KEPT_KEYS:
            self.assertIn(kept, validate_skill_spec_modifiers.KEPT_MECHANICS_KEYS)

    def test_dead_keys_absent_gate(self) -> None:
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        self.assertEqual(
            validate_skill_spec_modifiers.check_dead_keys_absent(mechanics), []
        )

    def test_dead_keys_gate_detects_reintroduction(self) -> None:
        # 防回潮：check_retired_keys_absent 不覆盖无 canonical 的死键，独立门禁须能拦截。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH2_DEAD_KEYS:
            tainted = copy.deepcopy(mechanics)
            tainted[str(skill_id)][str(node_id)][key] = 1.0
            self.assertTrue(
                validate_skill_spec_modifiers.check_dead_keys_absent(tainted),
                f"死键 {skill_id}/{node_id}.{key} 回潮未被拦截",
            )

    def test_registry_reverse_gate(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        self.assertEqual(
            validate_skill_spec_modifiers.check_registry_reverse(records, mechanics),
            [],
        )


if __name__ == "__main__":
    unittest.main()
