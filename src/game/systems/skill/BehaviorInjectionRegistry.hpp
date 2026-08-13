#pragma once
#include <array>
#include <functional>
#include <entt/entt.hpp>
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"

namespace NoMoreDay {

class BehaviorInjectionRegistry {
public:
    using BehaviorInjector = std::function<void(entt::registry&, entt::entity)>;
    using Id = SkillBehaviorId;

    /**
     * @brief Register a behavior injection logic for a specific enum id.
     * @param id The behavior id (must not be SkillBehaviorId::None)
     * @param injector Lambda that modifies the entity (e.g., adds components)
     */
    static void Register(Id id, BehaviorInjector injector);

    /**
     * @brief Apply a specific behavior to an entity if it exists in the registry.
     */
    static void Apply(Id id, entt::registry& registry, entt::entity entity);

    /**
     * @brief Initialize default behaviors (called at startup).
     */
    static void Init();

private:
    static std::array<BehaviorInjector,
                      static_cast<std::size_t>(SkillBehaviorId::Count)>
        injectors;
};

} // namespace NoMoreDay
