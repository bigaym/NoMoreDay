// 技能 11（天剑降临）统一施法信封与节点功能测试（计划 A2-2 的 T9.1/T9.4）。
//
// 覆盖三部分：
// 1) 生产 ResolveHeavenlySwordCastSpec 的点读 / 点亮标志映射（生成表驱动）；
//    机制系数已从 SpecState POD 剥离（Track A-02 §2.3），改由 DoCast 经 GetMech 本地读取，
//    故本文件不再断言 spec 上的机制/回退字段。
// 2) 外置到 skill_mechanics.json 技能 11 的调平数值回归（GetMech）。
// 3) 外置数值经施法写入 HeavenlySwordFieldComponent 的行为等价。
// 元素节点（1121/1122/1123）的完整战斗行为由 tests/unit/HeavenlySwordClosureTests.cpp 覆盖，
// 本文件聚焦统一施法信封与数值外置接线。

#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"

#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/components/PersistentFieldComponents.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = skills::kHeavenlySwordSkillId;
namespace Nodes = skills::HeavenlySwordNodes;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
}

// 组装技能 11 施法者：剑意资源 + 天剑专精 + 指定节点投入。
void ConfigureCaster(entt::registry &registry, entt::entity caster,
                     const std::unordered_map<uint32_t, int> &allocated_nodes = {},
                     int blade_tiers = 5) {
  auto &mastery = registry.emplace<BladeMasteryComponent>(caster);
  mastery.selected = BladeMasteryId::HeavenlySword;
  mastery.heavenly_attunement = BladeAttunement::None;

  auto &resource = registry.emplace<BladeResourceComponent>(caster);
  resource.kind = BladeResourceKind::SpiritBladeTier;
  resource.current = blade_tiers;
  resource.max = 10;

  std::vector<std::pair<uint32_t, int>> allocated;
  allocated.reserve(allocated_nodes.size());
  for (const auto &[node, points] : allocated_nodes) {
    allocated.emplace_back(node, points);
  }
  test::skill_keynode_matrix::ConfigureSpecialization(registry, caster, kSkillId,
                                                      allocated);
}

void CastHeavenlySword(entt::registry &registry, entt::entity caster,
                       Vector2 target_pos) {
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = caster;
  exec.target_pos = target_pos;

  auto cast = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(cast != nullptr);
  cast(registry, caster, exec);
}

const HeavenlySwordFieldComponent *FindField(const entt::registry &registry) {
  const auto view = registry.view<HeavenlySwordFieldComponent>();
  if (view.begin() == view.end()) {
    return nullptr;
  }
  return &view.get<HeavenlySwordFieldComponent>(*view.begin());
}

} // namespace

TEST_CASE("[Functional] Skill 11 - Cast spec resolves point and flag bindings") {
  entt::registry registry;
  const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
  ConfigureCaster(registry, owner,
                  {{Nodes::SwordCoreCalibration, 3},
                   {Nodes::SkyEdgeInfusion, 4},
                   {Nodes::OverflowingTiers, 2},
                   {Nodes::EnduringHeaven, 5},
                   {Nodes::ElementalRazing, 6},
                   {Nodes::SkyRendAftershock, 7},
                   {Nodes::CycleOfAllForms, 1},
                   {Nodes::SkyPiercingFall, 1},
                   {Nodes::ArraySynchrony, 1},
                   {Nodes::LightningTribunal, 1}});

  const skills::HeavenlySwordCastSpec spec =
      skills::ResolveHeavenlySwordCastSpec(registry, owner);

  CHECK(spec.swordCoreCalibrationPoints == 3);
  CHECK(spec.skyEdgeInfusionPoints == 4);
  CHECK(spec.overflowingTiersPoints == 2);
  CHECK(spec.enduringHeavenPoints == 5);
  CHECK(spec.elementalRazingPoints == 6);
  CHECK(spec.skyRendAftershockPoints == 7);
  CHECK(spec.spinningHeavensPoints == 0);
  CHECK(spec.cycleOfAllForms == true);
  CHECK(spec.skyPiercingFall == true);
  CHECK(spec.arraySynchrony == true);
  CHECK(spec.lightningTribunal == true);
  CHECK(spec.frozenDominion == false);
  CHECK(spec.solarIncineration == false);
}

TEST_CASE("[Functional] Skill 11 - externalized mechanics match legacy literals") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  // 逐 key 断言外置值与迁移前字面量等价；技能级默认参数位于 node 0。
  CHECK(skills::GetMech(11, 1100, "impact_stability_per_point", -1.0f) ==
        doctest::Approx(0.10f));
  CHECK(skills::GetMech(11, 1101, "field_radius_range_per_point", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(11, 1102, "impact_damage_per_point_per_tier", -1.0f) ==
        doctest::Approx(0.04f));
  CHECK(skills::GetMech(11, 1103, "tick_interval_reduction_per_point", -1.0f) ==
        doctest::Approx(0.05f));
  CHECK(skills::GetMech(11, 1104, "center_damage_per_point", -1.0f) ==
        doctest::Approx(0.10f));
  CHECK(skills::GetMech(11, 1105, "elite_impact_damage_per_point", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(11, 1105, "elite_field_damage_per_point", -1.0f) ==
        doctest::Approx(0.04f));
  CHECK(skills::GetMech(11, 1106, "slow_duration", -1.0f) ==
        doctest::Approx(2.0f));
  CHECK(skills::GetMech(11, 1106, "slow_percent_per_point", -1.0f) ==
        doctest::Approx(10.0f));
  CHECK(skills::GetMech(11, 1107, "field_radius_mult", -1.0f) ==
        doctest::Approx(0.7f));
  CHECK(skills::GetMech(11, 1107, "impact_damage_bonus", -1.0f) ==
        doctest::Approx(0.35f));
  CHECK(skills::GetMech(11, 1108, "scar_damage_per_point", -1.0f) ==
        doctest::Approx(0.18f));
  CHECK(skills::GetMech(11, 1108, "scar_base_damage", -1.0f) ==
        doctest::Approx(32.0f));
  CHECK(skills::GetMech(11, 1108, "scar_damage_per_tier", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(11, 1108, "scar_elite_damage_per_strike", -1.0f) ==
        doctest::Approx(0.20f));
  CHECK(skills::GetMech(11, 1108, "scar_interval", -1.0f) ==
        doctest::Approx(0.15f));
  CHECK(skills::GetMech(11, 1108, "scar_delay", -1.0f) ==
        doctest::Approx(0.25f));
  CHECK(skills::GetMech(11, 1109, "field_damage_per_point_per_tier", -1.0f) ==
        doctest::Approx(0.04f));
  CHECK(skills::GetMech(11, 1111, "echo_damage_per_tier", -1.0f) ==
        doctest::Approx(0.10f));
  CHECK(skills::GetMech(11, 1112, "cadence_bonus_per_point", -1.0f) ==
        doctest::Approx(0.15f));
  CHECK(skills::GetMech(11, 1113, "impact_damage_mult", -1.0f) ==
        doctest::Approx(0.8f));
  CHECK(skills::GetMech(11, 1114, "empower_bonus_per_point", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(11, 1116, "tick_interval_reduction_per_point", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(11, 1117, "linked_cut_cooldown", -1.0f) ==
        doctest::Approx(0.15f));
  CHECK(skills::GetMech(11, 1117, "linked_cut_damage", -1.0f) ==
        doctest::Approx(18.0f));
  CHECK(skills::GetMech(11, 1118, "afflicted_damage_per_point", -1.0f) ==
        doctest::Approx(0.10f));
  CHECK(skills::GetMech(11, 1119, "duration_per_point", -1.0f) ==
        doctest::Approx(0.5f));
  CHECK(skills::GetMech(11, 1121, "tick_interval_mult", -1.0f) ==
        doctest::Approx(1.2f));
  CHECK(skills::GetMech(11, 1121, "field_damage_mult", -1.0f) ==
        doctest::Approx(1.3f));
  CHECK(skills::GetMech(11, 1121, "radius_mult", -1.0f) ==
        doctest::Approx(1.2f));
  CHECK(skills::GetMech(11, 1122, "field_damage_mult", -1.0f) ==
        doctest::Approx(1.12f));
  CHECK(skills::GetMech(11, 1122, "pulse_damage_mult", -1.0f) ==
        doctest::Approx(1.08f));
  CHECK(skills::GetMech(11, 1122, "freeze_chance", -1.0f) ==
        doctest::Approx(0.15f));
  CHECK(skills::GetMech(11, 1122, "freeze_duration", -1.0f) ==
        doctest::Approx(1.0f));
  CHECK(skills::GetMech(11, 1122, "slow_percent", -1.0f) ==
        doctest::Approx(12.0f));
  CHECK(skills::GetMech(11, 1123, "field_damage_mult", -1.0f) ==
        doctest::Approx(1.18f));
  CHECK(skills::GetMech(11, 1123, "pulse_damage_mult", -1.0f) ==
        doctest::Approx(1.10f));
  CHECK(skills::GetMech(11, 1123, "ignite_magnitude_ratio", -1.0f) ==
        doctest::Approx(0.25f));
  CHECK(skills::GetMech(11, 1124, "resist_shred_per_point", -1.0f) ==
        doctest::Approx(2.0f));
  CHECK(skills::GetMech(11, 1124, "resist_shred_cap", -1.0f) ==
        doctest::Approx(12.0f));
  CHECK(skills::GetMech(11, 0, "field_damage_per_tier", -1.0f) ==
        doctest::Approx(0.08f));
  CHECK(skills::GetMech(11, 0, "base_resist_reduction", -1.0f) ==
        doctest::Approx(6.0f));
  CHECK(skills::GetMech(11, 0, "resist_shred_duration", -1.0f) ==
        doctest::Approx(1.25f));
  CHECK(skills::GetMech(11, 0, "field_pulse_base_damage", -1.0f) ==
        doctest::Approx(28.0f));
  CHECK(skills::GetMech(11, 0, "field_pulse_damage_per_tier", -1.0f) ==
        doctest::Approx(6.0f));
  CHECK(skills::GetMech(11, 0, "base_damage_fallback", -1.0f) ==
        doctest::Approx(120.0f));
  // 技能级参数单源自 skills.json（contract）：原 mech 节点 0 同名键应已删除，
  // 缺失时 GetMech 返回调用方兜底值；实际数值改由 skills.json params 提供。
  CHECK(skills::GetMech(11, 0, "impact_radius_default", -1.0f) ==
        doctest::Approx(-1.0f));
  CHECK(skills::GetMech(11, 0, "field_radius_default", -1.0f) ==
        doctest::Approx(-1.0f));
  CHECK(skills::GetMech(11, 0, "field_duration_default", -1.0f) ==
        doctest::Approx(-1.0f));
  CHECK(skills::GetMech(11, 0, "tier_damage_bonus_default", -1.0f) ==
        doctest::Approx(-1.0f));
  CHECK(skills::GetMech(11, 0, "tier_radius_bonus_default", -1.0f) ==
        doctest::Approx(-1.0f));
  const auto *skill11 = SkillRegistry::Get().GetSkill(11);
  REQUIRE(skill11 != nullptr);
  CHECK(skill11->GetParam("impact_radius", -1.0f) == doctest::Approx(90.0f));
  CHECK(skill11->GetParam("field_radius", -1.0f) == doctest::Approx(140.0f));
  CHECK(skill11->GetParam("field_duration", -1.0f) == doctest::Approx(5.0f));
  CHECK(skill11->GetParam("tier_damage_bonus", -1.0f) ==
        doctest::Approx(0.18f));
  CHECK(skill11->GetParam("tier_radius_bonus", -1.0f) ==
        doctest::Approx(14.0f));
}

TEST_CASE("[Functional] Skill 11 - externalized coefficients drive field on cast") {
  TestSetupScope setup;
  EnsureSkillMechanics();
  ProcBudgetManager::Get().ResetForTests();
  CombatEventDispatcher::Init();

  SUBCASE("EnduringHeaven extends field duration") {
    entt::registry registry;
    const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
    ConfigureCaster(registry, owner, {{Nodes::EnduringHeaven, 4}});
    CastHeavenlySword(registry, owner, {0.0f, 0.0f});

    const auto *field = FindField(registry);
    REQUIRE(field != nullptr);
    // 基础 5.0s + 每点 0.5s * 4 = 7.0s（技能11 / 1119 duration_per_point）。
    CHECK(field->header.duration == doctest::Approx(7.0f));
  }

  SUBCASE("SkyEdgeInfusion and OverflowingTiers scale impact damage and radius") {
    entt::registry registry;
    const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
    ConfigureCaster(registry, owner,
                    {{Nodes::SkyEdgeInfusion, 3}, {Nodes::OverflowingTiers, 2}},
                    /*blade_tiers=*/10);
    CastHeavenlySword(registry, owner, {0.0f, 0.0f});

    const auto *field = FindField(registry);
    REQUIRE(field != nullptr);
    // spend_cap = 5 + 2 = 7；impact = 1 + 7*0.18 + 0.04*3*7 = 3.10。
    CHECK(field->impact_damage_mult == doctest::Approx(3.10f));
    // radius = 140 + 7*14 = 238（无 CelestialDomain / SkyPiercingFall）。
    CHECK(field->header.radius == doctest::Approx(238.0f));
    // field_damage = 1 + 0.08*7 = 1.56（无 EdgeOffering）。
    CHECK(field->field_damage_mult == doctest::Approx(1.56f));
  }

  SUBCASE("SkyPiercingFall multiplies radius and impact damage") {
    entt::registry registry;
    const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
    ConfigureCaster(registry, owner, {{Nodes::SkyPiercingFall, 1}});
    CastHeavenlySword(registry, owner, {0.0f, 0.0f});

    const auto *field = FindField(registry);
    REQUIRE(field != nullptr);
    // radius = (140 + 5*14) * 0.7 = 147；impact = (1 + 5*0.18) * 1.35 = 2.565。
    CHECK(field->header.radius == doctest::Approx(147.0f));
    CHECK(field->impact_damage_mult == doctest::Approx(2.565f));
  }

  SUBCASE("EdgeOffering and ElementalRazing use externalized per-point values") {
    entt::registry registry;
    const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
    ConfigureCaster(registry, owner,
                    {{Nodes::EdgeOffering, 2}, {Nodes::ElementalRazing, 10}});
    CastHeavenlySword(registry, owner, {0.0f, 0.0f});

    const auto *field = FindField(registry);
    REQUIRE(field != nullptr);
    // field_damage = 1 + 0.08*5 + 0.04*2*5 = 1.8。
    CHECK(field->field_damage_mult == doctest::Approx(1.8f));
    // extra_resist = min(12, 2*10) = 12（上限亦外置）。
    CHECK(field->extra_resist_reduction == doctest::Approx(12.0f));
  }

  SUBCASE("FieldResonance and ResidualPressure reduce tick interval") {
    entt::registry registry;
    const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
    ConfigureCaster(registry, owner,
                    {{Nodes::ResidualPressure, 2}, {Nodes::FieldResonance, 2}});
    CastHeavenlySword(registry, owner, {0.0f, 0.0f});

    const auto *field = FindField(registry);
    REQUIRE(field != nullptr);
    // 0.5 * (1 - 0.05*2) * (1 - 0.08*2) = 0.5 * 0.9 * 0.84 = 0.378。
    CHECK(field->header.tick_interval == doctest::Approx(0.378f));
  }
}

// T4.1：技能 11 技能级参数兜底常量回归。用「桩技能数据（params 为空）+ 零剑意层数」
// 稳定触发 DoCast 的 GetParam 兜底分支，锚定提升为公开 static constexpr 的兜底值。
TEST_CASE("[Functional] HeavenlySwordDescent - DoCast constexpr fallback constants") {
  TestSetupScope scope;

  // 覆盖 skills.json 中技能 11 的数据并清空 params，使 DoCast 走字段缺失兜底路径。
  SkillData stub{};
  stub.id = kSkillId;
  SkillRegistry::Get().RegisterSkill(stub);

  entt::registry registry;
  const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
  // 零层数：spent_tiers=0，避免 tier_radius_bonus 叠加，直接观测基础半径兜底值。
  ConfigureCaster(registry, owner, {}, /*blade_tiers=*/0);
  CastHeavenlySword(registry, owner, {0.0f, 0.0f});

  const auto *field = FindField(registry);
  REQUIRE(field != nullptr);
  CHECK(field->header.duration ==
        doctest::Approx(skills::HeavenlySwordDescent::kFieldDurationFallback)); // 5.0f
  CHECK(field->header.radius ==
        doctest::Approx(skills::HeavenlySwordDescent::kFieldRadiusFallback)); // 140.0f
  // 组件默认 tick=0.5，无节点修正时经 clamp(0.18f, 0.75f) 不改变结果。
  CHECK(field->header.tick_interval == doctest::Approx(0.5f));
  // resist_reduction 走 GetMech 兜底（非五常量之一），一并锚定。
  CHECK(field->resist_reduction == doctest::Approx(6.0f));
}

} // namespace NoMoreDay
