#include "TestCommon.hpp"
#include "doctest.h"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"

namespace NoMoreDay {
namespace {

// 构造仅含基础战斗属性的攻击者：暴击固定为 0，保证结算数值确定。
entt::entity MakeAttacker(entt::registry &registry) {
  const entt::entity attacker = registry.create();
  auto &stats = registry.emplace<CombatStats>(attacker);
  stats.crit_chance = 0.0f;
  stats.min_weapon_damage = 0.0f;
  stats.max_weapon_damage = 0.0f;
  stats.cached_area_level = 1;
  return attacker;
}

} // namespace

// S4：次级打击（SecondaryProc）带 payload 基础伤害时不得叠加技能固有伤害与武器乘区。
TEST_CASE("[Functional] DamagePipeline - SecondaryProc payload skips skill base and weapon mult") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  const SkillData *skill = SkillRegistry::Get().GetSkill(5);
  REQUIRE(skill != nullptr);
  REQUIRE(skill->base_damage > 0.0f);
  REQUIRE(skill->weapon_damage_mult > 0.0f);

  entt::registry registry;
  const entt::entity attacker = MakeAttacker(registry);

  auto make_request = [&](DamageOrigin origin) {
    DamageRequest request;
    request.origin = origin;
    request.attacker = attacker;
    request.defender = entt::null;
    request.skill_id = 5;
    request.additional_tags = Tag::Hit;
    request.skip_mitigation = true;
    request.dispatch_damage_events = false;
    request.is_simulation = false;
    request.payload_context = DamagePayloadContext{};
    request.payload_context->base_damage_min = 100.0f;
    request.payload_context->base_damage_max = 100.0f;
    request.payload_context->effective_tags = Tag::Physical;
    return request;
  };

  const DamageResult direct = DamagePipeline::Calculate(
      registry, make_request(DamageOrigin::DirectSkillCast));
  const DamageResult secondary = DamagePipeline::Calculate(
      registry, make_request(DamageOrigin::SecondaryProc));

  // 直接施法：skill.base_damage + payload × weapon_damage_mult（既有语义）。
  CHECK(direct.total_damage ==
        doctest::Approx(skill->base_damage +
                        100.0f * skill->weapon_damage_mult));
  // 次级打击：仅调用方 payload 点伤，不加 skill.base、不乘 wmult（设计 D6/S4）。
  CHECK(secondary.total_damage == doctest::Approx(100.0f));
  CHECK(secondary.total_damage < direct.total_damage);
}

} // namespace NoMoreDay
