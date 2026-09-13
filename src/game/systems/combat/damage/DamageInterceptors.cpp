#include "game/systems/combat/damage/DamageInterceptors.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "game/foundation/components/AdvancedAffixComponents.hpp" // InvulnerableComponent
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp" // BladeWardComponent
#include "spdlog/spdlog.h"
#include <algorithm>

namespace NoMoreDay {
namespace damage {

InvulnerabilityIntercept EvaluateInvulnerability(entt::registry &registry,
                                                 entt::entity defender,
                                                 bool is_simulation) {
  InvulnerabilityIntercept intercept;
  // 模拟路径不做业务拦截，保证模拟与实战数值分离。
  if (is_simulation || !registry.valid(defender)) {
    return intercept;
  }
  if (registry.all_of<InvulnerableComponent>(defender)) {
    intercept.blocked = true;
  }
  return intercept;
}

BladeWardIntercept EvaluateBladeWardInterception(entt::registry &registry,
                                                 entt::entity defender,
                                                 entt::entity source_entity,
                                                 Tag combined_tags,
                                                 bool is_simulation) {
  BladeWardIntercept intercept;
  if (is_simulation || !registry.valid(defender)) {
    return intercept;
  }
  // 投射物实体（含物理投射物）的 Blade Ward 判定由 ProjectileSystem 负责
  // (见 ProjectileSystem.cpp:509-527)，那里会自行消耗剑count/偏转；此处仅处理
  // “带 Projectile 标签但源实体不是投射物”的伤害，避免对投射物二次拦截。
  const bool is_projectile_source =
      registry.valid(source_entity) && registry.all_of<Projectile>(source_entity);
  if (!HasTag(combined_tags, Tag::Projectile) || is_projectile_source) {
    return intercept;
  }
  auto *ward = registry.try_get<BladeWardComponent>(defender);
  if (ward == nullptr) {
    return intercept;
  }
  // 434 无瑕之御：充能就绪时完全招架本次命中并消耗充能，不消耗飞剑。
  if (ward->perfect_parry_ready) {
    ward->perfect_parry_ready = false;
    ward->perfect_parry_timer = 0.0f;
    intercept.intercepted = true;
    LOG_INFO("Blade Ward: perfect parry negated hit for entity {}!",
             static_cast<uint32_t>(defender));
    return intercept;
  }
  if (ward->sword_count <= 0) {
    return intercept;
  }
  const float chance =
      std::clamp(static_cast<float>(ward->sword_count) * ward->interception_chance,
                 0.0f, 1.0f);
  if (utils::ThreadSafeRandom::GetFloat01() >= chance) {
    return intercept;
  }
  intercept.intercepted = true;
  if (!ward->is_solidified) {
    ward->sword_count--;
  }
  LOG_INFO("Blade Ward: intercepted non-projectile-source hit for entity {}! "
           "Swords remaining: {}",
           static_cast<uint32_t>(defender), ward->sword_count);
  return intercept;
}

} // namespace damage
} // namespace NoMoreDay
