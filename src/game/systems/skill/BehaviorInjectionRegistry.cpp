#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include <spdlog/spdlog.h>

namespace NoMoreDay {

std::array<BehaviorInjectionRegistry::BehaviorInjector,
           static_cast<std::size_t>(SkillBehaviorId::Count)>
    BehaviorInjectionRegistry::injectors;

void BehaviorInjectionRegistry::Register(Id id, BehaviorInjector injector) {
  if (id == SkillBehaviorId::None) {
    LOG_WARN("BehaviorInjectionRegistry: refusing to register None behavior");
    return;
  }
  const std::size_t idx = static_cast<std::size_t>(id);
  if (idx >= injectors.size()) {
    LOG_WARN("BehaviorInjectionRegistry: behavior id {} out of range", idx);
    return;
  }
  if (injectors[idx]) {
    LOG_WARN("BehaviorInjectionRegistry: overwriting behavior for id {}", idx);
  }
  injectors[idx] = std::move(injector);
}

void BehaviorInjectionRegistry::Apply(Id id, entt::registry &r,
                                      entt::entity e) {
  const std::size_t idx = static_cast<std::size_t>(id);
  if (idx >= injectors.size()) {
    return;
  }
  const auto &injector = injectors[idx];
  if (injector) {
    injector(r, e);
  }
}

void BehaviorInjectionRegistry::Clear() {
  injectors.fill(nullptr);
}

void BehaviorInjectionRegistry::Init() {
  if (injectors[static_cast<std::size_t>(SkillBehaviorId::ShadowCaster)]) {
    return; // Already initialized
  }

  Register(SkillBehaviorId::ShadowCaster,
           [](entt::registry &r, entt::entity e) {
             auto &trig = r.get_or_emplace<TriggerRuleComponent>(e);
             if (!trig.HasRule(124)) {
               TriggerRule rule;
               rule.rule_id = 124;
               rule.listen_event = CombatEventType::Count; // 哨值：由 ShadowDuplicationHook 前置钩子直读消费，不走 ProcEngine 通用派发
               rule.target_mode = TriggerTargetPolicy::GroundTarget;
               rule.cast_skill_id = 124;
               rule.internal_cooldown = 3.0f;
               rule.base_chance = 1.0f;
               rule.use_proc_scaling = false;
               rule.effectiveness = 0.5f;
               trig.AddRule(rule);
             }
             r.get_or_emplace<ShadowKillArrayReady>(e);
           });

  std::size_t count = 0;
  for (const auto &injector : injectors) {
    if (injector) {
      ++count;
    }
  }
  LOG_INFO("BehaviorInjectionRegistry: Initialized with {} behaviors", count);
}

} // namespace NoMoreDay
