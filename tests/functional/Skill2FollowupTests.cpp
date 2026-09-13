// 剑意乾坤 (技能2) 后续实现行为测试：251 剑意爆发满层联动的正向用例——
// 满 10 层剑意消耗时额外重置技能1 [流云刺] 的冷却，未满层只消耗不重置；
// 并断言阈值/每层范围/每层暴击系数均来自 skill_mechanics.json 而非代码字面量。
#include "TestCommon.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"

#include <unordered_map>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 2;             // 裂空斩
constexpr uint32_t kFlowingThrustSkillId = 1; // 流云刺
constexpr uint32_t kIntentBurstNode = 251;   // 剑意爆发

void LoadSkillData() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
  auto &ailments = systems::AilmentRegistry::Get();
  ailments.ResetForTests();
  REQUIRE(ailments.EnsureLoaded());
}

// 构造携带剑意与流云刺冷却槽位的施法者；251 未烘焙 profile 时行为层回退
// 读取 specialized_slots 的点数（与既有 RendingWaveNodes 用例一致）。
entt::entity MakePlayer(entt::registry &registry,
                        const std::unordered_map<uint32_t, int> &nodes,
                        int intentStacks, float flowingThrustCooldown) {
  auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(player, 100.0f, 100.0f);

  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 100.0f;
  stats.health = 100.0f;
  stats.max_mana = 100.0f;
  stats.mana = 100.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;
  stats.effective_dexterity = 50.0f;
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.slots[0].id = kSkillId;
  active.slots[1].id = kFlowingThrustSkillId;
  active.slots[1].cooldown = flowingThrustCooldown;
  active.specialized_slots[0].skill_id = kSkillId;
  active.specialized_slots[0].allocated_points = nodes;

  auto &intent = registry.emplace<SwordIntentComponent>(player);
  intent.stacks = intentStacks;
  intent.max_stacks = SkillConstants::DEFAULT_MAX_SWORD_INTENT;
  return player;
}

float GetSlotCooldown(entt::registry &registry, entt::entity player,
                      uint32_t skillId) {
  const auto &active = registry.get<ActiveSkillsComponent>(player);
  for (const auto &slot : active.slots) {
    if (slot.id == skillId) {
      return slot.cooldown;
    }
  }
  return -1.0f;
}

void CastRendingWave(entt::registry &registry, entt::entity player,
                     Vector2 target_pos) {
  SkillExecution exec;
  exec.skill_id = kSkillId;
  exec.owner = player;
  exec.target_pos = target_pos;
  exec.cast_id = 1;

  auto castFunc = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(castFunc != nullptr);
  castFunc(registry, player, exec);
}

} // namespace

// B3.4：满 10 层剑意消耗时额外重置流云刺 CD（GDD L210），未满层不重置。
TEST_CASE("[Functional] Skill 2 - Full 10 Sword Intent Resets FlowThrust Cooldown (B3.4)") {
  TestSetupScope scope;
  LoadSkillData();

  const auto &mech = data::SkillMechanicsRegistry::Get();
  // 阈值必须来自机制表 251 (skill_mechanics.json:125-129)，非代码硬编码；
  // 用哨兵默认值 -1 断言键确实被读取到。
  const float thresholdRaw =
      mech.GetFloat(kSkillId, kIntentBurstNode, "intent_threshold", -1.0f);
  REQUIRE(thresholdRaw > 0.0f);
  CHECK(thresholdRaw == doctest::Approx(5.0f));
  const int threshold = static_cast<int>(thresholdRaw);
  const int fullIntent = SkillConstants::DEFAULT_MAX_SWORD_INTENT;
  REQUIRE(fullIntent == 10);

  // 场景一：满 10 层 -> 消耗全部剑意并额外重置流云刺冷却
  {
    entt::registry registry;
    auto player = MakePlayer(registry, {{kIntentBurstNode, 1}}, fullIntent, 7.5f);
    CastRendingWave(registry, player, {200.0f, 0.0f});

    CHECK(registry.get<SwordIntentComponent>(player).stacks == 0);
    CHECK(GetSlotCooldown(registry, player, kFlowingThrustSkillId) ==
          doctest::Approx(0.0f));
  }

  // 场景二：9 层（达到阈值但未满层）-> 消耗剑意但不重置冷却
  {
    entt::registry registry;
    auto player =
        MakePlayer(registry, {{kIntentBurstNode, 1}}, fullIntent - 1, 7.5f);
    CastRendingWave(registry, player, {200.0f, 0.0f});

    CHECK(registry.get<SwordIntentComponent>(player).stacks == 0);
    CHECK(GetSlotCooldown(registry, player, kFlowingThrustSkillId) ==
          doctest::Approx(7.5f));
  }

  // 场景三：低于阈值 -> 既不消耗也不重置
  {
    entt::registry registry;
    auto player =
        MakePlayer(registry, {{kIntentBurstNode, 1}}, threshold - 1, 7.5f);
    CastRendingWave(registry, player, {200.0f, 0.0f});

    CHECK(registry.get<SwordIntentComponent>(player).stacks == threshold - 1);
    CHECK(GetSlotCooldown(registry, player, kFlowingThrustSkillId) ==
          doctest::Approx(7.5f));
  }
}

// B2.4：251 每层范围/暴击系数消费机制表键 area_pct_per_intent /
// crit_pct_per_intent，断言弹体数值 == 机制表值推导结果。
TEST_CASE("[Functional] Skill 2 - Node 251 Per-Intent Area & Crit From Mechanics (B2.4)") {
  TestSetupScope scope;
  LoadSkillData();

  const auto &mech = data::SkillMechanicsRegistry::Get();
  const float areaPct =
      mech.GetFloat(kSkillId, kIntentBurstNode, "area_pct_per_intent", -1.0f);
  const float critPct =
      mech.GetFloat(kSkillId, kIntentBurstNode, "crit_pct_per_intent", -1.0f);
  REQUIRE(areaPct > 0.0f);
  REQUIRE(critPct > 0.0f);

  constexpr int kStacks = 6;
  entt::registry registry;
  // 未烘焙 profile 时基准半径 35（RendingWave.cpp 回退值），
  // 弹体半径按机制表逐层放大。
  auto player = MakePlayer(registry, {{kIntentBurstNode, 1}}, kStacks, 0.0f);
  CastRendingWave(registry, player, {200.0f, 0.0f});

  auto projView = registry.view<Projectile>();
  REQUIRE(projView.begin() != projView.end());
  const auto &proj = projView.get<Projectile>(*projView.begin());

  const float expectedRadius =
      35.0f * (1.0f + areaPct / 100.0f * static_cast<float>(kStacks));
  CHECK(proj.radius == doctest::Approx(expectedRadius));
  // 基础暴击为 0，逐层叠加 crit_pct_per_intent。
  CHECK(proj.payload_context->crit_chance ==
        doctest::Approx(critPct / 100.0f * static_cast<float>(kStacks)));
}

} // namespace NoMoreDay
