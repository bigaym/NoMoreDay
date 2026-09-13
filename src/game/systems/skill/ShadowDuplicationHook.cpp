#include "game/systems/skill/ShadowDuplicationHook.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>

namespace NoMoreDay {

PreCastShadowDuplicationResult CheckPreCastShadowDuplication(
    entt::registry &registry, entt::entity entity, const SkillData *data,
    float base_cost, const CombatStats *stats) {
  auto *trig = registry.try_get<TriggerRuleComponent>(entity);
  const bool hasRule124 = trig && trig->HasRule(124);
  if ((!hasRule124 && !registry.any_of<ShadowKillArrayReady>(entity)) || !data) {
    return {};
  }
  const bool excluded =
      HasTag(data->tags, Tag::Movement) || HasTag(data->tags, Tag::Buff) ||
      HasTag(data->tags, Tag::Aura) || HasTag(data->tags, Tag::Channeled);
  if (excluded) {
    return {};
  }
  auto *pStats = registry.try_get<PlayerStats>(entity);
  float currentTime = static_cast<float>(GetTime());
  if (pStats && (currentTime - pStats->last_shadow_trigger_time >= 3.0f)) {
    float extra_cost = base_cost * 0.5f;
    if (stats && stats->mana >= (base_cost + extra_cost)) {
      return {.duplicate = true, .extra_cost = extra_cost};
    }
  }
  return {};
}

void ExecutePreCastShadowDuplication(entt::registry &registry, entt::entity entity,
                                    uint32_t skill_id, Vector2 target_pos,
                                    const CombatStats *stats) {
  auto *pStats = registry.try_get<PlayerStats>(entity);
  if (pStats) {
    pStats->last_shadow_trigger_time = static_cast<float>(GetTime());
  }
  registry.remove<ShadowKillArrayReady>(entity);
  if (auto *trig = registry.try_get<TriggerRuleComponent>(entity)) {
    trig->RemoveRule(124);
  }

  auto *pos = registry.try_get<Position>(entity);
  Vector2 spawnPos = pos ? Vector2{pos->x, pos->y} : Vector2{0, 0};

  auto shadow_ent = registry.create();
  registry.emplace<LocalLevelTag>(shadow_ent);
  registry.emplace<Position>(shadow_ent, spawnPos.x, spawnPos.y);
  registry.emplace<AnimationStateComponent>(shadow_ent);
  registry.emplace<ColorComponent>(shadow_ent, ColorAlpha(PURPLE, 0.4f));
  registry.emplace<ShadowCloneComponent>(shadow_ent);

  auto &sc = registry.emplace<ShadowComponent>(shadow_ent);
  sc.damage_scale = 0.5f; // Explicit 50% for Shadow Kill Array
  sc.delay = 0.1f;
  sc.lifetime = 1.0f;
  sc.snapshot.skill_id = skill_id;
  sc.snapshot.position = spawnPos;
  sc.snapshot.target_pos = target_pos;
  if (stats) {
    sc.snapshot.stats = *stats;
  }

  registry.emplace<ShadowVisualComponent>(shadow_ent).color_tint = {
      60, 0, 80, 200}; // Distinct visual for clone
  LOG_INFO("Pre-cast hook: Duplicating skill {} for entity {}", skill_id,
           static_cast<uint32_t>(entity));
}

} // namespace NoMoreDay
