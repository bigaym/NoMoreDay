#include "ProgressionSystem.hpp"
#include "../components/PlayerState.hpp"
#include "../components/Stats.hpp"
#include <cmath>
#include <algorithm>

namespace NoMoreDay {

float ProgressionSystem::CalculateRequiredXP(int level) {
    if (level <= 0) return 100.0f;
    // Exponential curve: 100 * (level ^ 1.5)
    // Round to nearest integer for cleaner numbers
    return std::floor(100.0f * std::pow(static_cast<float>(level), 1.5f));
}

float ProgressionSystem::CalculateAwardedXP(int playerLevel, int monsterLevel, float baseXP) {
    float multiplier = 1.0f;
    if (playerLevel > monsterLevel) {
        float levelDifference = static_cast<float>(playerLevel - monsterLevel);
        // 10% reduction per level difference, minimum 10% of base XP
        multiplier = std::max(0.1f, 1.0f - levelDifference * 0.1f);
    }
    // No bonus for lower player level (higher monster level) as per spec focus on reduction.
    return baseXP * multiplier;
}

void ProgressionSystem::AddExperience(entt::registry& registry, entt::entity entity, float amount) {
    if (!registry.all_of<PlayerStats>(entity)) return;

    auto& stats = registry.get<PlayerStats>(entity);
    stats.current_xp += amount;

    // Check for level ups (loop to handle multiple levels at once)
    while (stats.current_xp >= stats.required_xp) {
        stats.current_xp -= stats.required_xp;
        LevelUp(registry, entity);
        // Refresh required XP for the new level
        stats.required_xp = CalculateRequiredXP(stats.level);
    }
}

void ProgressionSystem::LevelUp(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<PlayerStats>(entity)) return;

    auto& stats = registry.get<PlayerStats>(entity);
    stats.level++;
    
    // Update PlayerLevel component if it exists
    if (auto* levelComp = registry.try_get<PlayerLevel>(entity)) {
        levelComp->value = stats.level;
    }

    // Award Points
    stats.available_attribute_points += 5;
    stats.available_skill_points += 1;

    // Baseline Stat Growth
    if (auto* primStats = registry.try_get<PrimaryStats>(entity)) {
        primStats->strength += 2.0f;
        primStats->dexterity += 1.0f;
        primStats->intelligence += 1.0f;
        primStats->vitality += 2.0f;
        
        // Mark stats as dirty to trigger recalculation of CombatStats
        registry.get_or_emplace<StatsDirty>(entity);
    }
}

bool ProgressionSystem::AllocateAttribute(entt::registry& registry, entt::entity entity, StatType type) {
    if (!registry.all_of<PlayerStats, PrimaryStats>(entity)) return false;

    auto& stats = registry.get<PlayerStats>(entity);
    if (stats.available_attribute_points <= 0) return false;

    auto& primStats = registry.get<PrimaryStats>(entity);
    
    switch (type) {
        case StatType::Strength:     primStats.strength += 1.0f; break;
        case StatType::Dexterity:    primStats.dexterity += 1.0f; break;
        case StatType::Intelligence: primStats.intelligence += 1.0f; break;
        case StatType::Vitality:     primStats.vitality += 1.0f; break;
        default: return false; // Other stats can't be directly allocated
    }

    stats.available_attribute_points--;
    registry.get_or_emplace<StatsDirty>(entity);
    return true;
}

bool ProgressionSystem::AllocateSkillPoint(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<PlayerStats>(entity)) return false;

    auto& stats = registry.get<PlayerStats>(entity);
    if (stats.available_skill_points <= 0) return false;

    // Currently just decrements the point as Skill Tree is out of scope
    stats.available_skill_points--;
    return true;
}

} // namespace NoMoreDay