#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include <spdlog/spdlog.h>

namespace NoMoreDay {

std::unordered_map<std::string, BehaviorInjectionRegistry::BehaviorInjector> BehaviorInjectionRegistry::injectors;

void BehaviorInjectionRegistry::Register(const std::string& id, BehaviorInjector injector) {
    if (injectors.contains(id)) {
        spdlog::warn("BehaviorInjectionRegistry: Overwriting injector for ID '{}'", id);
    }
    injectors[id] = std::move(injector);
}

void BehaviorInjectionRegistry::Apply(const std::string& id, entt::registry& registry, entt::entity entity) {
    if (id.empty()) return;

    auto it = injectors.find(id);
    if (it != injectors.end()) {
        it->second(registry, entity);
    } else {
        // Optional: log warning if ID is not found but expected?
        // spdlog::trace("BehaviorInjectionRegistry: ID '{}' not found", id);
    }
}

void BehaviorInjectionRegistry::Init() {
    if (!injectors.empty()) return; // Already initialized

    // Example registration (Placeholders for future tasks)
    // Register("shadow_caster", [](entt::registry& r, entt::entity e) {
    //     r.emplace_or_replace<ShadowCasterTag>(e);
    // });
}

} // namespace NoMoreDay
