#include "doctest.h"

#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"

#include "game/foundation/components/AdvancedAffixComponents.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/components/PersistentFieldComponents.hpp"
#include "game/systems/skill/behaviors/BloodSea.hpp"
#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <cstdint>

namespace NoMoreDay {
namespace {

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
}

// 用空 params 的桩技能数据覆盖 skills.json，强制 DoCast 走技能级参数兜底分支。
void StubSkillData(const uint32_t skillId) {
  SkillData stub{};
  stub.id = skillId;
  SkillRegistry::Get().RegisterSkill(stub);
}

void CastSkill(entt::registry &registry, entt::entity owner,
               const uint32_t skillId, const Vector2 targetPos) {
  SkillExecution exec{};
  exec.skill_id = skillId;
  exec.owner = owner;
  exec.target_pos = targetPos;

  auto cast = SkillBehaviorRegistry::GetCast(skillId);
  REQUIRE(cast != nullptr);
  cast(registry, owner, exec);
}

} // namespace

// ===== R-01 空档案守护：未专精施法者仍可安全施放技能 10/11/12 =====
//
// 施法者经 CreateCaster 装配（无 ActiveSkillsComponent）时，既没有缓存烘焙档案，
// 也没有同 ID 专精槽，ResolveBakedProfile 返回 nullptr。三条 DoCast 路径必须
// 按技能级 params 兜底且不得崩溃——这是移除行为层手写数值分支后，
// 「未专精角色仍可正常施法」的回归证据。

TEST_CASE("[Unit] SkillBatch4NullProfile - SevenStarSlash baseline duration") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  StubSkillData(skills::seven_star_shared::kSevenStarSlashSkillId);

  entt::registry registry;
  const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
  CastSkill(registry, owner, skills::seven_star_shared::kSevenStarSlashSkillId,
            {100.0f, 100.0f});

  // 空档案下无敌时长回落技能级兜底 invulnerable_duration = 0.5s（无节点叠加）。
  const auto *invulnerable = registry.try_get<InvulnerableComponent>(owner);
  REQUIRE(invulnerable != nullptr);
  CHECK(invulnerable->duration == doctest::Approx(0.5f));
}

TEST_CASE("[Unit] SkillBatch4NullProfile - HeavenlySwordDescent baseline "
          "radius/duration") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  StubSkillData(skills::kHeavenlySwordSkillId);

  entt::registry registry;
  const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
  CastSkill(registry, owner, skills::kHeavenlySwordSkillId, {0.0f, 0.0f});

  const auto view = registry.view<HeavenlySwordFieldComponent>();
  REQUIRE(view.begin() != view.end());
  const auto &field = view.get<HeavenlySwordFieldComponent>(*view.begin());

  // 空档案下领域时长回落技能级兜底 field_duration = 5.0s（字面量锚定，避免与被测常量同源自证）。
  CHECK(field.header.duration == doctest::Approx(5.0f));
  // 空档案下领域半径回落 field_radius = 140.0（字面量锚定；无专精层数、无范围乘算）。
  CHECK(field.header.radius == doctest::Approx(140.0f));
}

TEST_CASE("[Unit] SkillBatch4NullProfile - BloodSea baseline radius/duration/more") {
  TestSetupScope scope;
  EnsureSkillMechanics();
  StubSkillData(skills::kBloodSeaSkillId);

  entt::registry registry;
  const entt::entity owner = test::skill_keynode_matrix::CreateCaster(registry);
  CastSkill(registry, owner, skills::kBloodSeaSkillId, {0.0f, 0.0f});

  const auto view = registry.view<BloodSeaFieldComponent>();
  REQUIRE(view.begin() != view.end());
  const auto &field = view.get<BloodSeaFieldComponent>(*view.begin());

  // 基准常量固化：空档案半径/时长/More 的唯一事实源（R-01 兜底来源）。
  CHECK(skills::BloodSea::kFieldDurationDefault == doctest::Approx(4.8f));
  CHECK(skills::BloodSea::kFieldRadiusDefault == doctest::Approx(120.0f));
  // effective_consumed = max(1, 0) = 1，兜底基准上必然叠加一层血欲系数
  // （field_duration_per_bloodthirst 0.2s / field_radius_per_bloodthirst 6.0），
  // 故裸 4.8/120 无法在 DoCast 出口直接观测；此处以常量固化 + 行为断言双重锚定，
  // 与既有 [Functional] BloodSea - DoCast constexpr fallback constants 同口径。
  CHECK(field.header.duration == doctest::Approx(4.8f + 0.2f));
  CHECK(field.header.radius == doctest::Approx(120.0f + 6.0f));
  // More 乘区空档案回落单位元 1.0，故增伤仍为单层血欲倍率 1 + 1 * 0.12 = 1.12。
  CHECK(field.bonus_damage_mult == doctest::Approx(1.0f + 0.12f));
}

} // namespace NoMoreDay
