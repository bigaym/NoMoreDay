#pragma once

#include "game/foundation/components/Buff.hpp"

#include <entt/entt.hpp>

namespace NoMoreDay {

// 眩晕没有 ailment_contracts 条目，统一直接施加 BuffType::Stun，
// 避免 AilmentApplier 因缺少契约而静默失败（技能2 顶点眩晕、技能8 巨阙）。
inline bool ApplyStun(entt::registry &registry, entt::entity target,
                      entt::entity source, float duration,
                      const char *effect_id, uint32_t source_skill_id) {
  if (!registry.valid(target) || duration <= 0.0f) {
    return false;
  }
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(target);
  BuffEffect stun{};
  stun.id = effect_id;
  stun.name = "Stun";
  stun.type = BuffType::Stun;
  stun.duration = duration;
  stun.remaining = duration;
  stun.stacks = 1;
  stun.max_stacks = 1;
  stun.is_debuff = true;
  stun.source = source;
  stun.source_skill_id = static_cast<int>(source_skill_id);
  effects.AddOrRefresh(stun);
  return true;
}

} // namespace NoMoreDay
