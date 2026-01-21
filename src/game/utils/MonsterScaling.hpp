#pragma once

#include "game/components/EnemyComponent.hpp"
#include <array>

namespace NoMoreDay {

struct MonsterScalingResult {
    float maxHealth;
    float minDamage;
    float maxDamage;
    float armor;
    float resistanceBonus = 0.0f; // 抗性加成 (Lv 100+)
    float xpValue;
};

class MonsterScaling {
public:
    // 计算最终属性
    static MonsterScalingResult Calculate(
        EnemyRace::Type race,
        int level,
        EnemyRarityComponent::Rarity rarity);
    
    // 等级同步
    static int SyncLevel(int areaLevel, int playerLevel);
    
    // 经验乘数 (D3 风格)
    static float GetXPMultiplier(int monsterLevel, int playerLevel);
    
    // 稀有度乘数查询
    static float GetHPMultiplier(EnemyRarityComponent::Rarity rarity);
    static float GetDamageMultiplier(EnemyRarityComponent::Rarity rarity);
    static float GetResistanceGrowth(EnemyRarityComponent::Rarity rarity);
    
private:
    // 指数成长辅助
    static float PowerCurve(float rate, int level);
    
    // 护甲反推
    static float ComputeArmorForTargetDR(int level, float targetDR);
};

} // namespace NoMoreDay
