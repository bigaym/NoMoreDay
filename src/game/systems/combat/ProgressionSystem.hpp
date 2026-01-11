#pragma once
#include <entt/entity/registry.hpp>
#include "game/components/Stats.hpp"

namespace NoMoreDay {

class ProgressionSystem {
public:
    static constexpr int MAX_LEVEL = 100;

    /**
     * @brief 计算特定等级所需的经验值。
     */
    static float CalculateRequiredXP(int level);

    /**
     * @brief 根据等级差计算奖励的经验值。
     */
    static float CalculateAwardedXP(int playerLevel, int monsterLevel, float baseXP);





    /**
     * @brief 向实体添加经验并处理升级。
     */
    static void AddExperience(entt::registry& registry, entt::entity entity, float amount);

    /**
     * @brief 处理升级逻辑（属性增长、奖励点数）。
     */
    static void LevelUp(entt::registry& registry, entt::entity entity);

    /**
     * @brief 将属性点分配给特定的基础属性。
     */
    static bool AllocateAttribute(entt::registry& registry, entt::entity entity, StatType type);

    /**
     * @brief 分配技能点（技能树集成的占位符）。
     */
    static bool AllocateSkillPoint(entt::registry& registry, entt::entity entity);
};

} // namespace NoMoreDay
