#pragma once
#include "entt/entt.hpp"
#include <functional>
#include <taskflow/taskflow.hpp>
#include "raylib.h"
#include "../core/TagRegistry.hpp"
#include "../components/SkillSystem.hpp"

namespace NoMoreDay {
namespace systems { class SpatialHashGrid; }

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
    
    // Optional snapshot for shadow casts or delayed effects
    bool has_snapshot = false;
    SkillSnapshot snapshot;

    bool is_empowered = false;
};

class SkillSystem {
public:
    using CastCallback = std::function<void(entt::registry&, entt::entity, SkillExecution&)>;
    using SkillHook = std::function<void(entt::registry&, entt::entity, SkillExecution&)>;

    static void Update(entt::registry& registry, systems::SpatialHashGrid& grid, float dt, tf::Executor* executor = nullptr);
    
    /**
     * @brief Attempt to cast a skill from a specific slot.
     */
    static bool TryCast(entt::registry& registry, entt::entity entity, int slot_index, Vector2 target_pos = {0, 0});

    /**
     * @brief Handles input for a skill slot. Starts casting if idle, or maintains channeling if active.
     */
    static void HandleSkillInput(entt::registry& registry, entt::entity entity, int slot_index, Vector2 target_pos);

    /**
     * @brief Trigger a shadow cast of a skill.
     */
    static bool ShadowCast(entt::registry& registry, entt::entity owner, uint32_t skill_id, Vector2 position, Vector2 target_pos = {0,0});

    /**
     * @brief Register a callback for a specific skill ID.
     */
    static void RegisterEffect(uint32_t skill_id, CastCallback callback);

    /**
     * @brief Register a hook called before the skill effect is triggered (during Preparing state).
     */
    static void AddPreCastHook(SkillHook hook);

    /**
     * @brief Register a hook called after the skill effect is triggered or finishes.
     */
    static void AddPostCastHook(SkillHook hook);

    /**
     * @brief Clear all registered hooks (mainly for testing).
     */
    static void ClearHooks();

    /**
     * @brief Initialize default skill hooks (Flowing Thrust, etc.).
     */
    static void InitHooks();

    /**
     * @brief Called when a skill hit occurs to process interactions like Sword Intent.
     */
    static void OnSkillHit(entt::registry& registry, entt::entity attacker, entt::entity target, uint32_t skill_id, Tag hit_tags, bool is_crit = false);

    /**
     * @brief Allocate a talent point to a specific skill's talent node.
     */
    static bool AddTalentPoint(entt::registry& registry, entt::entity entity, uint32_t skill_id, uint32_t node_id);

    /**
     * @brief Reset all talent points for a specific skill and refund them.
     */
    static bool ResetTalents(entt::registry& registry, entt::entity entity, uint32_t skill_id);

    /**
     * @brief Reset all talent points for ALL skills and refund them.
     */
    static bool ClearAllTalents(entt::registry& registry, entt::entity entity);

    static void UpdateSwordIntent(entt::registry& registry, float dt);
    static void UpdateCooldowns(entt::registry& registry, float dt);
    static void UpdateStates(entt::registry& registry, float dt);

private:
};

}
