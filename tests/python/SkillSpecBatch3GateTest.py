import copy
import json
import subprocess
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


# 本批新增的 11 条技能专精修饰器记录（设计 §4.3 / §4.4）。
# 元组 = (modifier_id, skill_id, node_id, opcode, value)。
BATCH3_RECORDS = (
    (2007000, 7, 700, "SKILL_MANA_COST_MULT", 0.1),
    (2007010, 7, 701, "SKILL_MORE_DAMAGE_MULT", 0.1),
    (2007020, 7, 702, "SKILL_AREA_MULT", 0.1),
    (2007030, 7, 703, "SKILL_RANGE_MULT", 0.1),
    (2007320, 7, 732, "SKILL_MANA_COST_MULT", -0.5),
    (2008000, 8, 800, "SKILL_MANA_COST_FLAT", -1.0),
    (2008010, 8, 801, "SKILL_SPEED_MULT", 0.15),
    (2008011, 8, 801, "SKILL_RANGE_MULT", 0.15),
    (2008100, 8, 810, "SKILL_DURATION_FLAT", 0.8),
    (2009750, 9, 975, "SKILL_DURATION_FLAT", 0.25),
    (2009860, 9, 986, "SKILL_COOLDOWN_FLAT", -1.0),
)

# 本批带 mechanics 源的 8 条退役键（设计 §4.3 / §4.4）：必须保持退役。
BATCH3_RETIRED_KEYS = (
    (7, 700, "mana_reduction_pct_per_point"),
    (7, 701, "phys_damage_pct_per_point"),
    (7, 702, "radius_pct_per_point"),
    (7, 703, "range_pct_per_point"),
    (7, 732, "mana_penalty_pct"),
    (8, 810, "hover_duration"),
    (9, 975, "duration_per_point"),
    (9, 986, "cd_per_point"),
)

# 本批独立死键（设计 §4.4）：无 canonical 替代记录，必须保持退役。
BATCH3_DEAD_KEYS = (
    (8, 810, "hover_tick_interval"),
    (9, 0, "form_move_pct"),
    (9, 0, "weaken_duration"),
)

# 已迁移节点/基准下须保留的 mechanics 键（设计 §4.4）。
BATCH3_KEPT_KEYS = (
    (7, 0, "base_radius"),
    (7, 0, "base_range"),
    (7, 0, "mana_cost_per_sec"),
    (7, 732, "move_speed_scale"),
    (9, 0, "form_duration"),
)


def _canonical_index() -> dict[int, dict]:
    records = validate_skill_spec_modifiers.load_canonical_records()
    return {entry["record"]["modifier_id"]: entry for entry in records}


class SkillSpecBatch3GateTest(unittest.TestCase):
    def test_batch3_records_are_committed(self) -> None:
        index = _canonical_index()
        for record_id, _skill, _node, _opcode, _value in BATCH3_RECORDS:
            self.assertIn(record_id, index, f"缺少 canonical 记录 {record_id}")

    def test_batch3_record_ids_decode_to_skill_node(self) -> None:
        index = _canonical_index()
        seen: set[int] = set()
        for record_id, _skill, node, _opcode, _value in BATCH3_RECORDS:
            self.assertNotIn(record_id, seen, f"记录 {record_id} 重复")
            seen.add(record_id)
            # ID = 2000000 + node*10 + op_index，node 由高位回解，op_index 取个位。
            self.assertEqual((record_id - 2000000) // 10, node)
            # op_index 为 ID 个位，设计仅允许 0..1（同一节点至多两条记录）。
            self.assertIn((record_id - 2000000) % 10, (0, 1))
            self.assertEqual(index[record_id]["runtime"]["node_id_whitelist"], [node])

    def test_batch3_records_match_design_table(self) -> None:
        index = _canonical_index()
        for record_id, skill, _node, opcode, value in BATCH3_RECORDS:
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
            # record.operation 必须与迁移切片登记的算子语义一致。
            self.assertEqual(
                record["operation"],
                migrate_skill_spec_modifier_slice.SUPPORTED_OPCODE_TO_OPERATION[opcode],
                record_id,
            )

    def test_batch3_opcodes_registered_in_both_directions(self) -> None:
        # 运行时 OpCode 字典与迁移切片的算子登记必须同时覆盖本批使用的 OpCode。
        for _record_id, _skill, _node, opcode, _value in BATCH3_RECORDS:
            self.assertIn(opcode, gen_modifier_runtime_v2.OPCODE_VALUES)
            self.assertIn(
                opcode,
                migrate_skill_spec_modifier_slice.SUPPORTED_OPCODE_TO_OPERATION,
            )

    def test_runtime_contract_contains_batch3_records(self) -> None:
        runtime_path = (
            REPO_ROOT / "assets" / "data" / "modifier_v2" / "skill_spec_modifiers.json"
        )
        payload = json.loads(runtime_path.read_text(encoding="utf-8"))
        committed = {record["id"] for record in payload["records"]}
        for record_id, _skill, _node, _opcode, _value in BATCH3_RECORDS:
            self.assertIn(record_id, committed, f"运行时契约缺少 {record_id}")

    def test_batch3_retired_and_dead_keys_absent(self) -> None:
        # 本批 8 个退役键 + 3 个死键在机制表中必须完全不存在。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH3_RETIRED_KEYS + BATCH3_DEAD_KEYS:
            self.assertNotIn(
                key,
                mechanics.get(str(skill_id), {}).get(str(node_id), {}),
                f"应退役 {skill_id}/{node_id}.{key}",
            )

    def test_kept_mechanics_keys_remain(self) -> None:
        # 防止删键越界：本批明确保留的键必须仍在机制表中（设计 §4.4）。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH3_KEPT_KEYS:
            self.assertIn(
                key,
                mechanics[str(skill_id)][str(node_id)],
                f"应保留 {skill_id}/{node_id}.{key}",
            )
        for kept in BATCH3_KEPT_KEYS:
            self.assertIn(kept, validate_skill_spec_modifiers.KEPT_MECHANICS_KEYS)

    def test_all_six_offline_gates_pass(self) -> None:
        # 逐项调用 6 个门禁函数，any failure 即视为离线门禁失守。
        records = validate_skill_spec_modifiers.load_canonical_records()
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        gates = {
            "id_decode": validate_skill_spec_modifiers.check_record_id_decode(records),
            "projectile_integer": (
                validate_skill_spec_modifiers.check_projectile_value_integer(records)
            ),
            "skill_delivery_domain": (
                validate_skill_spec_modifiers.check_skill_delivery_domain()
            ),
            "migration_equivalence": (
                validate_skill_spec_modifiers.check_migration_equivalence(
                    records, mechanics
                )
            ),
            "retired_keys_absent": (
                validate_skill_spec_modifiers.check_retired_keys_absent(mechanics)
                + validate_skill_spec_modifiers.check_dead_keys_absent(mechanics)
            ),
            "registry_reverse": (
                validate_skill_spec_modifiers.check_registry_reverse(records, mechanics)
            ),
        }
        for name, failures in gates.items():
            with self.subTest(gate=name):
                self.assertEqual(failures, [], f"门禁 {name} 存在失败项")

    def test_record_id_decode_gate(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        self.assertEqual(
            validate_skill_spec_modifiers.check_record_id_decode(records), []
        )

    def test_record_id_decode_gate_detects_duplicate_id(self) -> None:
        # 重复 modifier_id 会被 _canonical_index 静默覆盖，门禁须显式拦截。
        records = validate_skill_spec_modifiers.load_canonical_records()
        tainted = records + [copy.deepcopy(records[0])]
        failures = validate_skill_spec_modifiers.check_record_id_decode(tainted)
        self.assertTrue(failures, "重复 modifier_id 未被检出")
        self.assertTrue(
            any("duplicate modifier_id" in failure for failure in failures),
            f"未给出重复 modifier_id 提示：{failures}",
        )

    def test_projectile_integer_gate(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        self.assertEqual(
            validate_skill_spec_modifiers.check_projectile_value_integer(records), []
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

    def test_migration_equivalence_gate_detects_value_drift(self) -> None:
        # 防数值漂移：篡改 canonical 副本的 value 后，冻结期望值校验须报错并指向该记录。
        records = validate_skill_spec_modifiers.load_canonical_records()
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        tainted = copy.deepcopy(records)
        target_id = 2007000
        for entry in tainted:
            if entry["record"]["modifier_id"] == target_id:
                entry["record"]["value"] = 0.11
                break
        else:
            self.fail(f"canonical 缺少记录 {target_id}")
        failures = validate_skill_spec_modifiers.check_migration_equivalence(
            tainted, mechanics
        )
        self.assertTrue(failures, "canonical 数值漂移未被检出")
        self.assertTrue(
            any(str(target_id) in failure for failure in failures),
            f"数值漂移未指向记录 {target_id}：{failures}",
        )

    def test_retired_keys_absent_gate(self) -> None:
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        self.assertEqual(
            validate_skill_spec_modifiers.check_retired_keys_absent(mechanics), []
        )

    def test_dead_keys_absent_gate(self) -> None:
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        self.assertEqual(
            validate_skill_spec_modifiers.check_dead_keys_absent(mechanics), []
        )

    def test_dead_keys_gate_detects_reintroduction(self) -> None:
        # 防回潮：check_retired_keys_absent 不覆盖无 canonical 的死键，独立门禁须能拦截。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH3_DEAD_KEYS:
            tainted = copy.deepcopy(mechanics)
            tainted.setdefault(str(skill_id), {}).setdefault(str(node_id), {})[key] = 1.0
            self.assertTrue(
                validate_skill_spec_modifiers.check_dead_keys_absent(tainted),
                f"死键 {skill_id}/{node_id}.{key} 回潮未被拦截",
            )

    def test_retired_keys_gate_detects_reintroduction(self) -> None:
        # 迁移退役键回潮：带 canonical 替代记录的键须被 check_retired_keys_absent 拦截。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH3_RETIRED_KEYS:
            tainted = copy.deepcopy(mechanics)
            tainted.setdefault(str(skill_id), {}).setdefault(str(node_id), {})[key] = 1.0
            self.assertTrue(
                validate_skill_spec_modifiers.check_retired_keys_absent(tainted),
                f"退役键 {skill_id}/{node_id}.{key} 回潮未被拦截",
            )

    def test_registry_reverse_gate(self) -> None:
        records = validate_skill_spec_modifiers.load_canonical_records()
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        self.assertEqual(
            validate_skill_spec_modifiers.check_registry_reverse(records, mechanics),
            [],
        )

    def _assert_generator_check_passes(self, script_name: str) -> None:
        # 以子进程运行生成器 --check，断言退出码为 0（漂移时生成器返回 1）。
        # gen_modifier_runtime_v2.py 的 --check 比对的是 gitignore 的本地产物
        # assets/generated/modifier_runtime_v2.bin：干净检出场下产物不存在，
        # 须跳过而非误报失败（本文件其余用例均为纯 Python 读取，无需构建）。
        if script_name == "gen_modifier_runtime_v2.py":
            artifact = REPO_ROOT / "assets" / "generated" / "modifier_runtime_v2.bin"
            if not artifact.exists():
                self.skipTest(f"{artifact} 不存在（未构建），跳过生成器 --check")
        completed = subprocess.run(
            [sys.executable, str(SCRIPTS_DIR / script_name), "--check"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            completed.returncode,
            0,
            f"{script_name} --check 退出码 {completed.returncode}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}",
        )

    def test_modifier_runtime_generator_check(self) -> None:
        self._assert_generator_check_passes("gen_modifier_runtime_v2.py")

    def test_skill_mechanics_schema_generator_check(self) -> None:
        self._assert_generator_check_passes("gen_skill_mechanics_schema.py")


if __name__ == "__main__":
    unittest.main()
