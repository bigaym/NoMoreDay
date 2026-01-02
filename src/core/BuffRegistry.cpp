#include "BuffRegistry.hpp"

namespace NoMoreDay {

std::unordered_map<BuffType, BuffVisualData> BuffRegistry::registry;
BuffVisualData BuffRegistry::default_data = { "?", GRAY, "Unknown", "Unknown effect" };

void BuffRegistry::Initialize() {
    // Buffs (Green/Gold)
    registry[BuffType::AttackUp] = { "攻 ↑", GREEN, "Attack Up", "Increases attack damage." };
    registry[BuffType::DefenseUp] = { "防 ↑", GREEN, "Defense Up", "Increases defense." };
    registry[BuffType::SpeedUp] = { "速 ↑", GREEN, "Speed Up", "Increases movement speed." };
    registry[BuffType::CritRateUp] = { "暴 ↑", GOLD, "Crit Rate Up", "Increases critical hit chance." };
    registry[BuffType::CritDamageUp] = { "伤 ↑", GOLD, "Crit Damage Up", "Increases critical hit damage." };
    registry[BuffType::SwordIntent] = { "意", SKYBLUE, "Sword Intent", "Accumulated Sword Intent." };
    registry[BuffType::Shield] = { "盾", BLUE, "Shield", "Absorbs damage." };
    registry[BuffType::Invincible] = { "无", GOLD, "Invincible", "Immune to all damage." };

    // Debuffs (Red/Purple)
    registry[BuffType::AttackDown] = { "攻 ↓", RED, "Attack Down", "Decreases attack damage." };
    registry[BuffType::DefenseDown] = { "防 ↓", RED, "Defense Down", "Decreases defense." };
    registry[BuffType::SpeedDown] = { "速 ↓", RED, "Speed Down", "Decreases movement speed." };
    registry[BuffType::Stun] = { "晕", PURPLE, "Stunned", "Cannot move or act." };
    registry[BuffType::Freeze] = { "冻", SKYBLUE, "Frozen", "Cannot move or act." };
    registry[BuffType::Burn] = { "燃", ORANGE, "Burning", "Taking fire damage over time." };
    registry[BuffType::Poison] = { "毒", GREEN, "Poisoned", "Taking poison damage over time." };
    registry[BuffType::Bleed] = { "血", RED, "Bleeding", "Taking physical damage over time." };
}

void BuffRegistry::Shutdown() {
    registry.clear();
}

const BuffVisualData& BuffRegistry::GetVisualData(BuffType type) {
    if (registry.find(type) != registry.end()) {
        return registry[type];
    }
    return default_data;
}

} // namespace NoMoreDay