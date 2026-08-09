#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include <spdlog/spdlog.h>

#include "game/foundation/components/SkillDefs.hpp"

namespace NoMoreDay {

namespace BehaviorID {
constexpr const char *ShadowCaster = "shadow_caster";
}

std::unordered_map<std::string, BehaviorInjectionRegistry::BehaviorInjector>
    BehaviorInjectionRegistry::injectors;

void BehaviorInjectionRegistry::Register(const std::string &id,
                                         BehaviorInjector injector) {
  if (injectors.contains(id)) {
    LOG_WARN("BehaviorInjectionRegistry: Overwriting injector for ID '{}'",
                 id);
  }
  injectors[id] = std::move(injector);
}

void BehaviorInjectionRegistry::Apply(const std::string &id,
                                      entt::registry &registry,
                                      entt::entity entity) {
  if (id.empty())
    return;

  auto it = injectors.find(id);
  if (it != injectors.end()) {
    it->second(registry, entity);
    LOG_DEBUG(
        "BehaviorInjectionRegistry: Applied behavior '{}' to entity {}", id,
        static_cast<uint32_t>(entity));
  } else {
    LOG_WARN("BehaviorInjectionRegistry: Unknown behavior ID '{}'", id);
  }
}

void BehaviorInjectionRegistry::Init() {
  if (!injectors.empty())
    return; // Already initialized

  Register(BehaviorID::ShadowCaster, [](entt::registry &r, entt::entity e) {
    r.get_or_emplace<ShadowKillArrayReady>(e);
  });

  LOG_INFO("BehaviorInjectionRegistry: Initialized with {} behaviors",
               injectors.size());
}

} // namespace NoMoreDay
