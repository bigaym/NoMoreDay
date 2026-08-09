#include "game/foundation/data/BuffRegistry.hpp"
#include "engine/resource/BuffAssetRegistry.hpp"

// TODO: 后续需要更详细的对应
namespace NoMoreDay {

std::unordered_map<BuffType, BuffVisualData> BuffRegistry::registry;
BuffVisualData BuffRegistry::default_data = { "?", GRAY, "Unknown", "Unknown effect", false, nullptr };

void BuffRegistry::Initialize() {
    // Buffs (Green/Gold)
    registry[BuffType::AttackUp] = { "攻 ↑", GREEN, "攻击提升", "增加造成的攻击伤害。", false, &assets::buffs::general::melee_phys_dmg };
    registry[BuffType::DefenseUp] = { "防 ↑", GREEN, "防御提升", "增加护甲与防御力。", false, &assets::buffs::general::flat_armor };
    registry[BuffType::SpeedUp] = { "速 ↑", GREEN, "加速", "增加移动速度。", false, &assets::buffs::general::move_speed };
    registry[BuffType::CritRateUp] = { "暴 ↑", GOLD, "暴击提升", "增加暴击几率。", false, &assets::buffs::general::crit_chance };
    registry[BuffType::CritDamageUp] = { "伤 ↑", GOLD, "暴伤提升", "增加暴击伤害倍率。", false, &assets::buffs::general::spell_crit };
    registry[BuffType::SwordIntent] = { "意", SKYBLUE, "剑意", "积攒的剑道感悟，提升特定技能威力。", false, nullptr };
    registry[BuffType::Shield] = { "盾", BLUE, "护盾", "吸收即将受到的伤害。", false, &assets::buffs::general::pct_armor };
    registry[BuffType::Invincible] = { "无", GOLD, "无敌", "免疫所有伤害。", false, nullptr };
    registry[BuffType::PowerBoost] = { "力", ORANGE, "力量爆发", "获得巨大的力量增幅。", false, &assets::buffs::general::strength };
    registry[BuffType::Bloodlust] = { "血", RED, "嗜血", "击杀后获得，大幅提升攻击速度。", false, &assets::buffs::general::attack_speed };
    registry[BuffType::BloodSea] = { "海", Color{176, 62, 74, 255}, "Blood Sea", "A living crimson field follows the blade and reveals its current form on hover.", false, &assets::buffs::general::buff_xuehai };
    registry[BuffType::Hurt] = { "伤", RED, "受伤", "最近受到了伤害。", true, nullptr };

    // Debuffs (Red/Purple)
    registry[BuffType::AttackDown] = { "攻 ↓", RED, "攻击降低", "减少造成的攻击伤害。", true, nullptr };
    registry[BuffType::DefenseDown] = { "防 ↓", RED, "防御降低", "减少护甲与防御力。", true, nullptr };
    registry[BuffType::SpeedDown] = { "速 ↓", RED, "减速", "减少移动速度。", true, nullptr };
    registry[BuffType::Stun] = { "晕", PURPLE, "眩晕", "无法移动或执行动作。", true, nullptr };
    registry[BuffType::Freeze] = { "冻", SKYBLUE, "冻结", "被寒气冻结，无法行动。", true, &assets::buffs::general::resist_cold };
    registry[BuffType::Burn] = { "燃", ORANGE, "燃烧", "持续受到火焰伤害。", true, &assets::buffs::general::flat_fire };
    registry[BuffType::Poison] = { "毒", GREEN, "中毒", "持续受到毒素伤害。", true, &assets::buffs::general::resist_all };
    registry[BuffType::Bleed] = { "血", RED, "流血", "持续受到物理切割伤害。", true, &assets::buffs::general::flat_health };
}

void BuffRegistry::Shutdown() {
    registry.clear();
}

const BuffVisualData& BuffRegistry::GetVisualData(BuffType type) {
    auto it = registry.find(type);
    if (it != registry.end()) {
        return it->second;
    }
    return default_data;
}

} // namespace NoMoreDay
