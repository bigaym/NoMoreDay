#pragma once

#include "../../foundation/data/MapAffix.hpp"
#include "../../foundation/components/Stats.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <array>

namespace NoMoreDay {

struct MapAffixDefinition {
    std::string_view name;        // e.g. "Swarming", "of Iron"
    std::string_view nameZh;      // e.g. "虫群之", "钢铁之"
    std::string_view descriptionTemplate; // e.g. "怪物密度: +{value}%"
    MapAffixCategory category;
    float difficultyWeight;  // 0.5 to 3.0
    float valT1;             // Value at Tier 1
    float valT10;            // Value at Tier 10
    bool isSuffix;           // true if it goes after map name
    // 战斗属性映射（唯一事实来源，生成器与适配器共用）
    // StatType::Count 为哨兵值，表示该词缀无直接战斗属性映射
    StatType combatStat = StatType::Count;
};

class MapAffixRegistry {
public:
    static void Initialize();
    
    static constexpr const MapAffixDefinition& GetDef(MapAffixType type) {
        size_t idx = static_cast<size_t>(type);
        if (idx >= static_cast<size_t>(MapAffixType::Count)) {
            return G_AFFIX_DEFINITIONS[0]; // Unknown fallback
        }
        return G_AFFIX_DEFINITIONS[idx];
    }

    static std::vector<MapAffixType> GetAvailableAffixes(MapAffixCategory category);
    static float CalculateValue(MapAffixType type, int tier);
    static std::string FormatDescription(MapAffixType type, float value);
    
    static constexpr std::array<MapAffixDefinition, static_cast<size_t>(MapAffixType::Count)> G_AFFIX_DEFINITIONS = {
        // DropRarity
        MapAffixDefinition{"Gilded", "镀金之", "物品寻宝率: +{value}%", MapAffixCategory::Buff, 0.0f, 20.0f, 150.0f, false, StatType::Count},
        // DropQuantity
        MapAffixDefinition{"Opulent", "富裕之", "物品掉落数量: +{value}%", MapAffixCategory::Buff, 0.0f, 10.0f, 100.0f, false, StatType::Count},
        // MonsterDensity
        MapAffixDefinition{"Swarming", "虫群之", "怪物密度: +{value}%", MapAffixCategory::Debuff, 0.5f, 10.0f, 50.0f, false, StatType::Count},
        // MonsterLevel
        MapAffixDefinition{"Nightmare", "梦魇之", "怪物等级: +{value}", MapAffixCategory::Debuff, 0.5f, 1.0f, 5.0f, false, StatType::Count},
        // Enemy_ExtraHealth
        MapAffixDefinition{"of the Colossus", "巨像之", "怪物生命值: +{value}%", MapAffixCategory::Debuff, 1.0f, 20.0f, 100.0f, true, StatType::MaxHealth},
        // Enemy_ExtraBarrier
        MapAffixDefinition{"of the Aegis", "庇护之", "怪物护盾值: +{value}%", MapAffixCategory::Debuff, 1.0f, 20.0f, 80.0f, true, StatType::Count},
        // Enemy_BarrierRegen
        MapAffixDefinition{"of Restoration", "复苏之", "怪物护盾回复: +{value}/s", MapAffixCategory::Debuff, 1.0f, 1.0f, 5.0f, true, StatType::Count},
        // Enemy_Armor
        MapAffixDefinition{"of Iron", "钢铁之", "怪物护甲: +{value}", MapAffixCategory::Debuff, 1.0f, 500.0f, 3000.0f, true, StatType::Count},
        // Enemy_Dodge
        MapAffixDefinition{"of Mist", "迷雾之", "怪物闪避: +{value}", MapAffixCategory::Debuff, 1.0f, 500.0f, 2500.0f, true, StatType::Count},
        // Enemy_ResistAll
        MapAffixDefinition{"of Prism", "棱镜之", "怪物全抗性: +{value}%", MapAffixCategory::Debuff, 1.0f, 5.0f, 20.0f, true, StatType::Count},
        // Enemy_ResistPhys
        MapAffixDefinition{"of Granite", "花岗岩之", "怪物物理抗性: +{value}%", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true, StatType::Count},
        // Enemy_ResistFire
        MapAffixDefinition{"of Embers", "灰烬之", "怪物火焰抗性: +{value}%", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true, StatType::Count},
        // Enemy_ResistCold
        MapAffixDefinition{"of Frost", "霜冻之", "怪物冰霜抗性: +{value}%", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true, StatType::Count},
        // Enemy_ResistLight
        MapAffixDefinition{"of Storms", "风暴之", "怪物闪电抗性: +{value}%", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true, StatType::Count},
        // Enemy_ResistPois
        MapAffixDefinition{"of Venom", "剧毒之", "怪物毒素抗性: +{value}%", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true, StatType::Count},
        // Enemy_ResistVoid
        MapAffixDefinition{"of Null", "虚无之", "怪物虚无抗性: +{value}%", MapAffixCategory::Debuff, 1.0f, 25.0f, 60.0f, true, StatType::Count},
        // Enemy_CritResist
        MapAffixDefinition{"of Adamant", "坚韧之", "怪物爆伤减免: +{value}%", MapAffixCategory::Debuff, 1.0f, 30.0f, 60.0f, true, StatType::Count},
        // Enemy_ExtraDamage
        MapAffixDefinition{"of Violence", "暴力之", "怪物伤害: +{value}%", MapAffixCategory::Debuff, 2.0f, 15.0f, 50.0f, true, StatType::PhysicalDamage},
        // Enemy_Fast
        MapAffixDefinition{"of Frenzy", "狂乱之", "怪物速度: +{value}%", MapAffixCategory::Debuff, 2.0f, 10.0f, 35.0f, true, StatType::MoveSpeed},
        // Enemy_CritChance
        MapAffixDefinition{"of Precision", "精准之", "怪物暴击率: +{value}%", MapAffixCategory::Debuff, 2.0f, 20.0f, 100.0f, true, StatType::Count},
        // Enemy_ArmorShred
        MapAffixDefinition{"of Sundering", "穿透之", "怪物护甲穿透: +{value}", MapAffixCategory::Debuff, 2.0f, 5.0f, 15.0f, true, StatType::Count},
        // Player_ResistRedAll
        MapAffixDefinition{"of Exposure", "暴露之", "玩家全抗性: -{value}%", MapAffixCategory::Debuff, 3.0f, 5.0f, 20.0f, true, StatType::Count},
        // Player_ResistRedSpecific
        MapAffixDefinition{"of Vulnerability", "脆弱之", "玩家特定抗性: -{value}%", MapAffixCategory::Debuff, 3.0f, 15.0f, 40.0f, true, StatType::Count},
        // Player_RedRecovery
        MapAffixDefinition{"of Atrophy", "萎缩之", "玩家回复效率: -{value}%", MapAffixCategory::Debuff, 3.0f, 30.0f, 60.0f, true, StatType::Count},
        // Player_Fragile
        MapAffixDefinition{"of Glass", "玻璃之", "玩家受伤加成: +{value}%", MapAffixCategory::Debuff, 3.0f, 10.0f, 25.0f, true, StatType::Count},
        // Player_DodgePenalty
        MapAffixDefinition{"of Unwavering", "迟缓之", "玩家闪避率: -{value}%", MapAffixCategory::Debuff, 3.0f, 15.0f, 30.0f, true, StatType::Count},
        // Env_Firestorm
        MapAffixDefinition{"of Ash", "炼狱之", "环境效果: 灰烬风暴", MapAffixCategory::Environment, 0.0f, 1.0f, 1.0f, true, StatType::Count},
        // Env_Darkness
        MapAffixDefinition{"of Void", "漆黑之", "环境效果: 虚无深渊", MapAffixCategory::Environment, 0.0f, 1.0f, 1.0f, true, StatType::Count},
        // Env_GroundIce
        MapAffixDefinition{"of Frost", "冰洁之", "环境效果: 极寒领域", MapAffixCategory::Environment, 0.0f, 1.0f, 1.0f, true, StatType::Count},
        // Env_LightningStorm
        MapAffixDefinition{"of Fulgur", "雷霆之", "环境效果: 雷鸣风暴", MapAffixCategory::Environment, 0.0f, 1.0f, 1.0f, true, StatType::Count},
    };
};

} // namespace NoMoreDay