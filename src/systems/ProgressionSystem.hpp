#pragma once
#include <entt/entity/registry.hpp>
#include "../components/Stats.hpp"

namespace NoMoreDay {

class ProgressionSystem {
public:
    /**
     * @brief Calculate required XP for a specific level.
     */
    static float CalculateRequiredXP(int level);

    /**
     * @brief Calculate awarded XP based on level difference.
     */
    static float CalculateAwardedXP(int playerLevel, int monsterLevel, float baseXP);





    /**
     * @brief Adds experience to an entity and handles level-ups.
     */
    static void AddExperience(entt::registry& registry, entt::entity entity, float amount);

    /**
     * @brief Handles level-up logic (stat growth, awarding points).
     */
    static void LevelUp(entt::registry& registry, entt::entity entity);

    /**
     * @brief Allocates an attribute point to a specific primary stat.
     */
    static bool AllocateAttribute(entt::registry& registry, entt::entity entity, StatType type);

    /**
     * @brief Allocates a skill point (placeholder for skill tree integration).
     */
    static bool AllocateSkillPoint(entt::registry& registry, entt::entity entity);
};

} // namespace NoMoreDay
