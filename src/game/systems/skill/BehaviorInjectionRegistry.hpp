#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <entt/entt.hpp>
#include "game/foundation/components/Common.hpp"

namespace NoMoreDay {

class BehaviorInjectionRegistry {
public:
    using BehaviorInjector = std::function<void(entt::registry&, entt::entity)>;

    /**
     * @brief Register a behavior injection logic for a specific ID.
     * @param id The behavior ID (must match what's in skills.json/TalentNode)
     * @param injector Lambda that modifies the entity (e.g., adds components)
     */
    static void Register(const std::string& id, BehaviorInjector injector);

    /**
     * @brief Apply a specific behavior to an entity if it exists in the registry.
     */
    static void Apply(const std::string& id, entt::registry& registry, entt::entity entity);

    /**
     * @brief Initialize default behaviors (called at startup).
     */
    static void Init();

private:
    static std::unordered_map<std::string, BehaviorInjector> injectors;
};

} // namespace NoMoreDay
