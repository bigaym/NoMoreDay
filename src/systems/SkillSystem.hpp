#pragma once
#include "entt/entt.hpp"
#include <functional>
#include "raylib.h"
#include "../core/TagRegistry.hpp"

namespace NoMoreDay {

enum class SkillState : uint8_t {
    Ready,
    Preparing, // Wind-up / Animation start
    Casting,   // Active / Projectile spawn
    Settle,    // Wind-down / Recovery
};

struct SkillExecution {
    uint32_t skill_id;
    SkillState state = SkillState::Ready;
    float timer = 0.0f;
    entt::entity owner;
    int slot_index = -1;
    Vector2 target_pos = {0, 0};
};

class SkillSystem {
public:
    using CastCallback = std::function<void(entt::registry&, entt::entity, uint32_t, Vector2)>;

    static void Update(entt::registry& registry, float dt);
    
    /**
     * @brief Attempt to cast a skill from a specific slot.
     */
    static bool TryCast(entt::registry& registry, entt::entity entity, int slot_index, Vector2 target_pos = {0, 0});

    /**
     * @brief Trigger a shadow cast of a skill.
     */
    static bool ShadowCast(entt::registry& registry, entt::entity owner, uint32_t skill_id, Vector2 position, Vector2 target_pos = {0,0});

    /**
     * @brief Register a callback for a specific skill ID.
     */
    static void RegisterEffect(uint32_t skill_id, CastCallback callback);

    /**
     * @brief Initialize default skill hooks (Flowing Thrust, etc.).
     */
    static void InitHooks();

    /**
     * @brief Called when a skill hit occurs to process interactions like Sword Intent.
     */
    static void OnSkillHit(entt::registry& registry, entt::entity attacker, entt::entity target, uint32_t skill_id, Tag hit_tags);

private:
    static void UpdateCooldowns(entt::registry& registry, float dt);
    static void UpdateStates(entt::registry& registry, float dt);
    static void UpdateSwordIntent(entt::registry& registry, float dt);
    static void UpdateShadows(entt::registry& registry, float dt);
};

}
