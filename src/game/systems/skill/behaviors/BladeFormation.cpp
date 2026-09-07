/**
 * @file BladeFormation.cpp
 * @brief 灵剑决 (ID 3) - 模块化环绕守护与灵剑召唤实现
 */
#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <algorithm>

namespace NoMoreDay::skills {

namespace BladeFormationNodes {
constexpr uint32_t SwordPool = 300;
constexpr uint32_t SwiftIntent = 301;
constexpr uint32_t InfiniteSheath = 311;
constexpr uint32_t GiantSword = 330;
constexpr uint32_t QiReflow = 351;
constexpr uint32_t Immortality = 353;
constexpr uint32_t ElementEnchant = 370;
} // namespace BladeFormationNodes

struct BladeFormation : SkillBehaviorBase<BladeFormation> {
  static constexpr uint32_t kSkillId = 3;

  static void DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
    auto &formation = registry.get_or_emplace<BladeFormationComponent>(owner);
    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, kSkillId);
    BakedSkillProfile localProfile;
    if (!profile && registry.all_of<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : registry.get<ActiveSkillsComponent>(owner).specialized_slots) {
        if (spec.skill_id == kSkillId) { SkillSpecializationBaker::Bake(registry, owner, kSkillId, &spec, localProfile, nullptr); profile = &localProfile; break; }
      }
    }

    formation.has_giant_sword = profile ? ((profile->delivery.feature_flags & 2) != 0) : exec.active_nodes.test(BladeFormationNodes::GiantSword % 100);
    formation.mana_on_hit = profile ? ((profile->delivery.feature_flags & 4) != 0) : exec.active_nodes.test(BladeFormationNodes::QiReflow % 100);
    formation.immortality_ready = profile ? ((profile->delivery.feature_flags & 8) != 0) : exec.active_nodes.test(BladeFormationNodes::Immortality % 100);
    formation.damage_penalty = profile ? profile->more_damage_mult : 1.0f;
    formation.max_swords = profile ? (profile->projectile_count > 0 ? profile->projectile_count : 1) : (1 + (exec.active_nodes.test(BladeFormationNodes::SwordPool % 100) ? 1 : 0));
    if (!profile && exec.active_nodes.test(BladeFormationNodes::InfiniteSheath % 100)) { formation.max_swords *= 2; formation.damage_penalty = 0.6f; }
    if (formation.has_giant_sword) formation.max_swords = 1;
    float freqInc = profile ? profile->delivery.sub_interval : (exec.active_nodes.test(BladeFormationNodes::SwiftIntent % 100) ? 0.1f : 0.0f);
    formation.attack_interval = (1.0f / (1.0f + freqInc)) * (formation.has_giant_sword ? 2.0f : 1.0f);
    formation.current_swords = formation.max_swords;

    auto &sentinel = registry.emplace_or_replace<OrbitingSentinelComponent>(owner);
    sentinel.anchor_entity = owner; sentinel.skill_id = kSkillId; sentinel.count = formation.max_swords;
    sentinel.angular_velocity = 180.0f; sentinel.orbit_radius = 60.0f; sentinel.attack_interval = formation.attack_interval;

    const Tag attunement = systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);
    std::vector<entt::entity> existing;
    auto view = registry.view<SpiritSwordTag, SummonComponent>();
    for (auto e : view) { if (view.get<SummonComponent>(e).owner == owner) existing.push_back(e); }
    for (auto e : existing) {
      registry.get_or_emplace<SummonCombatProfile>(e).damage_scale = (formation.has_giant_sword ? 1.5f : 0.5f) * formation.damage_penalty;
      registry.get_or_emplace<SpiritSwordAI>(e).attack_interval = formation.attack_interval;
      if (attunement != Tag::None) {
        auto &m = registry.emplace_or_replace<SkillModifierComponent>(e); m.damage_modifiers.clear();
        m.damage_modifiers.push_back({Tag::Physical, attunement, 0.5f, ModifierType::Convert});
      }
    }
    const int cur = (int)existing.size();
    if (cur > formation.max_swords) {
      for (int i = formation.max_swords; i < cur; ++i) registry.destroy(existing[i]);
    } else if (cur < formation.max_swords) {
      auto *pos = registry.try_get<Position>(owner);
      for (int i = cur; i < formation.max_swords; ++i) {
        auto sword = registry.create();
        registry.emplace<LocalLevelTag>(sword); registry.emplace<SpiritSwordTag>(sword);
        registry.emplace<Position>(sword, pos ? pos->x : 0.0f, pos ? pos->y : 0.0f); registry.emplace<Velocity>(sword, 0.0f, 0.0f);
        auto &sm = registry.emplace<SummonComponent>(sword); sm.owner = owner; sm.skill_id = kSkillId; sm.archetype_id = SummonArchetype::SpiritSword; sm.lifetime = sm.max_lifetime = -1.0f;
        auto &ai = registry.emplace<SpiritSwordAI>(sword); ai.orbit_angle = (360.0f / formation.max_swords) * i; ai.attack_interval = formation.attack_interval;
        auto &cb = registry.emplace<SummonCombatProfile>(sword); cb.damage_scale = (formation.has_giant_sword ? 1.5f : 0.5f) * formation.damage_penalty;
        if (attunement != Tag::None) { auto &m = registry.emplace_or_replace<SkillModifierComponent>(sword); m.damage_modifiers.push_back({Tag::Physical, attunement, 0.5f, ModifierType::Convert}); }
      }
    }
  }

  static void DoHit(entt::registry &reg, entt::entity attacker, entt::entity, Tag, bool) {
    auto *formation = reg.try_get<BladeFormationComponent>(attacker);
    const auto *p = SkillSystem::GetBakedSkillProfile(reg, attacker, kSkillId);
    bool mana_on_hit = formation ? formation->mana_on_hit : (p ? ((p->delivery.feature_flags & 4) != 0) : false);
    if (mana_on_hit) {
      if (auto *stats = reg.try_get<CombatStats>(attacker)) {
        stats->mana = std::min(stats->max_mana, stats->mana + 2.0f);
        reg.get_or_emplace<StatsDirty>(attacker);
      }
    }
  }
};

REGISTER_SKILL_BEHAVIOR(BladeFormation)
void RegisterBladeFormation() {}
} // namespace NoMoreDay::skills
