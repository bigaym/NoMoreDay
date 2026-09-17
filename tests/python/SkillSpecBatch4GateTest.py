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
import gen_skill_spec_modifier_contract  # noqa: E402
import migrate_skill_spec_modifier_slice  # noqa: E402
import validate_skill_spec_modifiers  # noqa: E402


# 本批新增的 9 条技能专精修饰器记录（设计 §4.2）。
# 元组 = (modifier_id, skill_id, node_id, opcode, value)。
BATCH4_RECORDS = (
    (2010010, 10, 1001, "SKILL_BONUS_CRIT", 0.02),
    (2010150, 10, 1015, "SKILL_DURATION_FLAT", 0.03),
    (2011010, 11, 1101, "SKILL_AREA_MULT", 0.08),
    (2011070, 11, 1107, "SKILL_AREA_MULT", -0.30),
    (2011190, 11, 1119, "SKILL_DURATION_FLAT", 0.50),
    (2012010, 12, 1201, "SKILL_MORE_DAMAGE_MULT", 0.06),
    (2012070, 12, 1207, "SKILL_MORE_DAMAGE_MULT", 0.10),
    (2012071, 12, 1207, "SKILL_AREA_MULT", -0.40),
    (2012190, 12, 1219, "SKILL_DURATION_FLAT", 0.60),
)

# 本批迁移退役的 8 个 mechanics 键（设计 §4.3）：必须保持退役。
BATCH4_RETIRED_KEYS = (
    (10, 1001, "crit_chance_per_point"),
    (10, 1015, "invulnerable_duration_per_point"),
    (11, 1101, "field_radius_range_per_point"),
    (11, 1107, "field_radius_mult"),
    (11, 1119, "duration_per_point"),
    (12, 1201, "damage_per_point"),
    (12, 1207, "damage_mult"),
    (12, 1219, "duration_per_point"),
)

# literal 迁移键：无 mechanics 源，须由 DEAD_MECHANICS_KEYS 独立守护防回潮。
BATCH4_LITERAL_DEAD_KEYS = (
    (11, 1107, "field_radius_mult"),
    (12, 1207, "damage_mult"),
)

# 已迁移节点/基准下须保留的 mechanics 键（设计 §5.1）。
BATCH4_KEPT_KEYS = (
    (10, 0, "slash_count"),
    (11, 0, "field_damage_per_tier"),
    (11, 0, "base_resist_reduction"),
    (11, 0, "field_pulse_base_damage"),
    (11, 1107, "impact_damage_bonus"),
    (12, 0, "low_life_threshold"),
    (12, 0, "field_duration_per_bloodthirst"),
    (12, 0, "field_radius_per_bloodthirst"),
    (12, 1200, "radius_per_point"),
)


def _canonical_index() -> dict[int, dict]:
    records = validate_skill_spec_modifiers.load_canonical_records()
    return {entry["record"]["modifier_id"]: entry for entry in records}


class SkillSpecBatch4GateTest(unittest.TestCase):
    def test_batch4_records_are_committed(self) -> None:
        index = _canonical_index()
        for record_id, _skill, _node, _opcode, _value in BATCH4_RECORDS:
            self.assertIn(record_id, index, f"缺少 canonical 记录 {record_id}")

    def test_batch4_record_ids_decode_to_skill_node(self) -> None:
        index = _canonical_index()
        seen: set[int] = set()
        for record_id, _skill, node, _opcode, _value in BATCH4_RECORDS:
            self.assertNotIn(record_id, seen, f"记录 {record_id} 重复")
            seen.add(record_id)
            # ID = 2000000 + node*10 + op_index，node 由高位回解，op_index 取个位。
            self.assertEqual((record_id - 2000000) // 10, node)
            # op_index 为 ID 个位，设计仅允许 0..1（同一节点至多两条记录）。
            self.assertIn((record_id - 2000000) % 10, (0, 1))
            self.assertEqual(index[record_id]["runtime"]["node_id_whitelist"], [node])

    def test_batch4_records_match_design_table(self) -> None:
        index = _canonical_index()
        for record_id, skill, _node, opcode, value in BATCH4_RECORDS:
            entry = index[record_id]
            record = entry["record"]
            runtime = entry["runtime"]
            self.assertEqual(runtime["opcode"], opcode, record_id)
            # 契约约束：record.stat_path == runtime.target。
            self.assertEqual(runtime["target"], record["stat_path"], record_id)
            self.assertAlmostEqual(float(record["value"]), value, places=6)
            self.assertAlmostEqual(float(runtime["param_f32"]), value, places=6)
            # 不变量：stacks == runtime.param_u32 == skill_id。
            self.assertEqual(record["stacks"], skill, record_id)
            self.assertEqual(runtime["param_u32"], skill, record_id)
            self.assertEqual(runtime["skill_id_whitelist"], [skill], record_id)
            self.assertEqual(record["conditions"]["all_skill_ids"], [skill], record_id)
            self.assertEqual(record["tags"], ["skill"], record_id)
            # 通用头约束（设计 §4.1）。
            self.assertEqual(runtime["priority"], 200, record_id)
            self.assertEqual(runtime["profession_mask"], 1, record_id)
            self.assertEqual(runtime["weapon_class_mask"], 65535, record_id)
            self.assertEqual(runtime["equip_slot_mask"], 0, record_id)
            self.assertEqual(runtime["exclusive_group"], 0, record_id)
            self.assertEqual(runtime["max_active"], 0, record_id)
            self.assertEqual(runtime["debug_source"], "skill_spec_node", record_id)
            self.assertEqual(record["conditions"]["min_player_level"], 1, record_id)
            # record.operation 必须与迁移切片登记的算子语义一致。
            self.assertEqual(
                record["operation"],
                migrate_skill_spec_modifier_slice.SUPPORTED_OPCODE_TO_OPERATION[opcode],
                record_id,
            )

    def test_batch4_opcodes_registered_in_both_directions(self) -> None:
        # 运行时 OpCode 字典与迁移切片的算子登记必须同时覆盖本批使用的 OpCode。
        for _record_id, _skill, _node, opcode, _value in BATCH4_RECORDS:
            self.assertIn(opcode, gen_modifier_runtime_v2.OPCODE_VALUES)
            self.assertIn(
                opcode,
                migrate_skill_spec_modifier_slice.SUPPORTED_OPCODE_TO_OPERATION,
            )

    def test_runtime_contract_contains_batch4_records(self) -> None:
        runtime_path = (
            REPO_ROOT / "assets" / "data" / "modifier_v2" / "skill_spec_modifiers.json"
        )
        payload = json.loads(runtime_path.read_text(encoding="utf-8"))
        committed = {record["id"] for record in payload["records"]}
        for record_id, _skill, _node, _opcode, _value in BATCH4_RECORDS:
            self.assertIn(record_id, committed, f"运行时契约缺少 {record_id}")

    def test_runtime_contract_synced_with_canonical(self) -> None:
        # canonical -> runtime contract 必须严格同步（无漂移）。
        schema_path = (
            REPO_ROOT
            / "assets"
            / "data"
            / "modifier_v2"
            / "canonical"
            / "skill_spec_modifier_record.schema.json"
        )
        canonical_path = (
            REPO_ROOT
            / "assets"
            / "data"
            / "modifier_v2"
            / "canonical"
            / "skill_spec_modifiers.canonical.json"
        )
        contract_path = (
            REPO_ROOT / "assets" / "data" / "modifier_v2" / "skill_spec_modifiers.json"
        )
        expected = gen_skill_spec_modifier_contract.generate_runtime_document(
            schema=json.loads(schema_path.read_text(encoding="utf-8")),
            canonical_doc=json.loads(canonical_path.read_text(encoding="utf-8")),
        )
        committed = json.loads(contract_path.read_text(encoding="utf-8"))
        self.assertEqual(committed, expected, "canonical 与运行时契约不同步")

    def test_batch4_migration_equivalence_relation(self) -> None:
        # 本批 9 条登记必须精确落位在 MIGRATION_EQUIVALENCE（含 2011190 -> node 1119）。
        registry = {
            (record_id, skill, node): (key, relation, expected)
            for record_id, skill, node, key, relation, expected in (
                validate_skill_spec_modifiers.MIGRATION_EQUIVALENCE
            )
        }
        # opcode -> canonical stat_path 映射（设计 §4.2）。
        _opcode_to_stat = {
            "SKILL_MORE_DAMAGE_MULT": "skill.more_damage",
            "SKILL_BONUS_CRIT": "skill.bonus_crit",
            "SKILL_AREA_MULT": "skill.area_mult",
            "SKILL_DURATION_FLAT": "skill.duration_flat",
        }
        # canonical 索引与循环无关，提取到循环外，避免每条记录重复解析同一文件。
        index = _canonical_index()
        for record_id, skill, node, opcode, value in BATCH4_RECORDS:
            with self.subTest(record_id=record_id):
                self.assertIn((record_id, skill, node), registry, record_id)
                key, relation, expected = registry[(record_id, skill, node)]
                self.assertAlmostEqual(float(expected), value, places=6)
                if key is None:
                    self.assertEqual(relation, "literal", record_id)
                else:
                    self.assertEqual(relation, "raw", record_id)
            self.assertEqual(
                index[record_id]["record"]["stat_path"],
                _opcode_to_stat[opcode],
                record_id,
            )

    def test_batch4_retired_and_dead_keys_absent(self) -> None:
        # 本批 8 个退役键（含 2 个 literal 死键）在机制表中必须完全不存在。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH4_RETIRED_KEYS:
            self.assertNotIn(
                key,
                mechanics.get(str(skill_id), {}).get(str(node_id), {}),
                f"应退役 {skill_id}/{node_id}.{key}",
            )

    def test_batch4_literal_dead_keys_registered(self) -> None:
        # literal 迁移键须独立登记于 DEAD_MECHANICS_KEYS，防止跳过退役检查。
        for literal in BATCH4_LITERAL_DEAD_KEYS:
            self.assertIn(literal, validate_skill_spec_modifiers.DEAD_MECHANICS_KEYS)

    def test_kept_mechanics_keys_remain(self) -> None:
        # 防止删键越界：本批明确保留的键必须仍在机制表中（设计 §5.1）。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH4_KEPT_KEYS:
            self.assertIn(
                key,
                mechanics[str(skill_id)][str(node_id)],
                f"应保留 {skill_id}/{node_id}.{key}",
            )
        for kept in BATCH4_KEPT_KEYS:
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
        target_id = 2012010
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
        # 防回潮：literal 迁移键须被独立死键门禁拦截。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH4_LITERAL_DEAD_KEYS:
            tainted = copy.deepcopy(mechanics)
            tainted.setdefault(str(skill_id), {}).setdefault(str(node_id), {})[key] = 1.0
            self.assertTrue(
                validate_skill_spec_modifiers.check_dead_keys_absent(tainted),
                f"死键 {skill_id}/{node_id}.{key} 回潮未被拦截",
            )

    def test_retired_keys_gate_detects_reintroduction(self) -> None:
        # 迁移退役键回潮：带 canonical 替代记录的键须被 check_retired_keys_absent 拦截。
        mechanics = validate_skill_spec_modifiers.load_skill_mechanics()
        for skill_id, node_id, key in BATCH4_RETIRED_KEYS:
            if (skill_id, node_id, key) in BATCH4_LITERAL_DEAD_KEYS:
                continue  # literal 键无 mechanics 源，由死键门禁用例覆盖。
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
        # 须跳过而非误报失败。
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

    def test_skill_spec_contract_generator_check(self) -> None:
        self._assert_generator_check_passes("gen_skill_spec_modifier_contract.py")

    def test_modifier_runtime_generator_check(self) -> None:
        self._assert_generator_check_passes("gen_modifier_runtime_v2.py")


if __name__ == "__main__":
    unittest.main()
