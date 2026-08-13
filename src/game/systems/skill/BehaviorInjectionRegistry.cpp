#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
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
    LOG_WARN("BehaviorInjectionRegistry: Overwriting injector for ID '{}'",
             SkillBehaviorIdToString(id));
  }
  injectors[idx] = std::move(injector);
}

void BehaviorInjectionRegistry::Apply(Id id, entt::registry &registry,
                                      entt::entity entity) {
  if (id == SkillBehaviorId::None) {
    return;
  }

  const std::size_t idx = static_cast<std::size_t>(id);
  if (idx < injectors.size() && injectors[idx]) {
    injectors[idx](registry, entity);
    LOG_DEBUG(
        "BehaviorInjectionRegistry: Applied behavior '{}' to entity {}",
        SkillBehaviorIdToString(id), static_cast<uint32_t>(entity));
  } else {
    LOG_WARN("BehaviorInjectionRegistry: Unknown behavior ID '{}'",
             SkillBehaviorIdToString(id));
  }
}

void BehaviorInjectionRegistry::Init() {
  if (injectors[static_cast<std::size_t>(SkillBehaviorId::ShadowCaster)]) {
    return; // Already initialized
  }

  Register(SkillBehaviorId::ShadowCaster,
           [](entt::registry &r, entt::entity e) {
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
