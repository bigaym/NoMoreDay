#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/FlowingThrustComponents.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/physics/SpatialGrid.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorBase.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/behaviors/FlowingThrust.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"
#include "raylib.h"

namespace NoMoreDay {

namespace {

// 技能交付算子由生成的 UMR 运行时二进制提供。同进程内其他用例可能经
// LoadFromBytes 注入合成数据（此后 EnsureLoaded 视为通配来源不再重载），
// 故烘焙断言前显式重载真实产物，避免跨用例状态泄漏。
void EnsureModifierRuntimeForSkillSpec() {
  // 复用 TestCommon 的单一资产路径来源，同时保留资产缺失/解析失败即硬失败。
  REQUIRE(ReloadModifierRuntimeFromAsset());
}

template <typename T>
void AppendRuntimeStruct(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

// 合成单记录运行时二进制：同一技能同时携带加法平减与乘法折扣两个法耗算子，
// 用于锁定「先加性、后乘性」的确定性结算顺序（真实资产暂无同时携带两者的技能）。
// flat_delta 为每点绝对平减（负值即减免）；mult_discount_per_point 为每点折扣率，
// 乘性系数由算子计算为 max(0, 1 - 折扣率 * 点数)。
std::vector<uint8_t> BuildManaRuntimeBlob(float flat_delta,
                                          float mult_discount_per_point) {
  ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 2;
  header.index_count = 0;
  header.records_offset = sizeof(ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(ModifierRuntimeRecord);
  header.ops_offset = header.filters_offset + sizeof(ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + 2u * sizeof(ModifierRuntimeOp);
  header.crc32 = 0;

  ModifierRuntimeRecord record;
  record.id = 7101u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 2;

  ModifierRuntimeFilter filter; // 空白名单：技能与节点均按通配处理

  ModifierRuntimeOp flatOp;
  flatOp.opcode =
      static_cast<uint16_t>(ModifierOpCode::SKILL_MANA_COST_FLAT);
  flatOp.param_u32 = 2u;
  flatOp.param_f32 = flat_delta;

  ModifierRuntimeOp multOp;
  multOp.opcode =
      static_cast<uint16_t>(ModifierOpCode::SKILL_MANA_COST_MULT);
  multOp.param_u32 = 2u;
  multOp.param_f32 = mult_discount_per_point;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) +
               2u * sizeof(ModifierRuntimeOp));
  AppendRuntimeStruct(blob, header);
  AppendRuntimeStruct(blob, record);
  AppendRuntimeStruct(blob, filter);
  AppendRuntimeStruct(blob, flatOp);
  AppendRuntimeStruct(blob, multOp);
  return blob;
}

// 合成单记录运行时二进制：同一技能同时携带持续时间加性与弹速乘性两个交付算子，
// 用于验证负向每点参数经线性外推后产生的非法交付参数会被 Baker 端 max(0) 钳制。
// duration_flat_per_point 为每点绝对秒数（负值即缩短）；speed_mult_per_point 为每点
// 乘性偏移，系数由算子计算为 1 + 偏移率 * 点数（可为负）。
std::vector<uint8_t> BuildDeliveryRuntimeBlob(float duration_flat_per_point,
                                              float speed_mult_per_point) {
  ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 2;
  header.index_count = 0;
  header.records_offset = sizeof(ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(ModifierRuntimeRecord);
  header.ops_offset = header.filters_offset + sizeof(ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + 2u * sizeof(ModifierRuntimeOp);
  header.crc32 = 0;

  ModifierRuntimeRecord record;
  record.id = 6201u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 2;

  ModifierRuntimeFilter filter; // 空白名单：技能与节点均按通配处理

  ModifierRuntimeOp durationOp;
  durationOp.opcode =
      static_cast<uint16_t>(ModifierOpCode::SKILL_DURATION_FLAT);
  durationOp.param_u32 = 6u;
  durationOp.param_f32 = duration_flat_per_point;

  ModifierRuntimeOp speedOp;
  speedOp.opcode = static_cast<uint16_t>(ModifierOpCode::SKILL_SPEED_MULT);
  speedOp.param_u32 = 6u;
  speedOp.param_f32 = speed_mult_per_point;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) +
               2u * sizeof(ModifierRuntimeOp));
  AppendRuntimeStruct(blob, header);
  AppendRuntimeStruct(blob, record);
  AppendRuntimeStruct(blob, filter);
  AppendRuntimeStruct(blob, durationOp);
  AppendRuntimeStruct(blob, speedOp);
  return blob;
}

} // namespace

TEST_CASE("[Unit] SkillSpecializationBaker - Base Profile Baking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();

  entt::registry registry;
  const auto player = registry.create();

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 1, nullptr, profile, nullptr);

  CHECK(profile.skill_id == 1);
  CHECK(profile.effective_level == 1);
  CHECK(profile.projectile_count >= 1);
  CHECK(profile.more_damage_mult == doctest::Approx(1.0f));
  CHECK(profile.delivery.speed == doctest::Approx(400.0f));
}

TEST_CASE("[Unit] SkillSpecializationBaker - Talent Modifiers and Archetype Morph") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  // 210 的投射物增量已迁至 UMR，须显式重载真实产物，避免依赖用例执行顺序。
  EnsureModifierRuntimeForSkillSpec();

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &triggers = registry.emplace<TriggerRuleComponent>(player);

  // Configure Skill 2 with talent 210 (extra projectiles) and 230 (Boomerang morph)
  SpecializedSkill spec;
  spec.skill_id = 2;
  spec.allocated_points[210] = 3; // +3 projectiles
  spec.allocated_points[211] = 1; // split
  spec.allocated_points[230] = 1; // Boomerang morph

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, &triggers);

  CHECK(profile.skill_id == 2);
  CHECK(profile.projectile_count == 4); // 1 base + 3
  CHECK((profile.delivery.feature_flags & 1) != 0); // Boomerang morph
  CHECK(profile.delivery.sub_count == 3);
  CHECK((profile.delivery.feature_flags & 4) != 0); // HasSplit
}

TEST_CASE("[Unit] SkillSpecializationBaker - Skill 2 UMR Delivery Baking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();
  // 210 的非线性惩罚改为从机制表读取，需加载真实数值。
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));

  entt::registry registry;
  const auto player = registry.create();

  // 200 剑气纵横：范围与射程按 UMR AREA_MULT / RANGE_MULT 各自 +10%/点
  {
    SpecializedSkill spec;
    spec.skill_id = 2;
    spec.allocated_points[200] = 2;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, nullptr);
    CHECK(profile.area_radius == doctest::Approx(42.0f)); // 基础 35 × 1.2（2 点 × +10%）= 42
    CHECK(profile.delivery.range == doctest::Approx(600.0f)); // 基础 500 × 1.2（2 点 × +10%）= 600
  }

  // 201 凝神：法耗平减 -1/点（flat 语义）
  {
    SpecializedSkill spec;
    spec.skill_id = 2;
    spec.allocated_points[201] = 2;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(8.0f)); // 基础 10 - 2 点 × 1 = 8
  }

  // 202 锋芒：More 增伤 +10%/点（乘算）
  {
    SpecializedSkill spec;
    spec.skill_id = 2;
    spec.allocated_points[202] = 3;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, nullptr);
    CHECK(profile.more_damage_mult == doctest::Approx(1.30f));
  }

  // 210 多重剑气：投射物由 UMR 追加，惩罚按机制表分档（3 点 = pt3 = 0.15）
  {
    SpecializedSkill spec;
    spec.skill_id = 2;
    spec.allocated_points[210] = 3;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, nullptr);
    CHECK(profile.projectile_count == 4);                      // 基础 1 柄 + 3 点 × 1 = 4
    CHECK(profile.more_damage_mult == doctest::Approx(0.85f)); // pt3 惩罚：1 × (1 - 0.15) = 0.85
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Skill 3 UMR Delivery Baking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();

  entt::registry registry;
  const auto player = registry.create();

  // 步骤 1 基准：未加点时索敌半径为 200、灵剑基数为 3
  {
    BakedSkillProfile base{};
    SkillSpecializationBaker::Bake(registry, player, 3, nullptr, base, nullptr);
    CHECK(base.delivery.range == doctest::Approx(200.0f));
    CHECK(base.projectile_count == 3);
  }

  // 300 剑池充盈：+1 灵剑/点
  {
    SpecializedSkill spec;
    spec.skill_id = 3;
    spec.allocated_points[300] = 4;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 3, &spec, profile, nullptr);
    CHECK(profile.projectile_count == 7); // 3 + 4
  }

  // 310 索敌范围：在基准 200 上 +20%/点
  {
    SpecializedSkill spec;
    spec.skill_id = 3;
    spec.allocated_points[310] = 2;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 3, &spec, profile, nullptr);
    CHECK(profile.delivery.range == doctest::Approx(280.0f)); // 200 * 1.4
  }

  // 312 灵力网络：法耗 -5%/点（乘算折扣）
  {
    SpecializedSkill spec;
    spec.skill_id = 3;
    spec.allocated_points[312] = 2;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 3, &spec, profile, nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(22.5f)); // 25 * 0.9
  }

  // 331 弱点锁定：暴击率 +5%/点
  {
    SpecializedSkill spec;
    spec.skill_id = 3;
    spec.allocated_points[331] = 2;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 3, &spec, profile, nullptr);
    CHECK(profile.delivery.bonus_crit == doctest::Approx(0.10f));
  }

  // 332 致命锋芒：暴伤 +25%/点
  {
    SpecializedSkill spec;
    spec.skill_id = 3;
    spec.allocated_points[332] = 2;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 3, &spec, profile, nullptr);
    CHECK(profile.delivery.bonus_crit_damage == doctest::Approx(0.50f));
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Skill 3 Keystone Overrides UMR") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();

  entt::registry registry;
  const auto player = registry.create();

  // 300(4) + 311：先加 4 柄灵剑，再按无尽剑匣翻倍
  {
    SpecializedSkill spec;
    spec.skill_id = 3;
    spec.allocated_points[300] = 4;
    spec.allocated_points[311] = 1;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 3, &spec, profile, nullptr);
    CHECK(profile.projectile_count == 14); // (基础 3 柄 + 4 点) × 无尽剑匣 2 倍 = 14
  }

  // 300(4) + 330：巨剑降临形态晚于 UMR 覆盖为单柄、范围 80
  {
    SpecializedSkill spec;
    spec.skill_id = 3;
    spec.allocated_points[300] = 4;
    spec.allocated_points[330] = 1;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 3, &spec, profile, nullptr);
    CHECK(profile.projectile_count == 1);
    CHECK(profile.area_radius == doctest::Approx(80.0f));
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Node 210 Penalty Reads Mechanics") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();

  // 改写机制表证明惩罚值确由数据驱动：pt3 = 0.30 时 More 应变为 0.70。
  // 若代码仍残留 0.15 字面量，本用例会失败。
  const auto dir = std::filesystem::temp_directory_path() /
                   "nmd_skill_spec_baker_mechanics_guard";
  std::filesystem::create_directories(dir);
  const auto mechanicsPath = dir / "mechanics_pt3_030.json";
  {
    std::ofstream out(mechanicsPath, std::ios::binary);
    REQUIRE(out.good());
    // 机制表加载要求 1..12 技能 id 全部在场（缺一即拒载），故补空节点占位。
    out << R"({"version":1,"1":{},"2":{"210":{"damage_penalty_pt3":0.30}},)"
           R"("3":{},"4":{},"5":{},"6":{},"7":{},"8":{},"9":{},"10":{},)"
           R"("11":{},"12":{}})";
  }
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      mechanicsPath.string()));

  entt::registry registry;
  const auto player = registry.create();
  SpecializedSkill spec;
  spec.skill_id = 2;
  spec.allocated_points[210] = 3;
  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, nullptr);

  CHECK(profile.projectile_count == 4);
  CHECK(profile.more_damage_mult == doctest::Approx(0.70f));

  std::filesystem::remove(mechanicsPath);

  // 恢复真实机制表，避免合成表泄漏到依赖技能数据的后续用例。
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));
}

TEST_CASE("[Unit] SkillSpecializationBaker - Mana Flat Then Mult Order") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  // 注入合成 UMR：技能 2 同时含 flat(-1/点) 与 mult(0.10/点)，有效点数为 1。
  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
      BuildManaRuntimeBlob(-1.0f, 0.10f)));

  entt::registry registry;
  const auto player = registry.create();
  SpecializedSkill spec;
  spec.skill_id = 2;
  spec.allocated_points[201] = 1; // 非空加点使记录通过白名单采集
  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, nullptr);

  // 先 flat 后 mult：(10 - 1) * 0.90 = 8.10；若顺序颠倒则为 10 * 0.90 - 1 = 8.00。
  CHECK(profile.effective_mana_cost == doctest::Approx(8.10f));

  // 恢复真实运行时产物，避免合成数据泄漏到其它用例。
  REQUIRE(ReloadModifierRuntimeFromAsset());
}

TEST_CASE("[Unit] SkillSpecializationBaker - Mana Cost Floors At Zero") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  // 平减额远超基础法耗（10 - 100）*1.0 = -90，必须被钳制为 0 而非负法耗。
  // 折扣率传 0（乘性系数 = 1 - 0*1 = 1），否则算子自身的乘性下限会把结果归零，
  // 使本用例无法区分生产端是否真的做了钳制。
  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
      BuildManaRuntimeBlob(-100.0f, 0.0f)));

  entt::registry registry;
  const auto player = registry.create();
  SpecializedSkill spec;
  spec.skill_id = 2;
  spec.allocated_points[201] = 1;
  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, nullptr);

  CHECK(profile.effective_mana_cost == doctest::Approx(0.0f));

  REQUIRE(ReloadModifierRuntimeFromAsset());
}

TEST_CASE("[Unit] SkillSpecializationBaker - Trigger Contract Rule Generation") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &triggers = registry.emplace<TriggerRuleComponent>(player);

  // Skill 1 node 134 is a trigger contract (Shadow Blitz)
  SpecializedSkill spec;
  spec.skill_id = 1;
  spec.allocated_points[134] = 1;

  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, &triggers);

  const auto *contract = SkillRegistry::Get().GetNodeContract(1, 134);
  if (contract && contract->role == SpecNodeRole::Trigger) {
    CHECK(triggers.HasRule(134));
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Idempotence and Equality") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);

  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[100] = 2;

  SkillSystem::RebakeSkillProfiles(registry, player);
  const auto *profile1 = SkillSystem::GetBakedSkillProfile(registry, player, 1);
  REQUIRE(profile1 != nullptr);
  const BakedSkillProfile savedCopy = *profile1;

  // Rebake again without changes
  SkillSystem::RebakeSkillProfiles(registry, player);
  const auto *profile2 = SkillSystem::GetBakedSkillProfile(registry, player, 1);
  REQUIRE(profile2 != nullptr);

  // Profile must be strictly identical
  CHECK(*profile2 == savedCopy);
}

TEST_CASE("[Unit] SkillSpecializationBaker - Rebake TriggerRule Idempotency") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &triggers = registry.emplace<TriggerRuleComponent>(player);

  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[134] = 1; // Trigger node

  SkillSystem::RebakeSkillProfiles(registry, player);
  const size_t count1 = triggers.rule_count;
  CHECK(count1 > 0);
  CHECK(triggers.HasRule(134));

  // Rebake a second time: rule_count must NOT accumulate
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == count1);

  // Rebake a third time
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == count1);

  // Player respecs: clear allocated points and rebake
  active.specialized_slots[0].allocated_points.clear();
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == 0);
  CHECK(!triggers.HasRule(134));

  // Player re-allocates and equips
  active.specialized_slots[0].allocated_points[134] = 1;
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count > 0);
  CHECK(triggers.HasRule(134));

  // Player unequips hotbar slot only (specialization tree remains allocated)
  active.slots[0].id = 0;
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == 0);
  CHECK(!triggers.HasRule(134));
  CHECK(SkillSystem::GetBakedSkillProfile(registry, player, 1) == nullptr);

  // Player re-equips hotbar slot
  active.slots[0].id = 1;
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count > 0);
  CHECK(triggers.HasRule(134));
  CHECK(SkillSystem::GetBakedSkillProfile(registry, player, 1) != nullptr);

  // Player unsets/unequips skill slot completely (both slot and specialization tree)
  active.slots[0].id = 0;
  active.specialized_slots[0].skill_id = 0;
  active.specialized_slots[0].allocated_points.clear();
  SkillSystem::RebakeSkillProfiles(registry, player);
  CHECK(triggers.rule_count == 0);
  CHECK(!triggers.HasRule(134));

  // Scenario: Entity does NOT have TriggerRuleComponent initially
  {
    entt::registry freshRegistry;
    const auto player2 = freshRegistry.create();
    auto &active2 = freshRegistry.emplace<ActiveSkillsComponent>(player2);
    active2.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};
    active2.specialized_slots[0].skill_id = 1;
    active2.specialized_slots[0].allocated_points[134] = 1;

    CHECK_FALSE(freshRegistry.all_of<TriggerRuleComponent>(player2));
    SkillSystem::RebakeSkillProfiles(freshRegistry, player2);
    CHECK(freshRegistry.all_of<TriggerRuleComponent>(player2));
    auto *trig2 = freshRegistry.try_get<TriggerRuleComponent>(player2);
    REQUIRE(trig2 != nullptr);
    CHECK(trig2->rule_count == 1);
    CHECK(trig2->HasRule(134));

    // Rebake again: rule count unchanged
    SkillSystem::RebakeSkillProfiles(freshRegistry, player2);
    CHECK(trig2->rule_count == 1);
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - BakedDeliveryParams Dedicated Fields") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();

  entt::registry registry;
  const auto player = registry.create();

  // Skill 2 node 232 (Pull Trap) sets pull_radius without overwriting ballistic range
  {
    SpecializedSkill spec;
    spec.skill_id = 2;
    spec.allocated_points[232] = 1;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 2, &spec, profile, nullptr);
    CHECK((profile.delivery.feature_flags & 1) != 0); // Boomerang morph
    CHECK(profile.delivery.pull_radius == 120.0f);
    CHECK((profile.delivery.feature_flags & 16) != 0);
    CHECK(profile.delivery.range == 500.0f); // Default ballistic range preserved
  }

  // Skill 5 nodes 502 (more damage), 555 (crit dmg) & 574 (armor pen)
  {
    SpecializedSkill spec;
    spec.skill_id = 5;
    spec.allocated_points[502] = 2; // 2 * 10% = 20%
    spec.allocated_points[555] = 2; // 2 * 0.2 = 0.4（分数制暴伤增量）
    spec.allocated_points[574] = 3; // 3 * 6.0 = 18.0
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 5, &spec, profile, nullptr);
    CHECK(profile.delivery.bonus_crit_damage == doctest::Approx(0.4f));
    CHECK(profile.delivery.armor_pen == doctest::Approx(18.0f));
    // 技能5 交付基准改由 Baker 显式写入：下落弹速 1000、索敌 range 取机制表
    // lock_range=450；未分配 511 时乘算因子为 1.0，故保持基准值不被清零。
    CHECK(profile.delivery.speed == doctest::Approx(1000.0f));
    CHECK(profile.delivery.range == doctest::Approx(450.0f));
    CHECK(profile.more_damage_mult == doctest::Approx(1.20f));
  }

  // Skill 8 飞行参数唯一事实源为 skills.json params(speed 400 / max_distance 300)
  {
    SpecializedSkill spec;
    spec.skill_id = 8;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 8, &spec, profile, nullptr);
    CHECK(profile.delivery.speed == doctest::Approx(400.0f));
    CHECK(profile.delivery.range == doctest::Approx(300.0f));
    CHECK(profile.delivery.sub_count == 0);
  }

  // Skill 8 专职字段：830 侧刃、854 巨剑、800 法耗、801 速度/距离、803 折返、810 悬停、
  //                832 接刃回蓝、833 接刃攻速、834 剑步、851 牵引半径、853 剑意尺度
  {
    SpecializedSkill spec;
    spec.skill_id = 8;
    spec.allocated_points[800] = 4; // 8 - 4 = 4 法力
    spec.allocated_points[801] = 4; // +60% 速度/距离
    spec.allocated_points[803] = 4; // 1 + 0.40 折返倍率
    spec.allocated_points[810] = 1; // 悬停 0.8s
    spec.allocated_points[830] = 1; // 两侧刃
    spec.allocated_points[832] = 3; // 接刃回蓝 6
    spec.allocated_points[833] = 3; // 接刃攻速 +45%
    spec.allocated_points[834] = 3; // 剑步 +1.5s
    spec.allocated_points[851] = 4; // 牵引半径 *1.60
    spec.allocated_points[853] = 3; // 剑意尺度 +6%
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 8, &spec, profile, nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(4.0f));
    CHECK(profile.delivery.speed == doctest::Approx(640.0f));
    CHECK(profile.delivery.range == doctest::Approx(480.0f));
    CHECK(profile.delivery.return_damage_mult == doctest::Approx(1.40f));
    CHECK(profile.delivery.duration == doctest::Approx(0.8f));
    CHECK(profile.delivery.sub_count == 2);
    CHECK(profile.delivery.catch_mana == doctest::Approx(6.0f));
    CHECK(profile.delivery.combo_attack_speed == doctest::Approx(45.0f));
    CHECK(profile.delivery.step_extend_sec == doctest::Approx(1.5f));
    CHECK(profile.delivery.pull_radius_mult == doctest::Approx(1.60f));
    CHECK(profile.delivery.intent_scaling == doctest::Approx(0.06f));
  }

  // Skill 8 854 巨剑模式：取消侧刃并开启 5% 护甲转基础物伤
  {
    SpecializedSkill spec;
    spec.skill_id = 8;
    spec.allocated_points[854] = 1;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 8, &spec, profile, nullptr);
    CHECK(profile.delivery.sub_count == 0);
    CHECK(profile.delivery.giant_armor_scale == doctest::Approx(0.05f));
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Skill 1 Flowing Thrust Detailed Baking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();

  entt::registry registry;
  const auto player = registry.create();

  SUBCASE("Skill 1 Base Profile has 2 charges and 4.0s cooldown") {
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, nullptr, profile, nullptr);

    CHECK(profile.skill_id == 1);
    CHECK(profile.effective_charges == 2);
    CHECK(profile.effective_cooldown == doctest::Approx(4.0f));
    CHECK(profile.more_damage_mult == doctest::Approx(1.0f));
    CHECK((profile.effective_tags & Tag::SwordSkill) != Tag::None);
    CHECK((profile.effective_tags & Tag::Physical) != Tag::None);
    CHECK((profile.effective_tags & Tag::Movement) != Tag::None);
  }

  SUBCASE("Node 101 Energy Flow reduces mana cost without giving illegal damage") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[101] = 3; // 3 * 15% = 45% reduction

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    const auto *data = SkillRegistry::Get().GetSkill(1);
    REQUIRE(data != nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(data->mana_cost * 0.55f));
    CHECK(profile.more_damage_mult == doctest::Approx(1.0f));
  }

  SUBCASE("Node 102 Keen Edge increases bonus crit") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[102] = 3; // +6% crit

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    // 推导：node 102 每点 +2%，3 点 = 6% = 0.06（分数制）
    CHECK(profile.delivery.bonus_crit == doctest::Approx(0.06f));
  }

  SUBCASE("Node 103 Flowing Force increases more damage") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[103] = 2; // 2 * 10% = +20%

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.more_damage_mult == doctest::Approx(1.20f));
  }

  SUBCASE("Node 110 Thrust Rhythm reduces CD by 1s and damage by 15%") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[110] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.effective_cooldown == doctest::Approx(3.0f));
    CHECK(profile.more_damage_mult == doctest::Approx(0.85f));
  }

  SUBCASE("Node 111 Continuous Thrust increases max charges and CD") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[111] = 2; // +2 charges, +30% cooldown

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.effective_charges == 4);
    CHECK(profile.effective_cooldown == doctest::Approx(4.0f * 1.30f));
  }

  SUBCASE("Node 112 marks momentum allocation (runtime dash range / more scaling)") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[112] = 2;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    // 112 仅标记分配（feature_flags 位 256）；距离加成与位移 More 在运行时按实际位移结算
    CHECK((profile.delivery.feature_flags & 256) != 0);
    CHECK(profile.delivery.speed == doctest::Approx(400.0f));
    CHECK(profile.more_damage_mult == doctest::Approx(1.0f));
  }

  SUBCASE("Node 113 enables Windwalker feature flag") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[113] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.delivery.feature_flags & 4) != 0);
  }

  SUBCASE("Node 114 Riding The Wind is conditional on sword step, not baked unconditionally") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[114] = 3; // 御剑步期间近战暴击 +24%

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    // 未处于剑步时: 条件暴击不应注入无条件交付参数
    CHECK(profile.delivery.bonus_crit == doctest::Approx(0.0f));
    // 处于剑步时: 由 profile.riding_wind_bonus_crit 记录, 交付构造处检查剑步状态后注入
    // 推导：node 114 每点 +8%，3 点 = 24% = 0.24（分数制）
    CHECK(profile.riding_wind_bonus_crit == doctest::Approx(0.24f));
  }

  SUBCASE("Node 130 sets afterimage sub_count and feature flag") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[130] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.delivery.sub_count == 1);
    CHECK((profile.delivery.feature_flags & 2) != 0);
  }

  SUBCASE("Node 133 Swap enables explosion feature flag") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[133] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.delivery.feature_flags & 8) != 0);
  }

  SUBCASE("Node 134 Shadow Blitz generates trigger rule and scales radius/damage") {
    auto &triggers = registry.emplace<TriggerRuleComponent>(player);
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[134] = 2; // +50% dmg, +40% radius

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, &triggers);

    CHECK(profile.more_damage_mult == doctest::Approx(1.50f));
    CHECK(profile.area_radius == doctest::Approx(1.40f));
    CHECK(triggers.HasRule(134));
  }

  SUBCASE("Node 154 All In sets single charge, 8s CD, double mana, double damage, 100% crit") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[154] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    const auto *data = SkillRegistry::Get().GetSkill(1);
    REQUIRE(data != nullptr);
    CHECK(profile.effective_charges == 1);
    CHECK(profile.effective_cooldown == doctest::Approx(8.0f));
    CHECK(profile.effective_mana_cost == doctest::Approx(data->mana_cost * 2.0f));
    CHECK(profile.more_damage_mult == doctest::Approx(2.0f));
    // 推导：node 154 必暴 = 1.0（分数制）
    CHECK(profile.delivery.bonus_crit >= 1.0f);
  }

  SUBCASE("Node 170 Hellfire transmuter converts Physical to Fire") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[170] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.effective_tags & Tag::Fire) != Tag::None);
    CHECK((profile.effective_tags & Tag::Physical) == Tag::None);
  }

  SUBCASE("Node 172 FreezingWind transmuter converts Physical to Cold") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[172] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.effective_tags & Tag::Cold) != Tag::None);
    CHECK((profile.effective_tags & Tag::Physical) == Tag::None);
  }

  SUBCASE("Node 133 Swap removes Movement tag and adds Teleport tag") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[133] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK((profile.delivery.feature_flags & 8) != 0);
    CHECK((profile.effective_tags & Tag::Teleport) != Tag::None);
    CHECK((profile.effective_tags & Tag::Movement) == Tag::None);
  }

  SUBCASE("ResolveElementalConversion supports node-based transmuters across skills") {
    // Skill 1: 170 Fire, 172 Cold
    auto c170 = ResolveElementalConversion(170, 1);
    CHECK(c170.target_element == Tag::Fire);
    auto c172 = ResolveElementalConversion(172, 1);
    CHECK(c172.target_element == Tag::Cold);

    // Skill 2: 270 Cold, 272 Lightning (verified fix for 250 typo)
    auto c270 = ResolveElementalConversion(270, 1);
    CHECK(c270.target_element == Tag::Cold);
    auto c272 = ResolveElementalConversion(272, 1);
    CHECK(c272.target_element == Tag::Lightning);

    // Skill 3: 370 Fire, 372 Lightning
    auto c370 = ResolveElementalConversion(370, 1);
    CHECK(c370.target_element == Tag::Fire);
    auto c372 = ResolveElementalConversion(372, 1);
    CHECK(c372.target_element == Tag::Lightning);

    // Skill 4: 472 Lightning, 474 Cold
    auto c472 = ResolveElementalConversion(472, 1);
    CHECK(c472.target_element == Tag::Lightning);
    auto c474 = ResolveElementalConversion(474, 1);
    CHECK(c474.target_element == Tag::Cold);

    // Skill 5: 570 Fire, 572 Cold
    auto c570 = ResolveElementalConversion(570, 1);
    CHECK(c570.target_element == Tag::Fire);
    auto c572 = ResolveElementalConversion(572, 1);
    CHECK(c572.target_element == Tag::Cold);
  }

  SUBCASE("Combo 110+111 composes cooldown as (base + flat) * mult") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[110] = 1; // CD 平 -1s，More ×0.85
    spec.allocated_points[111] = 1; // 充能 +1，CD 乘 ×1.15

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    // 确定式合成：(4.0 - 1.0) × 1.15 = 3.45；旧实现依赖 unordered_map 遍历顺序
    CHECK(profile.effective_cooldown == doctest::Approx(3.45f));
    // 基础 2 充能 + 111 的 +1
    CHECK(profile.effective_charges == 3);
    CHECK(profile.more_damage_mult == doctest::Approx(0.85f));
  }

  SUBCASE("Combo 110+154 keeps All In final override") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[110] = 1; // CD 平 -1s
    spec.allocated_points[154] = 1; // 孤注一掷赋值式覆盖

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.effective_charges == 1);
    CHECK(profile.effective_cooldown == doctest::Approx(8.0f));
  }

  SUBCASE("Combo 111+154 keeps All In final override") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[111] = 1; // 充能 +1，CD 乘 ×1.15
    spec.allocated_points[154] = 1;

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.effective_charges == 1);
    CHECK(profile.effective_cooldown == doctest::Approx(8.0f));
  }

  SUBCASE("Combo 101+103 scales mana and more damage independently") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[101] = 1; // 法耗 ×0.85
    spec.allocated_points[103] = 1; // More ×1.10

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    const auto *data = SkillRegistry::Get().GetSkill(1);
    REQUIRE(data != nullptr);
    CHECK(profile.effective_mana_cost ==
          doctest::Approx(data->mana_cost * 0.85f));
    CHECK(profile.more_damage_mult == doctest::Approx(1.10f));
  }

  SUBCASE("Combo 102+134 scales crit, more damage and radius independently") {
    SpecializedSkill spec;
    spec.skill_id = 1;
    spec.allocated_points[102] = 1; // 暴击 +0.02
    spec.allocated_points[134] = 1; // More ×1.25，范围 ×1.20

    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 1, &spec, profile, nullptr);

    CHECK(profile.delivery.bonus_crit == doctest::Approx(0.02f));
    CHECK(profile.more_damage_mult == doctest::Approx(1.25f));
    CHECK(profile.area_radius == doctest::Approx(1.20f));
  }
}

TEST_CASE("[Unit] SkillSystem - Skill 1 Charges and Cooldown Execution") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();
  systems::SpatialHashGrid grid(1000, 1000, 50);

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.mana = 200.0f;
  stats.max_mana = 200.0f;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;

  SkillSystem::RebakeSkillProfiles(registry, player);

  SUBCASE("Base skill consumes charges and recharges with 4.0s cooldown") {
    auto &slot = active.slots[0];
    CHECK(slot.current_charges == 2);

    // First cast
    bool cast1 = SkillSystem::TryCast(registry, player, 0);
    CHECK(cast1);
    CHECK(slot.current_charges == 1);
    CHECK(slot.cooldown == doctest::Approx(4.0f));
    registry.remove<SkillExecution>(player);

    // Second cast
    bool cast2 = SkillSystem::TryCast(registry, player, 0);
    CHECK(cast2);
    CHECK(slot.current_charges == 0);
    registry.remove<SkillExecution>(player);

    // Third cast should fail because 0 charges
    bool cast3 = SkillSystem::TryCast(registry, player, 0);
    CHECK_FALSE(cast3);

    // Tick cooldown by 2.0s
    SkillSystem::Update(registry, grid, 2.0f);
    CHECK(slot.current_charges == 0);
    CHECK(slot.cooldown == doctest::Approx(2.0f));

    // Tick cooldown by another 2.0s (total 4.0s) -> 1 charge restored, next charge cooldown starts
    SkillSystem::Update(registry, grid, 2.0f);
    CHECK(slot.current_charges == 1);
    CHECK(slot.cooldown == doctest::Approx(4.0f));

    // Now can cast again
    bool cast4 = SkillSystem::TryCast(registry, player, 0);
    CHECK(cast4);
    CHECK(slot.current_charges == 0);
    registry.remove<SkillExecution>(player);

    // Tick 8.0s (two 4.0s recharge cycles) -> both charges fully restored
    SkillSystem::Update(registry, grid, 4.0f);
    SkillSystem::Update(registry, grid, 4.0f);
    CHECK(slot.current_charges == 2);
    CHECK(slot.cooldown == doctest::Approx(0.0f));
  }

  SUBCASE("Node 111 expands charges to 4") {
    active.specialized_slots[0].allocated_points[111] = 2; // +2 charges (total 4)
    SkillSystem::RebakeSkillProfiles(registry, player);

    auto &slot = active.slots[0];
    slot.current_charges = 4;
    slot.cooldown = 0.0f;

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 3);
    registry.remove<SkillExecution>(player);

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 2);
    registry.remove<SkillExecution>(player);

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 1);
    registry.remove<SkillExecution>(player);

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 0);
    registry.remove<SkillExecution>(player);

    CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));
  }

  SUBCASE("Node 154 All In restricts to 1 charge and 8s cooldown") {
    active.specialized_slots[0].allocated_points[154] = 1;
    SkillSystem::RebakeSkillProfiles(registry, player);

    auto &slot = active.slots[0];
    slot.current_charges = 1;
    slot.cooldown = 0.0f;

    CHECK(SkillSystem::TryCast(registry, player, 0));
    CHECK(slot.current_charges == 0);
    CHECK(slot.cooldown == doctest::Approx(8.0f));
    registry.remove<SkillExecution>(player);

    CHECK_FALSE(SkillSystem::TryCast(registry, player, 0));

    SkillSystem::Update(registry, grid, 4.0f);
    CHECK(slot.current_charges == 0);
    CHECK(slot.cooldown == doctest::Approx(4.0f));

    SkillSystem::Update(registry, grid, 4.0f);
    CHECK(slot.current_charges == 1);
    CHECK(slot.cooldown == doctest::Approx(0.0f));
  }
}

TEST_CASE("[Unit] FlowingThrust - Runtime Node Behaviors (150, 174, 115, 155)") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.crit_damage = 1.5f;
  pStats.crit_chance = 100.0f; // guarantee crit for testing crit mult

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;

  SUBCASE("Node 150 increases critical damage multiplier against >80% HP or controlled targets") {
    active.specialized_slots[0].allocated_points[150] = 4; // +60% crit multiplier

    // 1. Target with >80% HP
    const auto dummyHighHp = registry.create();
    registry.emplace<HealthComponent>(dummyHighHp, 100.0f, 100.0f);
    registry.emplace<Position>(dummyHighHp, 10.0f, 0.0f);

    DamageRequest reqHigh;
    reqHigh.attacker = player;
    reqHigh.defender = dummyHighHp;
    reqHigh.skill_id = 1;
    reqHigh.base_pool.Add(Tag::Physical, 100.0f);
    reqHigh.is_simulation = true;
    DamageResult resHigh = DamagePipeline::Calculate(registry, reqHigh);
    CHECK(resHigh.is_crit);
    // (100.0f base_pool + 10.0f skill_1 base_damage) * (1.5 base crit + 0.6 bonus crit mult) = 231.0f
    CHECK(resHigh.total_damage == doctest::Approx(231.0f));

    // 2. Target with <80% HP and not controlled
    const auto dummyLowHp = registry.create();
    registry.emplace<HealthComponent>(dummyLowHp, 50.0f, 100.0f);
    registry.emplace<Position>(dummyLowHp, 20.0f, 0.0f);

    DamageRequest reqLow;
    reqLow.attacker = player;
    reqLow.defender = dummyLowHp;
    reqLow.skill_id = 1;
    reqLow.base_pool.Add(Tag::Physical, 100.0f);
    reqLow.is_simulation = true;
    DamageResult resLow = DamagePipeline::Calculate(registry, reqLow);
    CHECK(resLow.is_crit);
    // (100.0f base_pool + 10.0f skill_1 base_damage) * 1.5 base crit = 165.0f
    CHECK(resLow.total_damage == doctest::Approx(165.0f));

    // 3. Target with <80% HP BUT controlled (e.g. Slow)
    auto &eff = registry.emplace<ActiveEffectsComponent>(dummyLowHp);
    eff.effects.push_back(BuffEffect{
        .id = "Slow",
        .name = "Slow",
        .type = BuffType::SpeedDown,
        .duration = 2.0f,
        .remaining = 2.0f
    });
    DamageResult resControlled = DamagePipeline::Calculate(registry, reqLow);
    CHECK(resControlled.is_crit);
    // Controlled -> receives +0.6 bonus crit mult -> 231.0f
    CHECK(resControlled.total_damage == doctest::Approx(231.0f));
  }

  SUBCASE("Node 174 applies and stacks Elemental Erosion debuff") {
    active.specialized_slots[0].allocated_points[174] = 4; // -12 resist per stack
    SkillSystem::RebakeSkillProfiles(registry, player);

    const auto target = registry.create();
    registry.emplace<HealthComponent>(target, 100.0f, 100.0f);
    registry.emplace<Position>(target, 10.0f, 0.0f);
    auto &effects = registry.emplace<ActiveEffectsComponent>(target);
    effects.effects.push_back(BuffEffect{
        .id = "Ignite",
        .name = "Ignite",
        .type = BuffType::Burn,
        .duration = 3.0f,
        .remaining = 3.0f
    });

    auto hitFunc = SkillBehaviorRegistry::GetHit(1);
    REQUIRE(hitFunc != nullptr);

    auto findBuff = [&](const std::string &id) -> const BuffEffect * {
      for (const auto &b : effects.effects) {
        if (b.id == id) return &b;
      }
      return nullptr;
    };

    // Hit 1: 1 stack (-12 fire resist)
    hitFunc(registry, player, target, Tag::Fire, false);
    const auto *buff = findBuff("ElementalErosionFire");
    REQUIRE(buff != nullptr);
    REQUIRE_FALSE(buff->modifiers.empty());
    CHECK(buff->modifiers[0].value == doctest::Approx(-12.0f));
    CHECK(buff->modifiers[0].type == StatType::ResistFire);

    // Hit 2: 2 stacks (-24 fire resist)
    hitFunc(registry, player, target, Tag::Fire, false);
    buff = findBuff("ElementalErosionFire");
    REQUIRE(buff != nullptr);
    CHECK(buff->modifiers[0].value == doctest::Approx(-24.0f));

    // Hit 3, 4, 5, 6: capped at 5 stacks (-60 fire resist)
    hitFunc(registry, player, target, Tag::Fire, false);
    hitFunc(registry, player, target, Tag::Fire, false);
    hitFunc(registry, player, target, Tag::Fire, false);
    hitFunc(registry, player, target, Tag::Fire, false);
    buff = findBuff("ElementalErosionFire");
    REQUIRE(buff != nullptr);
    CHECK(buff->modifiers[0].value == doctest::Approx(-60.0f));
  }

  SUBCASE("Node 155 CD and charge reset on kill with All In active") {
    active.specialized_slots[0].allocated_points[154] = 1; // All In
    active.specialized_slots[0].allocated_points[155] = 4; // 100% chance to reset CD on kill
    SkillSystem::RebakeSkillProfiles(registry, player);

    auto &slot = active.slots[0];
    slot.current_charges = 0;
    slot.cooldown = 8.0f;

    const auto victim = registry.create();
    registry.emplace<HealthComponent>(victim, 0.0f, 100.0f); // Dead
    registry.emplace<Position>(victim, 10.0f, 0.0f);

    auto hitFunc = SkillBehaviorRegistry::GetHit(1);
    REQUIRE(hitFunc != nullptr);

    hitFunc(registry, player, victim, Tag::Physical, false);

    CHECK(slot.current_charges == 1);
    CHECK(slot.cooldown == doctest::Approx(0.0f));
  }
}

TEST_CASE("[Unit] FlowingThrust - 133 Swap Explosion Damage Payload") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();
  // 注册技能行为与 OnSkillHit 事件处理器（DoHit 依赖事件链路）
  SkillSystem::InitHooks();

  systems::SpatialHashGrid grid(1000, 1000, 50);
  entt::registry registry;

  // 玩家位于原点，持有技能 1 并分配 133（移形换位：传送爆炸）
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<PlayerTag>(player);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.mana = 200.0f;
  pStats.max_mana = 200.0f;
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f; // 关闭暴击，保证伤害可精确对比
  pStats.crit_damage = 1.5f;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[133] = 1;

  // 预置契约运行时并压住 134 的触发冷却，隔离触发派生伤害，专注爆炸本体增伤
  auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
  runtime.trigger_cooldowns[134] = 999.0f;

  // 敌人位于传送目标点 (300, 0)
  const auto enemy = registry.create();
  registry.emplace<Position>(enemy, 300.0f, 0.0f);
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<HealthComponent>(enemy, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(enemy);
  registry.emplace<ActiveEffectsComponent>(enemy);

  // 事件探针：捕获爆炸命中的元素 tags（证明 payload_context.effective_tags 进入事件）
  Tag capturedTags = Tag::None;
  CombatEventDispatcher::Register(
      CombatEventType::OnSkillHit,
      [&](entt::registry &, const CombatEvent &evt) { capturedTags = evt.tags; },
      100);

  // 触发一次传送爆炸：Preparing -> Casting 调用 DoCast 生成两个 DirectStrike，
  // 再由 ProjectileSystem 结算命中
  auto castAndResolve = [&]() {
    SkillSystem::RebakeSkillProfiles(registry, player);
    REQUIRE(SkillSystem::TryCast(registry, player, 0, {300.0f, 0.0f}));
    SkillSystem::Update(registry, grid, 0.11f); // 触发 DoCast（Swap 分支）
    registry.remove<SkillExecution>(player);
    // 诊断：检查爆炸实体携带的有效载荷
    auto strikeView = registry.view<Position, DirectStrikeComponent>();
    for (auto ent : strikeView) {
      const auto &d = strikeView.get<DirectStrikeComponent>(ent);
      MESSAGE("strike pos=(", strikeView.get<Position>(ent).x, ",",
              strikeView.get<Position>(ent).y, ") has_payload=", d.has_payload,
              " more=", d.payload_context.more_damage,
              " effTags=", static_cast<uint64_t>(d.payload_context.effective_tags));
    }
    ProjectileSystem::Update(registry, grid, 0.02f); // DirectStrike 伤害结算
  };

  SUBCASE("Node 134 Shadow Blitz more damage multiplies explosion damage") {
    // 基线：仅 133（无 134），记录一次爆炸伤害
    castAndResolve();
    auto &hp = registry.get<HealthComponent>(enemy);
    const float baseline = hp.max - hp.current;
    REQUIRE(baseline > 0.0f);

    // 分配 134（1 点 -> more_damage_mult x1.25）后重测
    active.specialized_slots[0].allocated_points[134] = 1;
    hp.current = hp.max;
    active.slots[0].current_charges = 2;
    // 首 cast 已将玩家传送到目标点，重置回原点使起点/终点两次爆炸分离，
    // 保证再次命中时仅终点爆炸结算（否则两个爆炸叠加在敌人身上）
    auto &ppos = registry.get<Position>(player);
    ppos.x = 0.0f;
    ppos.y = 0.0f;
    castAndResolve();
    const float withBlitz = hp.max - hp.current;
    MESSAGE("baseline=", baseline, " with134=", withBlitz,
            " expected=", baseline * 1.25f);
    CHECK(withBlitz == doctest::Approx(baseline * 1.25f).epsilon(0.02f));
  }

  SUBCASE("Node 170 Hellfire explosion hit applies Ignite (Fire tag via payload)") {
    active.specialized_slots[0].allocated_points[170] = 1;
    castAndResolve();

    MESSAGE("capturedTags=", static_cast<uint64_t>(capturedTags),
            " hasFire=", ((capturedTags & Tag::Fire) != Tag::None));
    CHECK((capturedTags & Tag::Fire) != Tag::None);

    auto *effects = registry.try_get<ActiveEffectsComponent>(enemy);
    REQUIRE(effects != nullptr);
    MESSAGE("effectCount=", effects->effects.size());
    bool hasIgnite = false;
    for (const auto &b : effects->effects) {
      if (b.type == BuffType::Burn && b.name == "Ignite") {
        hasIgnite = true;
        break;
      }
    }
    CHECK(hasIgnite);
  }

  SUBCASE("Node 172 FreezingWind explosion hit carries Cold tag and applies 30% slow") {
    active.specialized_slots[0].allocated_points[172] = 1;
    castAndResolve();

    // 元素 tags 经 payload_context.effective_tags 进入命中事件，
    // DoHit 的 element_tag 即来自该事件
    MESSAGE("capturedTags=", static_cast<uint64_t>(capturedTags),
            " hasCold=", ((capturedTags & Tag::Cold) != Tag::None));
    CHECK((capturedTags & Tag::Cold) != Tag::None);

    // 172 减速: 走 legacy SpeedDown buff（Slow 异常未注册 ailment 契约，
    // 与 HazardSystem::ApplyChillDebuff 的既有减速机制保持一致）
    auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
    REQUIRE(fx != nullptr);
    const auto *slow = fx->Get("FrostSlow");
    REQUIRE(slow != nullptr);
    REQUIRE(slow->type == BuffType::SpeedDown);
    REQUIRE_FALSE(slow->modifiers.empty());
    CHECK(slow->modifiers[0].value == doctest::Approx(-30.0f));
    CHECK(slow->modifiers[0].type == StatType::MoveSpeed);
  }
}

TEST_CASE("[Unit] FlowingThrust - H9d removed: hit does NOT reduce skill 2 cooldown") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<CombatStats>(player);
  auto &blade = registry.emplace<BladeResourceComponent>(player);
  blade.kind = BladeResourceKind::SwordFlow;
  blade.current = 5;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[1] = SkillSlot{.id = 2, .cooldown = 5.0f, .current_charges = 1};
  active.specialized_slots[0].skill_id = 1;
  SkillSystem::RebakeSkillProfiles(registry, player);

  const auto victim = registry.create();
  registry.emplace<HealthComponent>(victim, 100.0f, 100.0f);
  registry.emplace<Position>(victim, 10.0f, 0.0f);

  auto hitFunc = SkillBehaviorRegistry::GetHit(1);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, victim, Tag::Physical, false);

  // 负向断言：旧私有特例"命中削减技能 2 CD 0.75s"已删除，命中后技能 2 CD 保持不变
  CHECK(active.slots[1].cooldown == doctest::Approx(5.0f));
}

TEST_CASE("[Unit] FlowingThrust - 170 Hellfire ember trail ignites enemies") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  (void)systems::AilmentRegistry::Get().EnsureLoaded();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[133] = 1; // 移形换位
  active.specialized_slots[0].allocated_points[170] = 1; // 劫火
  SkillSystem::RebakeSkillProfiles(registry, player);

  // 敌人位于突进路径线段 (0,0)->(100,0) 内
  const auto enemy = registry.create();
  registry.emplace<EnemyTag>(enemy);
  registry.emplace<Position>(enemy, 30.0f, 0.0f);
  registry.emplace<HealthComponent>(enemy, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(enemy);

  // 直接调用 DoCast（Swap 分支）：沿起点→终点铺设余烬带
  auto castFunc = SkillBehaviorRegistry::GetCast(1);
  REQUIRE(castFunc != nullptr);
  SkillExecution exec;
  exec.skill_id = 1;
  exec.owner = player;
  exec.target_pos = {100.0f, 0.0f};
  exec.active_nodes.set(33); // 133 Swap
  exec.active_nodes.set(70); // 170 Hellfire
  castFunc(registry, player, exec);

  // 余烬带参数从 skill_mechanics.json 读取（未分配 171 → 基础值）
  auto zoneView = registry.view<FlowingEmberZoneComponent>();
  REQUIRE_FALSE(zoneView.empty());
  for (auto ent : zoneView) {
    const auto &zone = zoneView.get<FlowingEmberZoneComponent>(ent);
    CHECK(zone.width == doctest::Approx(60.0f));
    CHECK(zone.duration == doctest::Approx(2.0f));
    CHECK(zone.remaining == doctest::Approx(2.0f));
    CHECK(zone.owner == player);
  }

  // 推进 0.5s：敌人处于余烬内被点燃（同一余烬去重）
  skills::UpdateFlowingThrustEmbers(registry, 0.5f);
  auto *fx = registry.try_get<ActiveEffectsComponent>(enemy);
  REQUIRE(fx != nullptr);
  bool hasIgnite = false;
  for (const auto &b : fx->effects) {
    if (b.type == BuffType::Burn && b.id.find("Ignite") != std::string::npos) {
      hasIgnite = true;
    }
  }
  CHECK(hasIgnite);

  // 余烬到期后销毁
  skills::UpdateFlowingThrustEmbers(registry, 2.0f);
  CHECK(registry.view<FlowingEmberZoneComponent>().empty());
}

TEST_CASE("[Unit] FlowingThrust - 171 Infernal Path fire damage while standing on embers") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[133] = 1;
  active.specialized_slots[0].allocated_points[170] = 1;
  active.specialized_slots[0].allocated_points[171] = 3; // 业火焚途 3 点
  SkillSystem::RebakeSkillProfiles(registry, player);

  auto castFunc = SkillBehaviorRegistry::GetCast(1);
  REQUIRE(castFunc != nullptr);
  SkillExecution exec;
  exec.skill_id = 1;
  exec.owner = player;
  exec.target_pos = {100.0f, 0.0f};
  exec.active_nodes.set(33);
  exec.active_nodes.set(70);
  castFunc(registry, player, exec);

  // 171 每点: 余烬宽度 +25% (60 → 105)、持续 +0.5s (2.0 → 3.5)
  auto zoneView = registry.view<FlowingEmberZoneComponent>();
  REQUIRE_FALSE(zoneView.empty());
  for (auto ent : zoneView) {
    const auto &zone = zoneView.get<FlowingEmberZoneComponent>(ent);
    CHECK(zone.width == doctest::Approx(105.0f));
    CHECK(zone.duration == doctest::Approx(3.5f));
    CHECK(zone.infernal_points == 3);
  }

  // 玩家被传送到终点 (100,0)，站在自己余烬上 → 火焰伤害加成 6%×3 = 18%
  skills::UpdateFlowingThrustEmbers(registry, 0.25f);
  auto &fx = registry.get_or_emplace<ActiveEffectsComponent>(player);
  const auto *buff = fx.Get("InfernalPath");
  REQUIRE(buff != nullptr);
  REQUIRE_FALSE(buff->modifiers.empty());
  CHECK(buff->modifiers[0].value == doctest::Approx(18.0f));
  CHECK(buff->modifiers[0].type == StatType::FireDamage);

  // 离开余烬：不再刷新，短时 buff 自然过期
  registry.get<Position>(player).x = 500.0f;
  skills::UpdateFlowingThrustEmbers(registry, 0.25f);
  fx.Update(0.3f);
  CHECK(fx.Get("InfernalPath") == nullptr);
}

TEST_CASE("[Unit] FlowingThrust - 172 FreezingWind +50% more damage vs frozen") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;
  const auto player = registry.create();
  auto &pStats = registry.emplace<CombatStats>(player);
  // 武器伤害置 0：仅剩技能固有基础伤害 base_damage=10（skills.json 技能1），
  // 使期望数值可精确断言
  pStats.min_weapon_damage = 0.0f;
  pStats.max_weapon_damage = 0.0f;
  pStats.crit_chance = 0.0f;
  pStats.crit_damage = 1.5f;

  const auto makeDefender = [&](bool frozen) {
    const auto e = registry.create();
    registry.emplace<HealthComponent>(e, 100000.0f, 100000.0f);
    registry.emplace<CombatStats>(e);
    if (frozen) {
      auto &fx = registry.emplace<ActiveEffectsComponent>(e);
      BuffEffect frozenBuff{.id = "Frozen",
                            .name = "Frozen",
                            .type = BuffType::Freeze,
                            .duration = 2.5f,
                            .remaining = 2.5f,
                            .is_debuff = true};
      fx.AddOrRefresh(frozenBuff);
    }
    return e;
  };

  const auto normal = makeDefender(false);
  const auto frozen = makeDefender(true);

  DamageRequest req;
  req.attacker = player;
  req.defender = normal;
  req.skill_id = 1;
  req.base_pool.Add(Tag::Physical, 100.0f);
  // is_simulation=true：跳过防御判定与拦截器，锁定伤害乘区，便于精确断言冻结增伤
  req.is_simulation = true;
  auto normalResult = DamagePipeline::Execute(registry, req, player, false);
  // 100 基础 + 技能固有基础伤害 10 = 110
  CHECK(normalResult.damage.total_damage == doctest::Approx(110.0f));

  req.defender = frozen;
  auto frozenResult = DamagePipeline::Execute(registry, req, player, false);
  // 110 × 1.5 冻结增伤 = 165
  CHECK(frozenResult.damage.total_damage == doctest::Approx(165.0f));
}

TEST_CASE("[Unit] FlowingThrust - 173 BoneDeepFrost shatter on frozen victim") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  // 武器伤害置 0：碎裂溅射数值取自 LastCritDamageComponent，与武器无关；
  // 置 0 后仅剩技能固有基础伤害 base_damage=10，便于精确断言设计值
  pStats.min_weapon_damage = 0.0f;
  pStats.max_weapon_damage = 0.0f;
  pStats.crit_chance = 0.0f; // 关闭暴击，保证溅射伤害可精确对比
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[173] = 7; // 碎裂几率 15%×7=105% 必然触发
  SkillSystem::RebakeSkillProfiles(registry, player);

  // 冻结目标: 拥有 Freeze buff，且记录了来自玩家的上次暴击 1000 点
  const auto victim = registry.create();
  registry.emplace<EnemyTag>(victim);
  registry.emplace<Position>(victim, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(victim, 10000.0f, 10000.0f);
  registry.emplace<CombatStats>(victim);
  auto &vfx = registry.emplace<ActiveEffectsComponent>(victim);
  BuffEffect frozen{.id = "Frozen",
                    .name = "Frozen",
                    .type = BuffType::Freeze,
                    .duration = 2.5f,
                    .remaining = 2.5f,
                    .is_debuff = true};
  vfx.AddOrRefresh(frozen);
  registry.emplace<LastCritDamageComponent>(victim, 1000.0f, player);

  // 溅射目标位于碎裂半径 200 码内
  const auto splashTarget = registry.create();
  registry.emplace<EnemyTag>(splashTarget);
  registry.emplace<Position>(splashTarget, 100.0f, 0.0f);
  registry.emplace<HealthComponent>(splashTarget, 10000.0f, 10000.0f);
  registry.emplace<CombatStats>(splashTarget);

  auto hitFunc = SkillBehaviorRegistry::GetHit(1);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, victim, Tag::Cold, true);

  // 溅射 = 1000 × 30% = 300 冰霜伤害
  const auto &shp = registry.get<HealthComponent>(splashTarget);
  // 实路径按完整管线结算，无额外合成系数。
  // 设计值 300 由下方 is_simulation 请求交叉验证。
  CHECK(shp.current == doctest::Approx(9700.0f));

  // 复现 DoHit 溅射请求构造（skill_id=1，Cold 300，防递归标签），交叉验证设计值
  DamageRequest splashReq;
  splashReq.attacker = player;
  splashReq.defender = splashTarget;
  splashReq.skill_id = 1;
  splashReq.base_pool.Add(Tag::Cold, 300.0f);
  splashReq.additional_tags = Tag::Cold | Tag::DamageOverTime | Tag::Area;
  splashReq.is_simulation = true;
  auto splashResult = DamagePipeline::Execute(registry, splashReq, player, false);
  // 300 溅射 + 技能固有基础伤害 10（skills.json base_damage=10，武器已置 0）
  CHECK(splashResult.damage.total_damage == doctest::Approx(310.0f));
}

TEST_CASE("[Unit] FlowingThrust - 175 Residual Elements spread respects 1s ICD") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  (void)systems::AilmentRegistry::Get().EnsureLoaded();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[175] = 5; // 传染几率 20%×5=100%

  // 携带点燃的命中目标
  const auto victim = registry.create();
  registry.emplace<EnemyTag>(victim);
  registry.emplace<Position>(victim, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(victim, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(victim);
  auto &vfx = registry.emplace<ActiveEffectsComponent>(victim);
  BuffEffect ignite{.id = "Ignite",
                    .name = "Ignite",
                    .type = BuffType::Burn,
                    .duration = 3.0f,
                    .remaining = 3.0f,
                    .is_debuff = true};
  vfx.AddOrRefresh(ignite);

  // 附近两名敌人：A 在 50 码、B 在 80 码
  const auto targetA = registry.create();
  registry.emplace<EnemyTag>(targetA);
  registry.emplace<Position>(targetA, 50.0f, 0.0f);
  registry.emplace<HealthComponent>(targetA, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(targetA);
  const auto targetB = registry.create();
  registry.emplace<EnemyTag>(targetB);
  registry.emplace<Position>(targetB, 80.0f, 0.0f);
  registry.emplace<HealthComponent>(targetB, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(targetB);

  auto hitFunc = SkillBehaviorRegistry::GetHit(1);
  REQUIRE(hitFunc != nullptr);

  // 第一次命中: 传染给最近的 A
  hitFunc(registry, player, victim, Tag::Fire, false);
  auto *fxA = registry.try_get<ActiveEffectsComponent>(targetA);
  REQUIRE(fxA != nullptr);
  bool aIgnited = false;
  for (const auto &b : fxA->effects) {
    if (b.type == BuffType::Burn && b.id.find("Ignite") != std::string::npos) aIgnited = true;
  }
  CHECK(aIgnited);
  CHECK(registry.try_get<ActiveEffectsComponent>(targetB) == nullptr);

  // 第二次命中（同一帧，ICD 1s 内）: 不传染
  hitFunc(registry, player, victim, Tag::Fire, false);
  CHECK(registry.try_get<ActiveEffectsComponent>(targetB) == nullptr);

  // 冷却结束后: A 移出范围，传染给 B
  auto &ftState = registry.get_or_emplace<FlowingThrustStateComponent>(player);
  ftState.last_infect_time = static_cast<float>(GetTime()) - 2.0f;
  registry.get<Position>(targetA).x = 500.0f;
  hitFunc(registry, player, victim, Tag::Fire, false);
  auto *fxB = registry.try_get<ActiveEffectsComponent>(targetB);
  REQUIRE(fxB != nullptr);
  bool bIgnited = false;
  for (const auto &b : fxB->effects) {
    if (b.type == BuffType::Burn && b.id.find("Ignite") != std::string::npos) bIgnited = true;
  }
  CHECK(bIgnited);
}

// F7 回归：传染减速与 172 直击减速共用 id "FrostSlow"，刷新时 AddOrRefresh 会
// 用新效果的 type/kind 覆盖旧值；若传染创建点漏写 type=SpeedDown，会把目标身上
// 共享的减速清成 type=None，导致 275 扩散/835 净化/UI 等 type 消费方失效。
TEST_CASE("[Unit] FlowingThrust - 175 spread slow keeps SpeedDown type on refresh") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  (void)systems::AilmentRegistry::Get().EnsureLoaded();

  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &pStats = registry.emplace<CombatStats>(player);
  pStats.min_weapon_damage = 30.0f;
  pStats.max_weapon_damage = 40.0f;
  pStats.crit_chance = 0.0f;
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 2};
  active.specialized_slots[0].skill_id = 1;
  active.specialized_slots[0].allocated_points[175] = 5; // 传染几率 20%×5=100%

  // 主目标携带寒冷减速（172 已施加的 FrostSlow：type=SpeedDown / kind=Slow）
  const auto victim = registry.create();
  registry.emplace<EnemyTag>(victim);
  registry.emplace<Position>(victim, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(victim, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(victim);
  auto &vfx = registry.emplace<ActiveEffectsComponent>(victim);
  vfx.AddOrRefresh(BuffEffect{.id = "FrostSlow",
                              .name = "Frost Slow",
                              .type = BuffType::SpeedDown,
                              .kind = BuffKind::Slow,
                              .duration = 2.5f,
                              .remaining = 2.5f,
                              .is_debuff = true});

  // 附近目标已带同一 FrostSlow，传染刷新后其 type/kind 必须保持不变
  const auto targetA = registry.create();
  registry.emplace<EnemyTag>(targetA);
  registry.emplace<Position>(targetA, 50.0f, 0.0f);
  registry.emplace<HealthComponent>(targetA, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(targetA);
  auto &afx = registry.emplace<ActiveEffectsComponent>(targetA);
  afx.AddOrRefresh(BuffEffect{.id = "FrostSlow",
                              .name = "Frost Slow",
                              .type = BuffType::SpeedDown,
                              .kind = BuffKind::Slow,
                              .duration = 2.5f,
                              .remaining = 2.5f,
                              .is_debuff = true});

  auto hitFunc = SkillBehaviorRegistry::GetHit(1);
  REQUIRE(hitFunc != nullptr);
  hitFunc(registry, player, victim, Tag::Cold, false);

  auto *fxA = registry.try_get<ActiveEffectsComponent>(targetA);
  REQUIRE(fxA != nullptr);
  const BuffEffect *spread = fxA->GetByKind(BuffKind::Slow);
  REQUIRE(spread != nullptr);
  CHECK(spread->id == "FrostSlow");
  CHECK(spread->type == BuffType::SpeedDown);
  CHECK(spread->kind == BuffKind::Slow);
}

// ===== UMR-SKILL-BATCH-2：技能 4/5/6 交付算子烘焙断言 =====

TEST_CASE("[Unit] SkillSpecializationBaker - Skill 4 Blade Ward UMR Baking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();

  entt::registry registry;
  const auto player = registry.create();

  // 402 法耗折扣：基准 30 法耗，30 * (1 - 0.15 * N)；节点上限 3 点。
  SUBCASE("402 mana cost discount scales per point") {
    constexpr int kPoints[3] = {1, 2, 3};
    constexpr float kExpected[3] = {25.5f, 21.0f, 16.5f};
    for (int i = 0; i < 3; ++i) {
      SpecializedSkill spec;
      spec.skill_id = 4;
      spec.allocated_points[402] = kPoints[i];
      BakedSkillProfile profile{};
      SkillSpecializationBaker::Bake(registry, player, 4, &spec, profile, nullptr);
      CHECK(profile.effective_mana_cost == doctest::Approx(kExpected[i]));
    }
  }

  // 471 反击增伤：1.0 + 0.20 * N。
  SUBCASE("471 counter more damage scales per point") {
    {
      SpecializedSkill spec;
      spec.skill_id = 4;
      spec.allocated_points[471] = 3; // 1.0 + 0.60
      BakedSkillProfile profile{};
      SkillSpecializationBaker::Bake(registry, player, 4, &spec, profile, nullptr);
      CHECK(profile.more_damage_mult == doctest::Approx(1.60f));
    }
    {
      // 未点 471：乘算基准保持 1.0，不得产生无来源反击增伤。
      SpecializedSkill spec;
      spec.skill_id = 4;
      BakedSkillProfile profile{};
      SkillSpecializationBaker::Bake(registry, player, 4, &spec, profile, nullptr);
      CHECK(profile.more_damage_mult == doctest::Approx(1.0f));
    }
  }

  // 470 反击剑气数为交付基准 5，与节点是否点亮、点数多少无关。
  SUBCASE("470 counter sword base count stays 5") {
    {
      SpecializedSkill spec;
      spec.skill_id = 4;
      BakedSkillProfile profile{};
      SkillSpecializationBaker::Bake(registry, player, 4, &spec, profile, nullptr);
      CHECK(profile.delivery.sub_count == 5);
    }
    {
      SpecializedSkill spec;
      spec.skill_id = 4;
      spec.allocated_points[470] = 1;
      BakedSkillProfile profile{};
      SkillSpecializationBaker::Bake(registry, player, 4, &spec, profile, nullptr);
      CHECK(profile.delivery.sub_count == 5);
    }
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Skill 5 Infinite Blades UMR Baking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();

  entt::registry registry;
  const auto player = registry.create();

  // 引导基准每秒法耗 20：500 折扣 20 * (1 - 0.10N)。
  SUBCASE("500 mana cost discount") {
    SpecializedSkill spec;
    spec.skill_id = 5;
    spec.allocated_points[500] = 2; // 20 * (1 - 0.20) = 16
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 5, &spec, profile, nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(16.0f));
  }

  // 502 增伤：1.0 + 0.10N。
  SUBCASE("502 more damage") {
    SpecializedSkill spec;
    spec.skill_id = 5;
    spec.allocated_points[502] = 2; // 1.0 + 0.20
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 5, &spec, profile, nullptr);
    CHECK(profile.more_damage_mult == doctest::Approx(1.20f));
  }

  // 510 代价换伤害：法耗 20 * (1 + 0.30N)，索敌基准 450 不变。
  SUBCASE("510 mana penalty and lock range baseline") {
    SpecializedSkill spec;
    spec.skill_id = 5;
    spec.allocated_points[510] = 1; // 20 * 1.30 = 26
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 5, &spec, profile, nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(26.0f));
    CHECK(profile.delivery.range == doctest::Approx(450.0f));
  }

  // 511 双算子：range 450 * (1 + 0.15N)，speed 1000 * (1 + 0.25N)。
  SUBCASE("511 range and speed multipliers") {
    SpecializedSkill spec;
    spec.skill_id = 5;
    spec.allocated_points[511] = 3;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 5, &spec, profile, nullptr);
    CHECK(profile.delivery.range == doctest::Approx(652.5f));  // 450 * 1.45
    CHECK(profile.delivery.speed == doctest::Approx(1750.0f)); // 1000 * 1.75
  }

  // 533 巨剑：增伤 1.0 + 1.50N = 2.5，索敌半径保底取机制表 giant_radius=70。
  SUBCASE("533 more damage and giant radius floor") {
    SpecializedSkill spec;
    spec.skill_id = 5;
    spec.allocated_points[533] = 1;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 5, &spec, profile, nullptr);
    CHECK(profile.more_damage_mult == doctest::Approx(2.50f));
    CHECK(profile.area_radius == doctest::Approx(70.0f));
  }

  // 554/555：暴击率 +1.0N，暴伤 +0.20N。
  SUBCASE("554 bonus crit and 555 bonus crit damage") {
    SpecializedSkill spec;
    spec.skill_id = 5;
    spec.allocated_points[554] = 1; // +1.0
    spec.allocated_points[555] = 2; // +0.4
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 5, &spec, profile, nullptr);
    CHECK(profile.delivery.bonus_crit == doctest::Approx(1.0f));
    CHECK(profile.delivery.bonus_crit_damage == doctest::Approx(0.4f));
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Skill 6 Sword Array UMR Baking") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  EnsureModifierRuntimeForSkillSpec();

  entt::registry registry;
  const auto player = registry.create();

  // 600 持续时间加算：5.0 + 0.5N。
  SUBCASE("600 duration flat") {
    SpecializedSkill spec;
    spec.skill_id = 6;
    spec.allocated_points[600] = 4; // 5.0 + 2.0
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 6, &spec, profile, nullptr);
    CHECK(profile.delivery.duration == doctest::Approx(7.0f));
  }

  // 601 阵半径：150 * (1 + 0.15N)。
  SUBCASE("601 area radius") {
    SpecializedSkill spec;
    spec.skill_id = 6;
    spec.allocated_points[601] = 4; // 150 * 1.60
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 6, &spec, profile, nullptr);
    CHECK(profile.area_radius == doctest::Approx(240.0f));
  }

  // 602 增伤：1.0 + 0.10N。
  SUBCASE("602 more damage") {
    SpecializedSkill spec;
    spec.skill_id = 6;
    spec.allocated_points[602] = 2; // 1.0 + 0.20
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 6, &spec, profile, nullptr);
    CHECK(profile.more_damage_mult == doctest::Approx(1.20f));
  }

  // 603 同时携带法耗折扣与施法 range 迁移：数值需与迁移前一致。
  // mana 30 * (1 - 0.05N) = 24；range 400 * (1 + 0.10N) = 560（迁移后暂无消费端）。
  SUBCASE("603 mana discount and cast range parity") {
    SpecializedSkill spec;
    spec.skill_id = 6;
    spec.allocated_points[603] = 4;
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 6, &spec, profile, nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(24.0f));
    CHECK(profile.delivery.range == doctest::Approx(560.0f));
    // 未点 603 时两基准保持 30 / 400，迁移不改变默认行为。
    SpecializedSkill empty;
    empty.skill_id = 6;
    BakedSkillProfile base{};
    SkillSpecializationBaker::Bake(registry, player, 6, &empty, base, nullptr);
    CHECK(base.effective_mana_cost == doctest::Approx(30.0f));
    CHECK(base.delivery.range == doctest::Approx(400.0f));
  }

  // 610 增伤惩罚：1.0 * (1 - 0.15N)。
  SUBCASE("610 more damage penalty") {
    SpecializedSkill spec;
    spec.skill_id = 6;
    spec.allocated_points[610] = 1; // 1 - 0.15
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 6, &spec, profile, nullptr);
    CHECK(profile.more_damage_mult == doctest::Approx(0.85f));
  }

  // 611 法耗惩罚：30 * (1 + 0.30N)。
  SUBCASE("611 mana penalty") {
    SpecializedSkill spec;
    spec.skill_id = 6;
    spec.allocated_points[611] = 1; // 30 * 1.30 = 39
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 6, &spec, profile, nullptr);
    CHECK(profile.effective_mana_cost == doctest::Approx(39.0f));
  }

  // 634 阵半径惩罚：150 * (1 - 0.30N)。
  SUBCASE("634 area radius penalty") {
    SpecializedSkill spec;
    spec.skill_id = 6;
    spec.allocated_points[634] = 1; // 150 * 0.70
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 6, &spec, profile, nullptr);
    CHECK(profile.area_radius == doctest::Approx(105.0f));
  }

  // 653 增伤惩罚：1.0 * (1 - 0.50N)。
  SUBCASE("653 more damage penalty") {
    SpecializedSkill spec;
    spec.skill_id = 6;
    spec.allocated_points[653] = 1; // 1 - 0.50
    BakedSkillProfile profile{};
    SkillSpecializationBaker::Bake(registry, player, 6, &spec, profile, nullptr);
    CHECK(profile.more_damage_mult == doctest::Approx(0.50f));
  }
}

TEST_CASE("[Unit] SkillSpecializationBaker - Skill 6 Delivery Floors At Zero") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  // 注入合成 UMR：技能 6 每点持续时间平减 -100s，弹速乘性偏移 -2。
  // 600 节点取 1 点（600 max_points=4，合法），则：
  //   duration 结果 = 5.0 + (-100 * 1) = -95，
  //   speed 乘性系数 = 1 + (-2) * 1 = -1（基准 300 * (-1)），
  // 两者均为非法负值，必须在 Baker 端被 max(0) 钳制为 0。速度偏移刻意取 -2 而非 -1：
  // 系数取 0 时乘积仍为 0，无法区分是否真的施加了下限保护；取负系数才能真正触发钳制。
  // 合成 blob 会替换整个运行时，故 600 的 canonical 加算 0.5 不参与，duration 只由注入算子决定。
  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
      BuildDeliveryRuntimeBlob(-100.0f, -2.0f)));

  entt::registry registry;
  const auto player = registry.create();
  SpecializedSkill spec;
  spec.skill_id = 6;
  spec.allocated_points[600] = 1;
  BakedSkillProfile profile{};
  SkillSpecializationBaker::Bake(registry, player, 6, &spec, profile, nullptr);

  // 基准 duration 5.0 + (-95) = -95 → 钳制 0；基准 speed 300 * (-1) = -300 → 钳制 0。
  // 若删除 Baker 的 max(0) 包装，本用例两处断言都会失败。
  CHECK(profile.delivery.duration == doctest::Approx(0.0f));
  CHECK(profile.delivery.speed == doctest::Approx(0.0f));

  // 恢复真实运行时产物，避免合成数据泄漏到其它用例。
  REQUIRE(ReloadModifierRuntimeFromAsset());
}

} // namespace NoMoreDay

