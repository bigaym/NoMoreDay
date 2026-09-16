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


# 本批新增的 11 条技能专精修饰器记录（设计 §4.2）。
BATCH1_RECORD_IDS = (
    2002000,
    2002001,
    2002010,
    2002020,
    2002100,
    2003000,
    2003020,
    2003100,
    2003120,
    2003310,
    2003320,
)


class SkillSpecBatch1GateTest(unittest.TestCase):
    def test_new_opcodes_registered_in_both_directions(self) -> None:
        expected_opcodes = {
            "SKILL_PROJECTILES_ADD": 37,
            "SKILL_MANA_COST_FLAT": 38,
            "SKILL_BONUS_CRIT_DAMAGE": 39,
            "SKILL_RANGE_MULT": 40,
        }
        for name, value in expected_opcodes.items():
            self.assertEqual(gen_modifier_runtime_v2.OPCODE_VALUES[name], value)

        expected_operations = {
            "SKILL_PROJECTILES_ADD": "add",
            "SKILL_MANA_COST_FLAT": "add",
            "SKILL_BONUS_CRIT_DAMAGE": "add",
            "SKILL_RANGE_MULT": "mul",
        }
        for name, operation in expected_operations.items():
            self.assertEqual(
                migrate_skill_spec_modifier_slice.SUPPORTED_OPCODE_TO_OPERATION[name],
                operation,
            )

    def test_batch1_records_are_committed(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        committed_ids = [entry["record"]["modifier_id"] for entry in records]
        for record_id in BATCH1_RECORD_IDS:
            self.assertIn(record_id, committed_ids)

    def test_record_id_decode_gate(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        self.assertEqual(
            validate_skill_spec_modifiers.check_record_id_decode(records), []
        )

    def test_projectile_value_integer_gate(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        self.assertEqual(
            validate_skill_spec_modifiers.check_projectile_value_integer(records), []
        )

    def test_projectile_value_gate_rejects_negative_and_fractional(self) -> None:
        # 防回归：算子按 static_cast<int>(param_f32 * 点数) 截断，负值会得到负弹道数。
        opcode = validate_skill_spec_modifiers.PROJECTILE_OPCODE

        def synthetic(param_f32: float) -> list[dict]:
            return [
                {
                    "record": {"modifier_id": 2999999},
                    "runtime": {"opcode": opcode, "param_f32": param_f32},
                }
            ]

        check = validate_skill_spec_modifiers.check_projectile_value_integer
        self.assertTrue(check(synthetic(-1.0)), "负 param_f32 必须被拒绝")
        self.assertTrue(check(synthetic(1.5)), "非整数 param_f32 必须被拒绝")
        self.assertEqual(check(synthetic(2.0)), [], "合法值不应报错")

    def test_skill_delivery_domain_gate(self) -> None:
        self.assertEqual(
            validate_skill_spec_modifiers.check_skill_delivery_domain(), []
        )

    def test_migration_equivalence_gate(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        self.assertEqual(
            validate_skill_spec_modifiers.check_migration_equivalence(
                records, mechanics
            ),
            [],
        )

    def test_retired_keys_absent_gate(self) -> None:
        # 退役键（设计 §4.3 删除清单）已在本批从 skill_mechanics.json 移除，本断言用于防回潮。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        self.assertEqual(
            validate_skill_spec_modifiers.check_retired_keys_absent(mechanics), []
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
