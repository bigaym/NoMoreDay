// ============================================================================
// SkillSpecializationStatModifierTests.cpp
//
// 用途：守护技能专精 stat_modifiers 的生效契约——SkillOnly / GlobalAlways /
// GlobalWhileBuffActive 三种作用域下的节点修饰符必须按分配点数缩放、按上下文技能
// 隔离，并受互斥基石与 Transmuter 选择约束；同时守护 F-06 修复后“未折叠来源”的
// 条件修饰符不再被 is_baked 启发式静默跳过，以及 F-02 哨兵档案的有效性判定。
//
// 背景：StatsSystem::GetStatWithTags 会同时消费多类来源（ModifierList、星盘、
// SkillModifierComponent、GlobalModifierComponent、专精节点）。修复前
// apply_if_tags_match 用“required_tags == Tag::None ⇒ is_baked”推断来源是否已被
// AttributePipeline::Calculate 折叠进基础属性，导致专精节点与
// SkillModifierComponent / GlobalModifierComponent 中所有 required_tags 为空的
// 修饰符被当作已折叠而跳过。修复改为由调用方显式传入 source_prebaked，仅对真正
// 预折叠的来源保留该短路。
//
// 用例分类（可证伪性声明，避免把相邻契约锁定误当 F-06 证据）：
//   F-06 可证伪证据（修复前必然失败，返回值停留在无加成基线）：
//     1  SkillOnly 节点按分配点数缩放
//     2  SkillOnly 按上下文技能隔离
//     5  互斥基石丢弃落败节点
//     6  非激活 Transmuter 阻止节点
//     7  SkillModifierComponent 空标签生效
//     8  GlobalModifierComponent 御剑标签生效
//   邻接契约锁定（不是 F-06 回归证据，仅防止相邻语义回退）：
//     3  GlobalAlways 作用域谓词（见用例内注释）
//     4  GlobalWhileBuffActive 作用域谓词 + stat 层缓存刷新（见用例内注释）
//     9  F-02 哨兵档案有效性判定与施法/冷却回退
//     10 资产不变量：专精节点不同时用 stat_modifiers 与 UMR 算子表达效果
//   测试夹具新增（见用例内注释）：
//     11 required_tags（非 None）专精节点修饰符的匹配/不匹配两侧
//
// 用例命名遵循 doctest 约定：前缀 `[Unit] `；值断言用 CHECK，前置条件用 REQUIRE。
// 注意避免 `CHECK(a && b)`（会触发 MSVC doctest C2338）。
// ============================================================================

#include "TestCommon.hpp"

#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillContract.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <set>
#include <utility>

namespace NoMoreDay {
namespace {

// 专精树技能：节点 1000/1007/1021/1022 来自 assets/data/mastery_skill_trees.json。
constexpr uint32_t kMasterySkillId = 10;
// 另一个已注册技能，用于验证 SkillOnly 的跨技能隔离。
constexpr uint32_t kOtherSkillId = 2;
// 绝影（技能 9）：GlobalWhileBuffActive 的形态窗口来源。
constexpr uint32_t kTranceSkillId = 9;
// 光束引导技能，用于 GlobalWhileBuffActive 的引导窗口。
constexpr uint32_t kBeamSkillId = 3;
// 技能表已注册且可正常烘焙的技能。
constexpr uint32_t kRegisteredBakedSkillId = 2;
// 技能表未注册，RebakeSkillProfiles 会为其写入哨兵档案。
constexpr uint32_t kUnregisteredSkillId = 999999u;
// 施法/冷却回退用例使用的已注册技能（静态法力 5、冷却 4s）。
constexpr uint32_t kCastSkillId = 1;

// 技能 10 的专精节点。
constexpr uint32_t kArmorPenNodeId = 1000;        // 每点 +5% 护甲穿透（SkillOnly）
constexpr uint32_t kKeystoneNodeId = 1007;        // 排除组 1 的基石（node_id 更低）
constexpr uint32_t kAccuracyNodeId = 1021;        // 排除组 1 的 Transmuter，+20% 命中
constexpr uint32_t kOtherTransmuterNodeId = 1022; // 同组另一个 Transmuter
// 技能 9 的唯一 GlobalWhileBuffActive 契约节点（skills.json 节点 991）；资产
// talent_tree 节点未携带 stat_modifiers，用例 4 经生产 API 注入测试夹具修饰符。
constexpr uint32_t kGlobalWhileBuffNodeId = 991;

StatModifier MakeStatModifier(StatType type, ModifierMode mode, float value,
                              Tag required_tags = Tag::None) {
  StatModifier mod;
  mod.type = type;
  mod.mode = mode;
  mod.value = value;
  mod.required_tags = required_tags;
  return mod;
}

// 创建带专精分配的施法者：specialized_slots[0] 绑定 skill_id，并按 nodes 填充
// allocated_points。StatsSystem 只读取 allocated_points 与节点表，不校验点数上限。
entt::entity MakeSpecializedCaster(
    entt::registry &registry, uint32_t skill_id,
    std::initializer_list<std::pair<uint32_t, int>> nodes) {
  const entt::entity entity = registry.create();
  registry.emplace<CombatStats>(entity);
  auto &active = registry.emplace<ActiveSkillsComponent>(entity);
  active.specialized_slots[0].skill_id = skill_id;
  for (const auto &[node_id, points] : nodes) {
    active.specialized_slots[0].allocated_points[node_id] = points;
  }
  return entity;
}

} // namespace

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - SkillOnly Node Scales With "
    "Allocated Points") {
  TestSetupScope scope;

  entt::registry registry;
  const entt::entity one_point =
      MakeSpecializedCaster(registry, kMasterySkillId, {{kArmorPenNodeId, 1}});
  registry.get<CombatStats>(one_point).armor_pen = 100.0f;
  const entt::entity four_points =
      MakeSpecializedCaster(registry, kMasterySkillId, {{kArmorPenNodeId, 4}});
  registry.get<CombatStats>(four_points).armor_pen = 100.0f;

  // 节点 1000 是每点 +5% 护甲穿透的 SkillOnly 节点；施加值按分配点数缩放，
  // 因此 1 点应是 100 -> 105，4 点应是 100 -> 120。
  const float one = StatsSystem::GetStatWithTags(
      registry, one_point, StatType::ArmorPenetration, Tag::None, kMasterySkillId);
  const float four = StatsSystem::GetStatWithTags(
      registry, four_points, StatType::ArmorPenetration, Tag::None, kMasterySkillId);
  CHECK(one == doctest::Approx(105.0f));
  CHECK(four == doctest::Approx(120.0f));
  CHECK(four > one);
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - SkillOnly Node Isolated To Source "
    "Skill") {
  TestSetupScope scope;

  entt::registry registry;
  const entt::entity caster =
      MakeSpecializedCaster(registry, kMasterySkillId, {{kArmorPenNodeId, 1}});
  registry.get<CombatStats>(caster).armor_pen = 100.0f;

  // 同一施法者只在专精技能 10 的上下文命中该节点；skill_id==0（无上下文）或
  // 其它技能都不得拿到加成，否则 SkillOnly 隔离失效。
  const float same_skill = StatsSystem::GetStatWithTags(
      registry, caster, StatType::ArmorPenetration, Tag::None, kMasterySkillId);
  const float no_skill = StatsSystem::GetStatWithTags(
      registry, caster, StatType::ArmorPenetration, Tag::None, 0u);
  const float other_skill = StatsSystem::GetStatWithTags(
      registry, caster, StatType::ArmorPenetration, Tag::None, kOtherSkillId);
  CHECK(same_skill == doctest::Approx(105.0f));
  CHECK(no_skill == doctest::Approx(100.0f));
  CHECK(other_skill == doctest::Approx(100.0f));
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - GlobalAlways Scope Applies In Any "
    "Context") {
  TestSetupScope scope;

  entt::registry registry;
  const entt::entity entity = registry.create();

  // 注意：本用例断言的是 SkillSystem::CanApplyScopePolicy，它与 StatsSystem.cpp
  // 内的 can_apply_scope 是逻辑等价的重复实现，因此只锁定作用域谓词语义，不是
  // F-06 回归证据。资产侧不存在任何 GlobalAlways 节点（全量扫描 0 处），无法经
  // GetStatWithTags 观测该作用域的属性值，故只能在此以谓词形式锁定契约。
  // GlobalAlways 不受上下文技能约束：无上下文与不匹配技能都应放行。
  CHECK(SkillSystem::CanApplyScopePolicy(registry, entity, 0u, kMasterySkillId,
                                         ScopePolicy::GlobalAlways));
  CHECK(SkillSystem::CanApplyScopePolicy(registry, entity, kOtherSkillId,
                                         kMasterySkillId, ScopePolicy::GlobalAlways));
  // 同一组参数下 SkillOnly 被拒绝，证明上面对 GlobalAlways 的放行是作用域语义
  // 而非无条件恒真。
  CHECK_FALSE(SkillSystem::CanApplyScopePolicy(registry, entity, kOtherSkillId,
                                               kMasterySkillId,
                                               ScopePolicy::SkillOnly));
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - GlobalWhileBuffActive Scope Tracks "
    "Buff Window") {
  TestSetupScope scope;

  entt::registry registry;
  const entt::entity entity = registry.create();

  // 注意（谓词部分）：以下断言走 SkillSystem::CanApplyScopePolicy，与 StatsSystem.cpp
  // 的 can_apply_scope 逻辑等价，只锁定“窗口开/关”的作用域谓词语义，不是 F-06 回归
  // 证据。真正的 stat 层缓存刷新契约见本用例末尾的 GetStatWithTags 断言。
  // 没有任何 buff 载体时窗口关闭。
  CHECK_FALSE(SkillSystem::CanApplyScopePolicy(registry, entity, kMasterySkillId,
                                               kTranceSkillId,
                                               ScopePolicy::GlobalWhileBuffActive));

  // 绝影形态窗口：remaining>0 时对来源技能 9 放行，窗口耗尽即关闭。
  auto &trance = registry.emplace<PhantomTranceComponent>(entity);
  trance.remaining = 3.0f;
  CHECK(SkillSystem::CanApplyScopePolicy(registry, entity, kMasterySkillId,
                                         kTranceSkillId,
                                         ScopePolicy::GlobalWhileBuffActive));
  trance.remaining = 0.0f;
  trance.enchant_remaining = 0.0f;
  CHECK_FALSE(SkillSystem::CanApplyScopePolicy(
      registry, entity, kMasterySkillId, kTranceSkillId,
      ScopePolicy::GlobalWhileBuffActive));

  // 光束引导窗口：BeamChannelComponent.skill_id 与来源技能一致时放行，不一致拒绝。
  auto &beam = registry.emplace<BeamChannelComponent>(entity);
  beam.skill_id = kBeamSkillId;
  CHECK(SkillSystem::CanApplyScopePolicy(registry, entity, kMasterySkillId,
                                         kBeamSkillId,
                                         ScopePolicy::GlobalWhileBuffActive));
  CHECK_FALSE(SkillSystem::CanApplyScopePolicy(
      registry, entity, kMasterySkillId, kOtherSkillId,
      ScopePolicy::GlobalWhileBuffActive));

  // ---- stat 层缓存刷新契约（plan §4.1 case 4 要求）----
  // 资产侧唯一 scope_policy==GlobalWhileBuffActive 的节点是技能 9 的 991
  // （skills.json:6319；GlobalAlways 全量扫描 0 处），但 991 的 talent_tree 节点
  // 未携带 stat_modifiers，因此资产层无法观测到窗口增益。此处不改动资产，而是通过
  // 生产 API SkillRegistry::GetMutableSkillTree 在内存中给该既有节点注入一条测试
  // 夹具修饰符（TestSetupScope 会在下个用例前重载技能表，不跨用例泄漏）。断言证明：
  // 窗口开启时读取并缓存增益，窗口关闭并置 StatsDirty 后经生产重算路径
  // （StatsSystem::update → Recalculate → ClearCache）刷新，增益严格消失。
  entt::registry stat_registry;
  const entt::entity baseline_entity = stat_registry.create();
  stat_registry.emplace<CombatStats>(baseline_entity).armor_pen = 100.0f;

  const entt::entity windowed_entity = stat_registry.create();
  stat_registry.emplace<CombatStats>(windowed_entity).armor_pen = 100.0f;
  auto &windowed_active =
      stat_registry.emplace<ActiveSkillsComponent>(windowed_entity);
  windowed_active.specialized_slots[0].skill_id = kTranceSkillId;
  windowed_active.specialized_slots[0]
      .allocated_points[kGlobalWhileBuffNodeId] = 1;
  auto &windowed_trance =
      stat_registry.emplace<PhantomTranceComponent>(windowed_entity);
  windowed_trance.remaining = 3.0f;

  SkillTreeDefinition *trance_tree =
      SkillRegistry::Get().GetMutableSkillTree(kTranceSkillId);
  REQUIRE(trance_tree != nullptr);
  auto trance_node_it = trance_tree->nodes.find(kGlobalWhileBuffNodeId);
  REQUIRE(trance_node_it != trance_tree->nodes.end());
  trance_node_it->second.stat_modifiers = {MakeStatModifier(
      StatType::ArmorPenetration, ModifierMode::Flat, 30.0f, Tag::None)};

  const float baseline = StatsSystem::GetStatWithTags(
      stat_registry, baseline_entity, StatType::ArmorPenetration, Tag::None, 0u);
  const float buffed = StatsSystem::GetStatWithTags(
      stat_registry, windowed_entity, StatType::ArmorPenetration, Tag::None, 0u);
  CHECK(baseline == doctest::Approx(100.0f));
  CHECK(buffed == doctest::Approx(130.0f));

  // 窗口结束：置 StatsDirty 并走生产重算路径。若 ClearCache 未随重算执行，下一次
  // 读取会命中旧缓存 130 而非基线，故该断言对“缓存刷新”可证伪。
  windowed_trance.remaining = 0.0f;
  windowed_trance.enchant_remaining = 0.0f;
  stat_registry.emplace<StatsDirty>(windowed_entity);
  StatsSystem::update(stat_registry);

  const float reverted = StatsSystem::GetStatWithTags(
      stat_registry, windowed_entity, StatType::ArmorPenetration, Tag::None, 0u);
  CHECK(reverted == doctest::Approx(baseline));
  CHECK(reverted < buffed);
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - Mutual Keystone Exclusion Drops "
    "Losing Node") {
  TestSetupScope scope;

  entt::registry registry;

  // 单独点出 1021（Transmuter，排除组 1）时该节点生效：命中 100 -> 120。
  const entt::entity solo =
      MakeSpecializedCaster(registry, kMasterySkillId, {{kAccuracyNodeId, 1}});
  registry.get<CombatStats>(solo).accuracy = 1.0f;
  const float solo_accuracy = StatsSystem::GetStatWithTags(
      registry, solo, StatType::Accuracy, Tag::None, kMasterySkillId);
  CHECK(solo_accuracy == doctest::Approx(120.0f));

  // 同组内 1007 的 node_id 更小，按互斥规则胜出，1021 的加成必须被丢弃。
  const entt::entity conflict = MakeSpecializedCaster(
      registry, kMasterySkillId, {{kKeystoneNodeId, 1}, {kAccuracyNodeId, 1}});
  registry.get<CombatStats>(conflict).accuracy = 1.0f;
  const float conflict_accuracy = StatsSystem::GetStatWithTags(
      registry, conflict, StatType::Accuracy, Tag::None, kMasterySkillId);
  CHECK(conflict_accuracy == doctest::Approx(100.0f));
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - Inactive Transmuter Blocks Node") {
  TestSetupScope scope;

  entt::registry registry;

  // 未指定运行时 Transmuter 时，1021 默认生效。
  const entt::entity active =
      MakeSpecializedCaster(registry, kMasterySkillId, {{kAccuracyNodeId, 1}});
  registry.get<CombatStats>(active).accuracy = 1.0f;
  const float active_accuracy = StatsSystem::GetStatWithTags(
      registry, active, StatType::Accuracy, Tag::None, kMasterySkillId);
  CHECK(active_accuracy == doctest::Approx(120.0f));

  // 运行时选择了同组的另一个 Transmuter 1022 时，1021 处于非激活态，不得贡献加成。
  const entt::entity inactive =
      MakeSpecializedCaster(registry, kMasterySkillId, {{kAccuracyNodeId, 1}});
  registry.get<CombatStats>(inactive).accuracy = 1.0f;
  auto &runtime = registry.emplace<SkillContractRuntimeComponent>(inactive);
  runtime.active_transmuter_node_by_skill[kMasterySkillId] =
      kOtherTransmuterNodeId;
  const float inactive_accuracy = StatsSystem::GetStatWithTags(
      registry, inactive, StatType::Accuracy, Tag::None, kMasterySkillId);
  CHECK(inactive_accuracy == doctest::Approx(100.0f));
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - SkillModifierComponent None Tags "
    "Applies") {
  TestSetupScope scope;

  entt::registry registry;

  const entt::entity caster = registry.create();
  registry.emplace<CombatStats>(caster).armor = 100.0f;
  const entt::entity source = registry.create();
  auto &source_mods = registry.emplace<SkillModifierComponent>(source);
  source_mods.stat_modifiers.push_back(
      MakeStatModifier(StatType::Armor, ModifierMode::Flat, 50.0f, Tag::None));

  // required_tags==None 的源实体修饰符表示“无前置标签、应无条件生效”，不是
  // “已被 AttributePipeline 折叠”。修复前它被 is_baked 启发式静默跳过，护甲停在 100。
  const float armor = StatsSystem::GetStatWithTags(
      registry, caster, StatType::Armor, Tag::None, 0u, source);
  CHECK(armor == doctest::Approx(150.0f));
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - GlobalModifierComponent SwordRiding "
    "Tags Applies In Stance") {
  TestSetupScope scope;

  entt::registry registry;

  const entt::entity entity = registry.create();
  registry.emplace<CombatStats>(entity).armor = 100.0f;
  auto &global = registry.emplace<GlobalModifierComponent>(entity);
  global.stat_modifiers.push_back(MakeStatModifier(
      StatType::Armor, ModifierMode::Flat, 25.0f, Tag::SwordRiding));
  auto &stance = registry.emplace<MovementStanceComponent>(entity);
  stance.stance = MovementStance::SwordRiding;

  // 御剑架势把 Tag::SwordRiding 并入查询标签，带该前置标签的装备词缀应生效。
  CHECK(StatsSystem::GetStatWithTags(registry, entity, StatType::Armor, Tag::None,
                                     0u) == doctest::Approx(125.0f));

  // 切回步行后查询标签不再包含 SwordRiding，同一修饰符必须失效。
  StatsSystem::ClearCache(registry, entity);
  stance.stance = MovementStance::Walking;
  CHECK(StatsSystem::GetStatWithTags(registry, entity, StatType::Armor, Tag::None,
                                     0u) == doctest::Approx(100.0f));
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - Sentinel Profile Rejected By Valid "
    "Accessor And Cast Fallback") {
  TestSetupScope scope;
  SkillBehaviorRegistry::Initialize();
  REQUIRE(ReloadModifierRuntimeFromAsset());

  entt::registry registry;

  // 未注册 skill_id 会被 RebakeSkillProfiles 写成哨兵：skill_id 命中但 is_baked==false。
  const entt::entity sentinel_entity = registry.create();
  auto &sentinel_active =
      registry.emplace<ActiveSkillsComponent>(sentinel_entity);
  sentinel_active.slots[0].id = kUnregisteredSkillId;
  SkillSystem::RebakeSkillProfiles(registry, sentinel_entity);

  const BakedSkillProfile *raw = SkillSystem::GetBakedSkillProfile(
      registry, sentinel_entity, kUnregisteredSkillId);
  REQUIRE(raw != nullptr);
  CHECK_FALSE(raw->is_baked);
  // 有效访问器必须拒绝哨兵，供调用方回退到静态数据。
  CHECK(SkillSystem::GetValidBakedSkillProfile(registry, sentinel_entity,
                                               kUnregisteredSkillId) == nullptr);

  // 已注册技能烘焙出的档案 is_baked==true，有效访问器必须返回它。
  const entt::entity baked_entity = registry.create();
  auto &baked_active = registry.emplace<ActiveSkillsComponent>(baked_entity);
  baked_active.slots[0].id = kRegisteredBakedSkillId;
  SkillSystem::RebakeSkillProfiles(registry, baked_entity);

  const BakedSkillProfile *baked = SkillSystem::GetValidBakedSkillProfile(
      registry, baked_entity, kRegisteredBakedSkillId);
  REQUIRE(baked != nullptr);
  CHECK(baked->is_baked);

  // 手工注入指向已注册技能的未烘焙档案（skill_id 命中但 is_baked==false），
  // 模拟槽位缓存中的过期哨兵。
  const entt::entity caster = registry.create();
  auto &combat = registry.emplace<CombatStats>(caster);
  combat.mana = 100.0f;
  auto &cast_active = registry.emplace<ActiveSkillsComponent>(caster);
  cast_active.slots[0].id = kCastSkillId;
  cast_active.slots[0].current_charges = 1;
  cast_active.specialized_slots[0].skill_id = kCastSkillId;
  cast_active.baked_profiles[0] = BakedSkillProfile{};
  cast_active.baked_profiles[0].skill_id = kCastSkillId;

  // 施法必须回退到技能静态法力消耗 5；若消费哨兵的 effective_mana_cost(0)，
  // 法力将保持不变（100）。
  REQUIRE(SkillSystem::TryCast(registry, caster, 0));
  CHECK(combat.mana == doctest::Approx(95.0f));

  // 冷却同理：技能 1 静态冷却 4s，回退后应重新填充为 4s，而不是哨兵的 0。
  const entt::entity cooler = registry.create();
  registry.emplace<CombatStats>(cooler);
  auto &cool_active = registry.emplace<ActiveSkillsComponent>(cooler);
  cool_active.slots[0].id = kCastSkillId;
  cool_active.slots[0].current_charges = 0;
  cool_active.slots[0].cooldown = 0.01f;
  cool_active.specialized_slots[0].skill_id = kCastSkillId;
  cool_active.baked_profiles[0] = BakedSkillProfile{};
  cool_active.baked_profiles[0].skill_id = kCastSkillId;

  SkillSystem::UpdateCooldowns(registry, 0.05f);
  CHECK(cool_active.slots[0].current_charges == 1);
  CHECK(cool_active.slots[0].cooldown == doctest::Approx(4.0f));
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - Mastery Asset Nodes Do Not Mix "
    "StatModifiers And UMR Ops") {
  // 数据不变量：同一专精节点不应同时用静态 stat_modifiers 和 UMR 投递算子表达效果，
  // 否则 StatsSystem 与 UMR 适配器会重复结算同一加成。此处直接读原始资产核对。
  std::ifstream mastery_stream("assets/data/mastery_skill_trees.json");
  std::ifstream umr_stream("assets/data/modifier_v2/skill_spec_modifiers.json");
  REQUIRE(mastery_stream.is_open());
  REQUIRE(umr_stream.is_open());

  const nlohmann::json mastery =
      nlohmann::json::parse(mastery_stream, nullptr, false);
  const nlohmann::json umr =
      nlohmann::json::parse(umr_stream, nullptr, false);
  REQUIRE_FALSE(mastery.is_discarded());
  REQUIRE_FALSE(umr.is_discarded());

  // 采集所有携带非空 stat_modifiers 的专精节点 (skill_id, node_id)。
  std::set<std::pair<uint32_t, uint32_t>> mastery_modifier_nodes;
  for (const auto &skill : mastery.value("skills", nlohmann::json::array())) {
    if (!skill.is_object()) {
      continue;
    }
    const uint32_t skill_id = skill.value("skill_id", 0u);
    for (const auto &node : skill.value("talent_tree", nlohmann::json::array())) {
      if (!node.is_object()) {
        continue;
      }
      const auto &mods = node.value("stat_modifiers", nlohmann::json::array());
      if (!mods.is_array() || mods.empty()) {
        continue;
      }
      mastery_modifier_nodes.emplace(skill_id, node.value("id", 0u));
    }
  }

  // 采集所有携带投递算子的 UMR 目标 (skill_id, node_id)；空 skill_id_whitelist
  // 视为匹配任意技能。
  std::set<std::pair<uint32_t, uint32_t>> umr_delivery_pairs;
  std::set<uint32_t> umr_wildcard_nodes;
  for (const auto &record : umr.value("records", nlohmann::json::array())) {
    if (!record.is_object()) {
      continue;
    }
    const auto &ops = record.value("ops", nlohmann::json::array());
    const auto &filters = record.value("filters", nlohmann::json::object());
    if (!filters.is_object()) {
      continue;
    }
    const auto &nodes = filters.value("node_id_whitelist", nlohmann::json::array());
    if (!ops.is_array() || ops.empty() || !nodes.is_array() || nodes.empty()) {
      continue;
    }
    const auto &skills = filters.value("skill_id_whitelist", nlohmann::json::array());
    const bool wildcard_skills = !skills.is_array() || skills.empty();
    for (const auto &node_json : nodes) {
      const uint32_t node_id = node_json.get<uint32_t>();
      if (wildcard_skills) {
        umr_wildcard_nodes.insert(node_id);
        continue;
      }
      for (const auto &skill_json : skills) {
        umr_delivery_pairs.emplace(skill_json.get<uint32_t>(), node_id);
      }
    }
  }

  // 两侧都必须真的采到数据，交集断言才不是恒真的空集比较。
  CHECK(mastery_modifier_nodes.size() > 0u);
  CHECK(umr_delivery_pairs.size() > 0u);

  size_t conflicts = 0;
  for (const auto &[skill_id, node_id] : mastery_modifier_nodes) {
    if (umr_delivery_pairs.count({skill_id, node_id}) > 0 ||
        umr_wildcard_nodes.count(node_id) > 0) {
      ++conflicts;
    }
  }
  CHECK(conflicts == 0);
}

TEST_CASE(
    "[Unit] SkillSpecializationStatModifier - Conditional RequiredTags Node "
    "Modifier Matches Only In Tag Context") {
  TestSetupScope scope;
  entt::registry registry;

  // 资产扫描：mastery_skill_trees.json 与 skills.json 的专精节点 stat_modifiers
  // 均未声明 required_tags（两文件 required_tags 出现次数为 0），故不存在真实的
  // 条件专精修饰符。此处不改动资产，通过生产 API GetMutableSkillTree 在内存中把
  // 既有节点 1000（技能10，SkillOnly）的修饰符替换为 required_tags==SwordRiding
  // 的测试夹具，以覆盖 required_tags（非 None）的匹配/不匹配两侧。
  SkillTreeDefinition *mastery_tree =
      SkillRegistry::Get().GetMutableSkillTree(kMasterySkillId);
  REQUIRE(mastery_tree != nullptr);
  auto node_it = mastery_tree->nodes.find(kArmorPenNodeId);
  REQUIRE(node_it != mastery_tree->nodes.end());
  node_it->second.stat_modifiers = {MakeStatModifier(
      StatType::ArmorPenetration, ModifierMode::Flat, 40.0f, Tag::SwordRiding)};

  // 匹配侧：御剑架势把 Tag::SwordRiding 并入查询标签，条件修饰符必须生效。该断言
  // 在 F-06 修复前会失败（旧 is_baked 启发式见 player_tags 含该标签即跳过）。
  const entt::entity matching =
      MakeSpecializedCaster(registry, kMasterySkillId, {{kArmorPenNodeId, 1}});
  registry.get<CombatStats>(matching).armor_pen = 100.0f;
  registry.emplace<MovementStanceComponent>(matching).stance =
      MovementStance::SwordRiding;
  CHECK(StatsSystem::GetStatWithTags(registry, matching,
                                     StatType::ArmorPenetration, Tag::None,
                                     kMasterySkillId) == doctest::Approx(140.0f));

  // 不匹配侧：步行架势不携带该标签，同一修饰符必须被跳过（自 F-06 前后行为一致，
  // 属契约锁定而非 F-06 证据）。使用独立实体以避免与匹配侧共用缓存键。
  const entt::entity non_matching =
      MakeSpecializedCaster(registry, kMasterySkillId, {{kArmorPenNodeId, 1}});
  registry.get<CombatStats>(non_matching).armor_pen = 100.0f;
  registry.emplace<MovementStanceComponent>(non_matching).stance =
      MovementStance::Walking;
  CHECK(StatsSystem::GetStatWithTags(registry, non_matching,
                                     StatType::ArmorPenetration, Tag::None,
                                     kMasterySkillId) == doctest::Approx(100.0f));
}

} // namespace NoMoreDay
