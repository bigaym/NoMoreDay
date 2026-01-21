#include "game/utils/MonsterScaling.hpp"
#include "game/components/Common.hpp"
#include "game/components/Combat.hpp" // For ARMOR_BASE
#include <cmath>
#include <algorithm>

namespace NoMoreDay {

using namespace NoMoreDay::Constants::Combat::Scaling::Monster;

MonsterScalingResult MonsterScaling::Calculate(
    EnemyRace::Type race,
    int level,
    EnemyRarityComponent::Rarity rarity) {
    
    MonsterScalingResult result{};

    // 1. 获取基础种族数据
    const auto& raceData = kRaceData[static_cast<size_t>(race)];
    float baseHP = raceData.baseHP;
    float baseDmg = raceData.baseDamage;
    float baseXP = raceData.baseXP;

    // 2. 指数成长计算
    // HP: Base * (1 + rate)^(lv-1)
    float hpMultiplier = PowerCurve(HP_GROWTH_RATE, level);
    float dmgMultiplier = PowerCurve(DMG_GROWTH_RATE, level);
    float xpMultiplier = PowerCurve(XP_GROWTH_RATE, level);

    // 3. 应用稀有度乘数
    hpMultiplier *= GetHPMultiplier(rarity);
    dmgMultiplier *= GetDamageMultiplier(rarity);
    if (rarity == EnemyRarityComponent::ELITE) xpMultiplier *= 2.5f;
    else if (rarity == EnemyRarityComponent::BOSS) xpMultiplier *= 10.0f;
    // Note: Other rarities use base XP or handled elsewhere? Plan didn't specify XP rarity mult perfectly but Spec says:
    // "float finalXP = baseXP × (1 + XP_GROWTH_RATE)^(Lv - 1) × xpMult;" 
    // Wait, the spec integrated XPAwardingSystem logic into spec. 
    // In XPAwardingSystem.cpp, existing logic is: if ELITE * 2.5, if BOSS * 10.0.
    // I will preserve that logic in result.xpValue, combining with growth.

    result.maxHealth = baseHP * hpMultiplier;
    
    // Damage logic
    float centerDmg = baseDmg * dmgMultiplier;
    result.minDamage = centerDmg * DMG_VARIANCE_MIN;
    result.maxDamage = centerDmg * DMG_VARIANCE_MAX;

    // 4.计算护甲
    // Target DR = min(0.20, 0.002 * (level - 1))
    float targetDR = std::min(TARGET_DR_AT_100, DR_PER_LEVEL * (std::max(1, level) - 1));
    result.armor = ComputeArmorForTargetDR(level, targetDR);

    // 5. 计算额外抗性 (Lv 100+)
    result.resistanceBonus = 0.0f;
    if (level > 100) {
        int overLevel = level - 100;
        float resPerLevel = GetResistanceGrowth(rarity);
        result.resistanceBonus = overLevel * resPerLevel;
    }
    
    // XP Calculation
    result.xpValue = baseXP * xpMultiplier;

    return result;
}

int MonsterScaling::SyncLevel(int areaLevel, int playerLevel) {
    int minLevel = std::max(1, playerLevel - LEVEL_SYNC_OFFSET);
    // 区域等级至少为1
    int effectiveAreaLevel = std::max(1, areaLevel);
    // 怪物等级取 max(玩家-5, 区域等级)
    // 这样当玩家远超区域等级时，怪物不会变得太弱（但也不会因为玩家等级高就无限变强，除非是动态区域）
    // WAIT: Spec says "max(minLevel, std::max(1, areaLevel))".
    // This design implies monsters scale UP to player level even in low zones?
    // "Synchronize monster level with the player's level, considering a level difference of -5"
    // Usually ARPGs do NOT scale low zones up unless it's a specific mode (Scaling Mode).
    // But the spec says: "SymcLevel(areaLevel, playerLevel) ... return max(minLevel, ...)"
    // So yes, monsters will scale up to at least Player-5.
    return std::max(minLevel, effectiveAreaLevel);
}

float MonsterScaling::GetXPMultiplier(int monsterLevel, int playerLevel) {
    int diff = std::abs(playerLevel - monsterLevel);
    if (diff <= (int)XP_DIFF_THRESHOLD) {
        return 1.0f;
    }
    float penalty = (diff - XP_DIFF_THRESHOLD) * XP_PENALTY_PER_LEVEL;
    return std::max(XP_MIN_MULT, 1.0f - penalty);
}

float MonsterScaling::GetHPMultiplier(EnemyRarityComponent::Rarity rarity) {
    switch (rarity) {
        case EnemyRarityComponent::NORMAL: return RARITY_HP_NORMAL;
        case EnemyRarityComponent::CHAMPION: return RARITY_HP_CHAMPION;
        case EnemyRarityComponent::ELITE: return RARITY_HP_ELITE;
        case EnemyRarityComponent::BOSS: return RARITY_HP_BOSS;
        case EnemyRarityComponent::NEMESIS: return RARITY_HP_NEMESIS; // Fallback if added
        default: return RARITY_HP_NORMAL;
    }
}

float MonsterScaling::GetDamageMultiplier(EnemyRarityComponent::Rarity rarity) {
    switch (rarity) {
        case EnemyRarityComponent::NORMAL: return RARITY_DMG_NORMAL;
        case EnemyRarityComponent::CHAMPION: return RARITY_DMG_CHAMPION;
        case EnemyRarityComponent::ELITE: return RARITY_DMG_ELITE;
        case EnemyRarityComponent::BOSS: return RARITY_DMG_BOSS;
        case EnemyRarityComponent::NEMESIS: return RARITY_DMG_NEMESIS;
        default: return RARITY_DMG_NORMAL;
    }
}

float MonsterScaling::GetResistanceGrowth(EnemyRarityComponent::Rarity rarity) {
    switch (rarity) {
        case EnemyRarityComponent::NORMAL: return RES_GROWTH_NORMAL;
        case EnemyRarityComponent::CHAMPION: return RES_GROWTH_CHAMPION;
        case EnemyRarityComponent::ELITE: return RES_GROWTH_ELITE;
        case EnemyRarityComponent::BOSS: return RES_GROWTH_BOSS;
        case EnemyRarityComponent::NEMESIS: return RES_GROWTH_NEMESIS;
        default: return RES_GROWTH_NORMAL;
    }
}

float MonsterScaling::PowerCurve(float rate, int level) {
    if (level <= 1) return 1.0f;
    return std::pow(1.0f + rate, static_cast<float>(level - 1));
}

float MonsterScaling::ComputeArmorForTargetDR(int level, float targetDR) {
    // DR = Armor / (Armor + LevelFactor * ARMOR_BASE)
    // Armor * (1 - DR) = DR * LevelFactor * ARMOR_BASE
    // Armor = (DR * LevelFactor * ARMOR_BASE) / (1 - DR)
    
    if (targetDR >= 0.99f) targetDR = 0.99f; // Prevent div by zero
    if (targetDR <= 0.0f) return 0.0f;

    using namespace NoMoreDay::Constants::Combat::Pipeline;
    using namespace NoMoreDay::Constants::Combat::Scaling;
    
    float lv = static_cast<float>(level);
    float levelFactor = LEVEL_BASE + lv * LEVEL_LINEAR + lv * lv * LEVEL_QUADRATIC;
    
    return (targetDR * levelFactor * ARMOR_BASE) / (1.0f - targetDR);
}

} // namespace NoMoreDay
