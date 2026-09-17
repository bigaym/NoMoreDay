#include "TestCommon.hpp"

#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

namespace NoMoreDay {
namespace {

// 真实装备词缀记录：法力消耗乘算 0.9，限定技能 1、需 Hit 标签、主手槽位。
constexpr uint32_t kManaCostRecordId = 1001001u;
constexpr uint32_t kFlowingThrustSkillId = 1u;
constexpr float kBaseManaCost = 5.0f;
constexpr float kFoldedManaCost = 4.5f;

// 主手装配/卸下带降耗词缀的装备。
void SetMainHandManaCostAffix(entt::registry &registry,
                              const entt::entity player, const bool equipped) {
  auto &equipment = registry.get_or_emplace<EquipmentComponent>(player);
  if (!equipped) {
    equipment.Set(EquipmentSlot::MainHand, entt::null);
    return;
  }

  const entt::entity item = registry.create();
  auto &itemComponent = registry.emplace<ItemComponent>(item);

  Affix affix;
  affix.type = AffixType::Strength;
  affix.modifier_record_ids = {kManaCostRecordId};
  itemComponent.affixes.push_back(affix);

  equipment.Set(EquipmentSlot::MainHand, item);
}

// 最小可施法角色：技能 1 入槽、已烘焙、法力 100。
void SetupCaster(entt::registry &registry, const entt::entity player) {
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kFlowingThrustSkillId;
  active.slots[0].current_charges = 1;
  active.specialized_slots[0].skill_id = kFlowingThrustSkillId;

  registry.emplace<CombatStats>(player).mana = 100.0f;
  SkillSystem::RebakeSkillProfiles(registry, player);
}

} // namespace

TEST_CASE("[Unit] SkillManaCostSettlement - equipment fold into baked profile") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(SkillRegistry::Get().GetSkill(kFlowingThrustSkillId) != nullptr);
  REQUIRE(ReloadModifierRuntimeFromAsset());

  entt::registry registry;
  const entt::entity player = registry.create();

  // 无装备时保持静态基础值。
  BakedSkillProfile plain{};
  SkillSpecializationBaker::Bake(registry, player, kFlowingThrustSkillId,
                                 nullptr, plain, nullptr);
  CHECK(plain.effective_mana_cost == doctest::Approx(kBaseManaCost));

  // 装备降耗词缀后乘算折叠进烘焙结果。
  SetMainHandManaCostAffix(registry, player, true);
  BakedSkillProfile folded{};
  SkillSpecializationBaker::Bake(registry, player, kFlowingThrustSkillId,
                                 nullptr, folded, nullptr);
  CHECK(folded.effective_mana_cost == doctest::Approx(kFoldedManaCost));

  // 卸下后必须恢复基础值，乘算不得被缓存在别处。
  SetMainHandManaCostAffix(registry, player, false);
  BakedSkillProfile unequipped{};
  SkillSpecializationBaker::Bake(registry, player, kFlowingThrustSkillId,
                                 nullptr, unequipped, nullptr);
  CHECK(unequipped.effective_mana_cost == doctest::Approx(kBaseManaCost));
}

TEST_CASE("[Unit] SkillManaCostSettlement - bake stays idempotent with equipment") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(ReloadModifierRuntimeFromAsset());

  entt::registry registry;
  const entt::entity player = registry.create();
  SetMainHandManaCostAffix(registry, player, true);

  BakedSkillProfile first{};
  SkillSpecializationBaker::Bake(registry, player, kFlowingThrustSkillId,
                                 nullptr, first, nullptr);
  BakedSkillProfile second{};
  SkillSpecializationBaker::Bake(registry, player, kFlowingThrustSkillId,
                                 nullptr, second, nullptr);

  CHECK(first == second);
  CHECK(first.effective_mana_cost == doctest::Approx(kFoldedManaCost));
}

TEST_CASE("[Unit] SkillManaCostSettlement - rebake reflects equipment change") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(ReloadModifierRuntimeFromAsset());

  entt::registry registry;
  const entt::entity player = registry.create();
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kFlowingThrustSkillId;
  active.slots[0].current_charges = 1;
  active.specialized_slots[0].skill_id = kFlowingThrustSkillId;

  SkillSystem::RebakeSkillProfiles(registry, player);
  const auto *before = SkillSystem::GetBakedSkillProfile(
      registry, player, kFlowingThrustSkillId);
  REQUIRE(before != nullptr);
  CHECK(before->effective_mana_cost == doctest::Approx(kBaseManaCost));

  // 装备变更后重烘焙必须产出新值（对应 StatsDirty 触发的重算链路）。
  SetMainHandManaCostAffix(registry, player, true);
  SkillSystem::RebakeSkillProfiles(registry, player);
  const auto *after = SkillSystem::GetBakedSkillProfile(
      registry, player, kFlowingThrustSkillId);
  REQUIRE(after != nullptr);
  CHECK(after->effective_mana_cost == doctest::Approx(kFoldedManaCost));
}

TEST_CASE("[Unit] SkillManaCostSettlement - TryCast settles folded mana cost") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  REQUIRE(ReloadModifierRuntimeFromAsset());

  entt::registry registry;

  SUBCASE("without equipment") {
    const entt::entity player = registry.create();
    SetupCaster(registry, player);

    auto &stats = registry.get<CombatStats>(player);
    REQUIRE(SkillSystem::TryCast(registry, player, 0));
    CHECK(stats.mana == doctest::Approx(100.0f - kBaseManaCost));
  }

  SUBCASE("with mana cost affix") {
    const entt::entity player = registry.create();
    SetMainHandManaCostAffix(registry, player, true);
    SetupCaster(registry, player);

    auto &stats = registry.get<CombatStats>(player);
    REQUIRE(SkillSystem::TryCast(registry, player, 0));
    // 乘算若被重复应用会得到 95.95；此处证明结算只有一处。
    CHECK(stats.mana == doctest::Approx(100.0f - kFoldedManaCost));
  }
}

TEST_CASE("[Unit] SkillManaCostSettlement - specialization and equipment compose "
          "multiplicatively") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  REQUIRE(SkillRegistry::Get().GetSkill(kFlowingThrustSkillId) != nullptr);
  REQUIRE(ReloadModifierRuntimeFromAsset());

  entt::registry registry;
  const entt::entity player = registry.create();

  // 专精降耗（SKILL_MANA_COST_MULT）与装备降耗（MANA_COST_MULT）为同一条乘算链：
  // 前者烘焙进 effective_mana_cost，后者在烘焙收尾继续乘算，绝不做加性叠加。
  // 说明：UMR-SKILL-BATCH-2 的 402 专精折扣限定技能 4，而真实资产中唯一带降耗的
  // 装备记录 1001001 限定技能 1，无技能 4 装备记录；故以技能 1 的 101（同 opcode
  // SKILL_MANA_COST_MULT、每点 15%）等价验证乘算复合语义，避免注入合成运行时数据。
  SpecializedSkill spec;
  spec.skill_id = kFlowingThrustSkillId;
  spec.allocated_points[101] = 3; // 1 - 0.15 * 3 = 0.55

  // 仅专精：5.0 * 0.55 = 2.75。
  BakedSkillProfile specOnly{};
  SkillSpecializationBaker::Bake(registry, player, kFlowingThrustSkillId, &spec,
                                 specOnly, nullptr);
  CHECK(specOnly.effective_mana_cost == doctest::Approx(2.75f));

  // 叠装备降耗 0.9：2.75 * 0.9 = 2.475；若为加性叠加会得到 2.25。
  SetMainHandManaCostAffix(registry, player, true);
  BakedSkillProfile composed{};
  SkillSpecializationBaker::Bake(registry, player, kFlowingThrustSkillId, &spec,
                                 composed, nullptr);
  CHECK(composed.effective_mana_cost == doctest::Approx(2.475f));
  CHECK_FALSE(composed.effective_mana_cost == doctest::Approx(2.25f));
}

} // namespace NoMoreDay
