#pragma once
#include <unordered_map>
#include <functional>
#include <entt/entt.hpp>
#include "game/components/SkillDefs.hpp"

namespace NoMoreDay {

// Forward declarations
struct SkillExecution;
struct ChannelingComponent;

/**
 * @brief Registry for skill behavior functions.
 * 
 * Stores function pointers for skill callbacks, allowing skills to be
 * registered from separate translation units while being called from
 * SkillSystem.
 * 
 * All functions are static function pointers (no std::function overhead).
 */
class SkillBehaviorRegistry {
public:
    using CastFunc = void(*)(entt::registry&, entt::entity, SkillExecution&);
    using TickFunc = void(*)(entt::registry&, entt::entity, ChannelingComponent&, float);
    using EndFunc = void(*)(entt::registry&, entt::entity, uint32_t);
    using HitFunc = void(*)(entt::registry&, entt::entity, entt::entity, Tag, bool);
    
    /**
     * @brief Register a skill's OnCast callback.
     */
    static void RegisterCast(uint32_t skill_id, CastFunc func);
    
    /**
     * @brief Register a skill's OnTick callback (for channeled skills).
     */
    static void RegisterTick(uint32_t skill_id, TickFunc func);
    
    /**
     * @brief Register a skill's OnEnd callback.
     */
    static void RegisterEnd(uint32_t skill_id, EndFunc func);
    
    /**
     * @brief Register a skill's OnHit callback.
     */
    static void RegisterHit(uint32_t skill_id, HitFunc func);
    
    /**
     * @brief Get the OnCast callback for a skill.
     * @return The callback, or nullptr if not registered.
     */
    static CastFunc GetCast(uint32_t skill_id);
    
    /**
     * @brief Get the OnTick callback for a skill.
     * @return The callback, or nullptr if not registered.
     */
    static TickFunc GetTick(uint32_t skill_id);
    
    /**
     * @brief Get the OnEnd callback for a skill.
     * @return The callback, or nullptr if not registered.
     */
    static EndFunc GetEnd(uint32_t skill_id);
    
    /**
     * @brief Get the OnHit callback for a skill.
     * @return The callback, or nullptr if not registered.
     */
    static HitFunc GetHit(uint32_t skill_id);
    
    /**
     * @brief Clear all registered callbacks (for testing).
     */
    static void Clear();
    
    /**
     * @brief Initialize all skill behaviors.
     * This function should be called during system initialization to ensure
     * that all skill behavior translation units are linked and their static
     * initializers are executed.
     */
    static void Initialize();
    
    /**
     * @brief Check if a skill has any registered behaviors.
     */
    static bool HasBehavior(uint32_t skill_id);

private:
    static std::unordered_map<uint32_t, CastFunc>& GetCastMap();
    static std::unordered_map<uint32_t, TickFunc>& GetTickMap();
    static std::unordered_map<uint32_t, EndFunc>& GetEndMap();
    static std::unordered_map<uint32_t, HitFunc>& GetHitMap();
};

/**
 * @brief Helper macro for auto-registering skill behaviors.
 * 
 * Usage:
 *   REGISTER_SKILL_BEHAVIOR(FlowingThrust);
 */
#define REGISTER_SKILL_BEHAVIOR(SkillClass) \
    namespace { \
        static const int s_register_##SkillClass = []{ \
            NoMoreDay::SkillBehaviorRegistry::RegisterCast( \
                SkillClass::kSkillId, &SkillClass::OnCast); \
            NoMoreDay::SkillBehaviorRegistry::RegisterHit( \
                SkillClass::kSkillId, &SkillClass::OnHit); \
            return 0; \
        }(); \
    }

/**
 * @brief Helper macro for auto-registering channeled skill behaviors.
 */
#define REGISTER_CHANNELED_SKILL_BEHAVIOR(SkillClass) \
    namespace { \
        static const int s_register_##SkillClass = []{ \
            NoMoreDay::SkillBehaviorRegistry::RegisterCast( \
                SkillClass::kSkillId, &SkillClass::OnCast); \
            NoMoreDay::SkillBehaviorRegistry::RegisterTick( \
                SkillClass::kSkillId, &SkillClass::OnTick); \
            NoMoreDay::SkillBehaviorRegistry::RegisterHit( \
                SkillClass::kSkillId, &SkillClass::OnHit); \
            return 0; \
        }(); \
    }

} // namespace NoMoreDay
