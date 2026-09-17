#include "TestCommon.hpp"
#include "SkillKeyNodeMatrixTestHelpers.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/behaviors/BloodSea.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace NoMoreDay {

// ===== 设计 §6 R-05：血海 More 乘算与 1202 平加项的顺序偏差量化快照 =====
//
// 迁移前 BloodSea.cpp 的增伤顺序（以 HEAD 复核）：
//   :484-485 1201 `*= (1 + 0.06·pts)` → :486-488 1202 `+= K` → :545 1207 `*= 1.1`
// 迁移后 1201 与 1207 的 More 均单源收敛进烘焙档案 more_damage_mult，统一在 1202 平加
// 之前一次性乘算，故：
//     旧值 = (baseline·more1201 + K) · more1207
//     新值 = baseline · (more1201·more1207) + K
//     偏差 = 旧值 − 新值 = K · (more1207 − 1) = 0.1 · K
// 关键：1201 在迁移前本就在 1202 之前，顺序变化仅来自 1207 的 `*= 1.1` 前移，
// 因此偏差系数是 1207 的 0.10，而不是 More 合成积的 (more − 1) = 0.364。
// 其中 baseline = 1 + effective_consumed · bloodthirst_damage_bonus（裸血欲倍率），
// K = effective_consumed · bloodthirstEdgePoints · damage_per_point_per_bloodthirst（1202 平加项）。
// 该偏差是设计 R-05 明确接受的迁移代价（换取单一事实源），本用例以精确断言锁定：
//   - “1202 = 0”：K = 0，旧新一致，final == baseline · more；
//   - “1202 > 0”：final == baseline · more + K，偏差 == 0.1 · K。
// 若把 More 乘算移到 1202 平加之后，“1202 > 0”的 final 会变成 (baseline + K)·more，
// 精确断言立即失败，从而锁定顺序。

namespace {

constexpr uint32_t kBloodSeaSkillIdLocal = 12;

// 1201 迁移前的单点乘区：1 + 0.06 * 4 = 1.24。
constexpr float kMore1201Expected = 1.24f;
// 1207 的乘区：1.10。
constexpr float kMore1207Expected = 1.10f;
// 1201 点满（4 点）与 1207 点亮（1 点）的 UMR More 合成积：1.24 * 1.10 = 1.364。
constexpr float kMoreDamageExpected = 1.364f;
// effective_consumed = 4 时：baseline = 1 + 4 * 0.12 = 1.48。
constexpr float kBaselineExpected = 1.48f;
// K = 4 * 4 * 0.025 = 0.4。
constexpr float kSlopeExpected = 0.4f;
// “1202 = 0”：2.01872；“1202 > 0”：2.41872。
constexpr float kExpectedZeroBonus = 2.01872f;
constexpr float kExpectedWithBonus = 2.41872f;
// 迁移前旧顺序 (baseline·1.24 + K) · 1.10 = 2.45872。
constexpr float kExpectedOldOrderWithBonus = 2.45872f;
// 顺序偏差：0.1 * K = 0.04。
constexpr float kExpectedDeviation = 0.04f;

} // namespace

TEST_CASE("[Unit] SkillBatch4BloodSea - R-05 More-before-1202 ordering snapshot") {
  TestSetupScope scope;
  REQUIRE(ReloadModifierRuntimeFromAsset());
  SkillBehaviorRegistry::Initialize();

  // 施放辅助：带非空烘焙档案（同 ID 专精槽 -> ResolveBakedProfile 回退烘焙），
  // 血渴 current = 4（effective_consumed = 4），DemonBlade + 血誓激活。
  const auto castField = [](const std::vector<std::pair<uint32_t, int>> &nodes) {
    entt::registry registry;
    const entt::entity player =
        test::skill_keynode_matrix::CreateCaster(registry, 400.0f);

    auto &mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.selected = BladeMasteryId::DemonBlade;
    mastery.blood_oath_active = true;

    auto &resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::Bloodthirst;
    resource.current = 4;
    resource.max = 10;

    test::skill_keynode_matrix::ConfigureSpecialization(registry, player,
                                                        kBloodSeaSkillIdLocal, nodes);

    SkillExecution exec;
    exec.skill_id = kBloodSeaSkillIdLocal;
    exec.owner = player;
    exec.cast_id = 12060u + static_cast<uint64_t>(nodes.size());
    exec.target_pos = {18.0f, 0.0f};

    auto cast = SkillBehaviorRegistry::GetCast(kBloodSeaSkillIdLocal);
    REQUIRE(cast != nullptr);
    cast(registry, player, exec);

    auto view = registry.view<BloodSeaFieldComponent>();
    REQUIRE(view.begin() != view.end());
    return view.get<BloodSeaFieldComponent>(*view.begin());
  };

  // “1202 = 0”：仅有 1201/1207 提供 More 乘区。
  const auto field1202Zero = castField({{1201, 4}, {1207, 1}});
  // “1202 > 0”：额外点出 1202（血渴锋缘），触发 More 乘算提前的顺序偏差。
  const auto field1202Positive = castField({{1201, 4}, {1207, 1}, {1202, 4}});

  // 期望 More 乘区：用同一组专精点直接烘焙参考档案，取 more_damage_mult（Baker 唯一事实源）。
  entt::registry refRegistry;
  const entt::entity refOwner =
      test::skill_keynode_matrix::CreateCaster(refRegistry, 400.0f);
  SpecializedSkill refSpec;
  refSpec.skill_id = kBloodSeaSkillIdLocal;
  refSpec.allocated_points[1201] = 4;
  refSpec.allocated_points[1207] = 1;
  BakedSkillProfile refProfile{};
  SkillSpecializationBaker::Bake(refRegistry, refOwner, kBloodSeaSkillIdLocal, &refSpec,
                                 refProfile, nullptr);
  const float more = refProfile.more_damage_mult;

  // effective_consumed = max(1, consumed)：本用例血渴 current = 4，故为 4；从场组件读取以自洽。
  const float effective =
      static_cast<float>(std::max(1, field1202Positive.consumed_bloodthirst));
  const auto *skill = SkillRegistry::Get().GetSkill(kBloodSeaSkillIdLocal);
  REQUIRE(skill != nullptr);
  const float bloodthirst_damage_bonus =
      skill->GetParam("bloodthirst_damage_bonus",
                      skills::BloodSea::kBloodthirstDamageBonusDefault);
  // baseline 为本用例自我一致的裸血欲倍率（未乘 More、未加 1202）。
  const float baseline = 1.0f + effective * bloodthirst_damage_bonus;
  // 1202 平加项 K：与实现同源读取机制表 damage_per_point_per_bloodthirst。
  const float slope = skills::GetMech(
      kBloodSeaSkillIdLocal, skills::BloodSeaNodes::BloodthirstEdge,
      "damage_per_point_per_bloodthirst", 0.025f);
  const float k1202 = effective * 4.0f * slope;

  CHECK(effective == doctest::Approx(4.0f));
  CHECK(baseline == doctest::Approx(kBaselineExpected));
  CHECK(more == doctest::Approx(kMoreDamageExpected));
  CHECK(more == doctest::Approx(1.24f * 1.10f));
  CHECK(k1202 == doctest::Approx(kSlopeExpected));

  // “1202 = 0”：final = baseline * more（无平加项，顺序无关，精确锁定 1201/1207 单源输出）。
  CHECK(field1202Zero.consumed_bloodthirst == 4);
  CHECK(field1202Zero.bonus_damage_mult == doctest::Approx(kExpectedZeroBonus));
  CHECK(field1202Zero.bonus_damage_mult == doctest::Approx(baseline * more));

  // “1202 > 0”：final = baseline * more + K（More 乘算在 1202 平加之前）。
  CHECK(field1202Positive.consumed_bloodthirst == 4);
  CHECK(field1202Positive.bonus_damage_mult == doctest::Approx(kExpectedWithBonus));
  CHECK(field1202Positive.bonus_damage_mult == doctest::Approx(baseline * more + k1202));

  // 顺序偏差量：旧顺序 (baseline·more1201 + K)·more1207 与新顺序之差 = K·(more1207 − 1)。
  const float oldOrderBonus =
      (baseline * kMore1201Expected + k1202) * kMore1207Expected;
  const float newOrderBonus = field1202Positive.bonus_damage_mult;
  CHECK(oldOrderBonus == doctest::Approx(kExpectedOldOrderWithBonus));
  CHECK(newOrderBonus == doctest::Approx(kExpectedWithBonus));
  CHECK(oldOrderBonus - newOrderBonus ==
        doctest::Approx(k1202 * (kMore1207Expected - 1.0f)));
  CHECK(oldOrderBonus - newOrderBonus == doctest::Approx(kExpectedDeviation));
}

} // namespace NoMoreDay
