#include "game/foundation/utils/MonsterScaling.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Combat.hpp" // For ARMOR_BASE
#include "game/systems/combat/CombatConstants.hpp"
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
    // 使用最终等级重新计算 XP (高等级怪物给更多 XP)
    float xpOpsLevel = static_cast<float>(std::max(1, level)); 
    // 基础经验公式: Base * (1 + 0.05)^(Lv-1)
    // 这里我们使用传入的 level (包含了 Rarity Offset)
    // 这意味着 Boss (Lv+3) 会自然比同级普通怪多 (1.05^3) ≈ 1.15倍基础经验
    // 然后再乘积下面的 rarity multiplier
    
    // Recalculate growth based on actual entity level
    float xpGrowth = PowerCurve(XP_GROWTH_RATE, level);
    
    result.xpValue = baseXP * xpGrowth * xpMultiplier;

    return result;
}

int MonsterScaling::CalculateMonsterLevel(
    int areaLevel,
    int playerLevel,
    EnemyRarityComponent::Rarity rarity,
    bool isEndgameContent) {
    
    // 1. 基础等级判定
    // Campaign: 严格遵循区域等级 (允许玩家碾压)
    // Endgame: 动态等级，至少匹配玩家等级 (或区域等级更高)
    int baseLevel = areaLevel;
    
    if (isEndgameContent) {
        // Mosaic/Rift 模式下，怪物等级跟随玩家，但不会低于区域基础等级
        // 允许区域等级高于玩家(如高层深渊)
        baseLevel = std::max(areaLevel, playerLevel);
    } else {
        // Campaign 模式下，允许动态浮动，但有上限 (区域等级 + 2)
        // 这样玩家回低级图依然有割草感，但同级图有微调
        // 这里我们暂时保持 clear 的 Zone Level 设计
        // 如果想引入 "动态难度"，可以在这里 max(areaLevel, playerLevel - 5)
        baseLevel = areaLevel; 
    }

    // 2. 稀有度等级偏移 (Level Offset/Suppression)
    // Boss 高 3 级意味着玩家对 Boss 的命中率/暴击率会天然降低 (参考 WoW/PoE)
    int rarityOffset = 0;
    switch (rarity) {
        case EnemyRarityComponent::NORMAL: rarityOffset = 0; break;
        case EnemyRarityComponent::CHAMPION: rarityOffset = 1; break;
        case EnemyRarityComponent::ELITE: rarityOffset = 2; break;
        case EnemyRarityComponent::BOSS: rarityOffset = 3; break;
        case EnemyRarityComponent::NEMESIS: rarityOffset = 5; break;
    }

    return std::max(1, baseLevel + rarityOffset);
}

int MonsterScaling::SyncLevel(int areaLevel, int playerLevel) {
    // Deprecated or used as helper for Base Area Level determination if needed.
    // 目前保留作为简易 fallback
    return std::max(std::max(1, playerLevel - LEVEL_SYNC_OFFSET), std::max(1, areaLevel));
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
