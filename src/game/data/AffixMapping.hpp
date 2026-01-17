#pragma once
#include <string_view>
#include <array>
#include "game/components/MapFragmentComponent.hpp"
#include "game/components/Stats.hpp"

namespace NoMoreDay {

/**
 * @brief 词缀名称映射条目 (中英文对照)
 */
struct AffixNameEntry {
    std::string_view en;
    std::string_view zh;
};

/**
 * @brief 碎片元素中文映射 (基于 FragmentElement 枚举)
 */
constexpr std::array<std::string_view, 6> FragmentElementzh = {
    "基础", // None
    "灼热", // Fire
    "凛冬", // Cold
    "惊雷", // Lightning
    "幽冥", // Shadow
    "混沌"  // Chaos
};

/**
 * @brief 碎片类型中文映射 (基于 FragmentType 枚举)
 */
constexpr std::array<std::string_view, 3> FragmentTypezh = {
    "地形", // Terrain
    "词缀", // Affix
    "特殊"  // Unique
};

/**
 * @brief 基础属性中文映射 (基于 StatType 枚举)
 * 线性表存储，通过枚举值作为索引
 */
constexpr std::array<std::string_view, static_cast<size_t>(StatType::Count)> StatTypezh = []() {
    std::array<std::string_view, static_cast<size_t>(StatType::Count)> arr{};
    arr[static_cast<size_t>(StatType::Strength)] = "力量";
    arr[static_cast<size_t>(StatType::Dexterity)] = "敏捷";
    arr[static_cast<size_t>(StatType::Intelligence)] = "智力";
    arr[static_cast<size_t>(StatType::Vitality)] = "体质";
    arr[static_cast<size_t>(StatType::MaxHealth)] = "最大生命";
    arr[static_cast<size_t>(StatType::MaxMana)] = "最大法力";
    arr[static_cast<size_t>(StatType::MoveSpeed)] = "移动速度";
    arr[static_cast<size_t>(StatType::Armor)] = "护甲";
    
    // 进攻
    arr[static_cast<size_t>(StatType::PhysicalDamage)] = "物理伤害";
    arr[static_cast<size_t>(StatType::FireDamage)] = "火焰伤害";
    arr[static_cast<size_t>(StatType::ColdDamage)] = "冰霜伤害";
    arr[static_cast<size_t>(StatType::LightningDamage)] = "闪电伤害";
    arr[static_cast<size_t>(StatType::PoisonDamage)] = "毒素伤害";
    arr[static_cast<size_t>(StatType::ShadowDamage)] = "暗影伤害";
    
    arr[static_cast<size_t>(StatType::CritChance)] = "暴击率";
    arr[static_cast<size_t>(StatType::CritDamage)] = "暴击倍率";
    arr[static_cast<size_t>(StatType::AttackSpeed)] = "攻击速度";
    arr[static_cast<size_t>(StatType::CastSpeed)] = "施法速度";
    arr[static_cast<size_t>(StatType::Accuracy)] = "命中率";
    arr[static_cast<size_t>(StatType::ManaOnHit)] = "击中回蓝";
    arr[static_cast<size_t>(StatType::ArmorPenetration)] = "护甲穿透";
    
    // 防御
    arr[static_cast<size_t>(StatType::ResistPhysical)] = "物理抗性";
    arr[static_cast<size_t>(StatType::ResistFire)] = "火焰抗性";
    arr[static_cast<size_t>(StatType::ResistCold)] = "冰霜抗性";
    arr[static_cast<size_t>(StatType::ResistLightning)] = "闪电抗性";
    arr[static_cast<size_t>(StatType::ResistPoison)] = "毒素抗性";
    arr[static_cast<size_t>(StatType::ResistShadow)] = "暗影抗性";
    arr[static_cast<size_t>(StatType::ResistAll)] = "全抗性";
    
    // 其他
    arr[static_cast<size_t>(StatType::CooldownReduction)] = "冷却缩减";
    arr[static_cast<size_t>(StatType::ResourceCostReduction)] = "法力消耗降低";
    arr[static_cast<size_t>(StatType::ProjectileCount)] = "投射物数量";
    arr[static_cast<size_t>(StatType::AreaScale)] = "影响范围";
    arr[static_cast<size_t>(StatType::ProjectileSpeed)] = "投射物速度";
    arr[static_cast<size_t>(StatType::DurationScale)] = "持续时间加成";
    
    arr[static_cast<size_t>(StatType::DodgeChance)] = "闪避率";
    arr[static_cast<size_t>(StatType::BlockChance)] = "格挡率";
    arr[static_cast<size_t>(StatType::LifeSteal)] = "吸血";
    arr[static_cast<size_t>(StatType::LifeOnHit)] = "击中生命回复";
    arr[static_cast<size_t>(StatType::HealthRegen)] = "生命秒回";
    arr[static_cast<size_t>(StatType::ManaRegen)] = "法力秒回";
    arr[static_cast<size_t>(StatType::Thorns)] = "反震伤害";
    arr[static_cast<size_t>(StatType::MagicFind)] = "物品掉落率";
    
    return arr;
}();

} // namespace NoMoreDay
