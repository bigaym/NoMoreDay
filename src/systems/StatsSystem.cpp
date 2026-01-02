#include "../pch.hpp"
#include "StatsSystem.hpp"
#include "../components/Stats.hpp"
#include "../components/Common.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/EquipmentComponent.hpp" // ADDED THIS LINE
#include "../components/ItemComponent.hpp"
#include "../components/ItemStats.hpp"
#include "../components/Progression.hpp"
#include "../core/AstrolabeRegistry.hpp"
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
namespace NoMoreDay {

struct StatCalculation {
    float base = 0.0f;
    float flat = 0.0f;
    float percent_add = 0.0f;
    float percent_mult = 1.0f;

    float Result() const { // 计算最终结果
        return (base + flat) * (1.0f + percent_add) * percent_mult;
    }
};

static void resetCombatStats(CombatStats& combat) { // 重置战斗属性
    combat.max_health = 100.0f;
    combat.max_mana = 100.0f;
    combat.armor = 0.0f;
    combat.move_speed = 300.0f;
    combat.crit_chance = 0.05f;
    combat.crit_damage = 1.50f;
    combat.attack_speed = 1.0f;
    combat.cast_speed = 1.0f;
    combat.accuracy = 0.97f;
    combat.knockback = 0.0f;
    combat.resistances.fill(0.0f);
    combat.flat_damage.fill(0.0f);
    combat.damage_multipliers.fill(1.0f);
    
    // Reset other stats to ensure no "sticky" values
    combat.damage_reduction = 0.0f;
    combat.thorns = 0.0f;
    combat.life_steal = 0.0f;
    combat.life_on_hit = 0.0f;
    combat.mana_on_hit = 0.0f;
    combat.cooldown_reduction = 0.0f;
    combat.resource_cost_reduction = 0.0f;
    combat.cast_range = 0.0f;
    combat.area_scale = 1.0f;
    combat.projectile_speed = 1.0f;
    combat.duration_scale = 1.0f;
    combat.block_chance = 0.0f;
    combat.block_amount = 0.0f;
    combat.dodge_chance = 0.0f;
    combat.gold_bonus = 0.0f;
    combat.experience_gain_mult = 0.0f;
    combat.pickup_range = 50.0f; // Default

    // Reset regeneration values
    combat.health_regen = 1.0f;
    combat.mana_regen = 1.0f;
    combat.health_regen_pct = 0.0f;
    combat.mana_regen_pct = 0.0f;
}

// 辅助函数：将通用 StatModifier 应用到计算结构
static void ApplyStatCalculation(StatCalculation& c, ModifierMode mode, float value) {
    switch (mode) {
        case ModifierMode::Flat:
            c.flat += value;
            break;
        case ModifierMode::PercentAdd:
            c.percent_add += value / 100.0f;
            break;
        case ModifierMode::PercentMult:
            c.percent_mult *= (1.0f + value / 100.0f);
            break;
    }
}

// 辅助函数：将通用 StatModifier 应用到计算数组
static void ApplyStatModifier(std::array<StatCalculation, static_cast<size_t>(StatType::Count)>& calcs, StatType type, ModifierMode mode, float value) {
    ApplyStatCalculation(calcs[static_cast<size_t>(type)], mode, value);
}

// 辅助函数：将 AffixType 转换为 StatType 并应用它
static void ApplyAffix(std::array<StatCalculation, static_cast<size_t>(StatType::Count)>& calcs, const Affix& affix) {
    switch (affix.type) {
        case AffixType::Strength:
            ApplyStatModifier(calcs, StatType::Strength, ModifierMode::Flat, affix.value);
            break;
        case AffixType::Dexterity:
            ApplyStatModifier(calcs, StatType::Dexterity, ModifierMode::Flat, affix.value);
            break;
        case AffixType::Intelligence:
            ApplyStatModifier(calcs, StatType::Intelligence, ModifierMode::Flat, affix.value);
            break;
        case AffixType::Vitality:
            ApplyStatModifier(calcs, StatType::Vitality, ModifierMode::Flat, affix.value);
            break;
        case AffixType::AllAttributes:
            ApplyStatModifier(calcs, StatType::Strength, ModifierMode::Flat, affix.value);
            ApplyStatModifier(calcs, StatType::Dexterity, ModifierMode::Flat, affix.value);
            ApplyStatModifier(calcs, StatType::Intelligence, ModifierMode::Flat, affix.value);
            ApplyStatModifier(calcs, StatType::Vitality, ModifierMode::Flat, affix.value);
            break;
        case AffixType::FlatHealth:
            ApplyStatModifier(calcs, StatType::MaxHealth, ModifierMode::Flat, affix.value);
            break;
        case AffixType::PercentHealth:
            ApplyStatModifier(calcs, StatType::MaxHealth, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::FlatMana:
            ApplyStatModifier(calcs, StatType::MaxMana, ModifierMode::Flat, affix.value);
            break;
        case AffixType::FlatArmor:
            ApplyStatModifier(calcs, StatType::Armor, ModifierMode::Flat, affix.value);
            break;
        case AffixType::PercentArmor:
            ApplyStatModifier(calcs, StatType::Armor, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::MoveSpeed:
            ApplyStatModifier(calcs, StatType::MoveSpeed, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::CritChance:
            ApplyStatModifier(calcs, StatType::CritChance, ModifierMode::Flat, affix.value);
            break;
        case AffixType::CritDamage:
            ApplyStatModifier(calcs, StatType::CritDamage, ModifierMode::Flat, affix.value);
            break;
        case AffixType::AttackSpeed:
            ApplyStatModifier(calcs, StatType::AttackSpeed, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::CastSpeed:
            ApplyStatModifier(calcs, StatType::CastSpeed, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::Accuracy:
            ApplyStatModifier(calcs, StatType::Accuracy, ModifierMode::Flat, affix.value);
            break;
        case AffixType::PercentPhysicalDamage:
            ApplyStatModifier(calcs, StatType::PhysicalDamage, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::PercentFireDamage:
            ApplyStatModifier(calcs, StatType::FireDamage, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::PercentColdDamage:
            ApplyStatModifier(calcs, StatType::ColdDamage, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::PercentLightningDamage:
            ApplyStatModifier(calcs, StatType::LightningDamage, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::PercentPoisonDamage:
            ApplyStatModifier(calcs, StatType::PoisonDamage, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::PercentShadowDamage:
            ApplyStatModifier(calcs, StatType::ShadowDamage, ModifierMode::PercentAdd, affix.value);
            break;
        case AffixType::ResistFire:
            ApplyStatModifier(calcs, StatType::ResistFire, ModifierMode::Flat, affix.value);
            break;
        case AffixType::ResistCold:
            ApplyStatModifier(calcs, StatType::ResistCold, ModifierMode::Flat, affix.value);
            break;
        case AffixType::ResistLightning:
            ApplyStatModifier(calcs, StatType::ResistLightning, ModifierMode::Flat, affix.value);
            break;
        case AffixType::ResistPoison:
            ApplyStatModifier(calcs, StatType::ResistPoison, ModifierMode::Flat, affix.value);
            break;
        case AffixType::ResistShadow:
            ApplyStatModifier(calcs, StatType::ResistShadow, ModifierMode::Flat, affix.value);
            break;
        case AffixType::ResistAll:
            ApplyStatModifier(calcs, StatType::ResistAll, ModifierMode::Flat, affix.value);
            break;
        default:
            break;
    }
}

void StatsSystem::Recalculate(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<CombatStats>(entity)) return;

    auto& combat = registry.get<CombatStats>(entity);
    resetCombatStats(combat); 
    
    // Prepare GlobalModifierComponent for DamagePipeline
    auto& global_mods = registry.get_or_emplace<GlobalModifierComponent>(entity);
    global_mods.modifiers.clear();

    std::array<StatCalculation, static_cast<size_t>(StatType::Count)> calcs; // 初始化计算数组
    
    // 使用默认值初始化
    calcs[static_cast<size_t>(StatType::MaxHealth)].base = 100.0f;
    calcs[static_cast<size_t>(StatType::MaxMana)].base = 100.0f;
    calcs[static_cast<size_t>(StatType::MoveSpeed)].base = 300.0f;
    calcs[static_cast<size_t>(StatType::Armor)].base = 0.0f;
    
    calcs[static_cast<size_t>(StatType::CritChance)].base = 5.0f; // 5%
    calcs[static_cast<size_t>(StatType::CritDamage)].base = 150.0f; // 150%
    calcs[static_cast<size_t>(StatType::AttackSpeed)].base = 100.0f; // 100%
    calcs[static_cast<size_t>(StatType::CastSpeed)].base = 100.0f;
    calcs[static_cast<size_t>(StatType::Accuracy)].base = 97.0f; // 97%

    // 伤害乘数默认为 100% (1.0)
    for (int i = 0; i < 6; ++i) {
        calcs[static_cast<size_t>(StatType::PhysicalDamage) + i].base = 100.0f;
    }

    // 0. 来自 PrimaryStats 组件的基础属性
    if (registry.all_of<PrimaryStats>(entity)) {
        const auto& primary = registry.get<PrimaryStats>(entity);
        calcs[static_cast<size_t>(StatType::Strength)].base = primary.strength;
        calcs[static_cast<size_t>(StatType::Dexterity)].base = primary.dexterity;
        calcs[static_cast<size_t>(StatType::Intelligence)].base = primary.intelligence;
        calcs[static_cast<size_t>(StatType::Vitality)].base = primary.vitality;
    }

    float mainHandAttack = 0.0f;
    bool hasMainHandWeapon = false;
    bool isTwoHanded = false;
    float offHandAttack = 0.0f;
    bool hasOffHandWeapon = false;
    bool offHandIsEmpty = true;

    // 定义处理词缀的 Lambda，供物品 and 套装奖励复用
    auto processAffixes = [&](const std::vector<Affix>& affixes) {
        for (const auto& affix : affixes) {
            ApplyAffix(calcs, affix);
            
            // 处理 ApplyAffix 中未涵盖的特殊词缀
            switch (affix.type) {
                // 基础点伤 (Flat Damage) - 目前 StatType 不处理点伤基数，保持原样
                case AffixType::FlatPhysicalDamage: combat.flat_damage[(int)DamageType::Physical] += affix.value; break;
                case AffixType::FlatFireDamage:     combat.flat_damage[(int)DamageType::Fire] += affix.value; break;
                case AffixType::FlatColdDamage:     combat.flat_damage[(int)DamageType::Cold] += affix.value; break;
                case AffixType::FlatLightningDamage:combat.flat_damage[(int)DamageType::Lightning] += affix.value; break;
                case AffixType::FlatPoisonDamage:   combat.flat_damage[(int)DamageType::Poison] += affix.value; break;
                case AffixType::FlatShadowDamage:   combat.flat_damage[(int)DamageType::Shadow] += affix.value; break;

                // 回复 (Recovery)
                case AffixType::LifeSteal:       combat.life_steal += affix.value / 100.0f; break;
                case AffixType::LifeOnHit:       combat.life_on_hit += affix.value; break;
                case AffixType::ManaOnHit:       combat.mana_on_hit += affix.value; break;
                case AffixType::HealthRegen:     combat.health_regen += affix.value; break;
                case AffixType::ManaRegen:       combat.mana_regen += affix.value; break;
                case AffixType::PercentHealthRegen: combat.health_regen_pct += affix.value / 100.0f; break;
                case AffixType::PercentManaRegen:   combat.mana_regen_pct += affix.value / 100.0f; break;

                // 其他特殊词缀
                case AffixType::Thorns:          combat.thorns += affix.value; break;
                case AffixType::DamageReduction: combat.damage_reduction += affix.value / 100.0f; break;
                case AffixType::CooldownReduction: combat.cooldown_reduction += affix.value / 100.0f; break;

                default: break;
            }
        }
    };

    std::unordered_map<std::string, int> setCounts;
    std::unordered_map<std::string, const std::vector<SetBonus>*> setDefinitions;

    // 1. 处理装备
    if (registry.all_of<EquipmentComponent>(entity)) {
        const auto& equipment = registry.get<EquipmentComponent>(entity);
        
        // Track offhand status specifically
        offHandIsEmpty = !registry.valid(equipment.slots[(int)EquipmentSlot::OffHand]);

        for (const auto& itemEntity : equipment.slots) {
            if (registry.valid(itemEntity) && registry.all_of<ItemComponent>(itemEntity)) {
                const auto& item = registry.get<ItemComponent>(itemEntity);

                // 提取武器基础伤害
                if (item.type == ItemType::Weapon && item.attack > 0) {
                    if (item.slot == EquipmentSlot::MainHand) {
                        mainHandAttack = item.attack;
                        hasMainHandWeapon = true;
                        if (item.isTwoHanded) isTwoHanded = true;
                    } else if (item.slot == EquipmentSlot::OffHand) {
                        offHandAttack = item.attack;
                        hasOffHandWeapon = true;
                    }
                }

                processAffixes(item.implicits);
                processAffixes(item.affixes);

                // 处理插槽中的符文
                for (auto runeEntity : item.sockets) {
                    if (registry.valid(runeEntity) && registry.all_of<RuneComponent>(runeEntity)) {
                        const auto& rune = registry.get<RuneComponent>(runeEntity);
                        // 根据物品槽位选择效果
                        if (item.type == ItemType::Weapon) {
                            processAffixes(rune.weaponEffects);
                        } else if (item.slot == EquipmentSlot::Neck || item.slot == EquipmentSlot::Ring1 || item.slot == EquipmentSlot::Ring2) {
                            processAffixes(rune.jewelryEffects);
                        } else {
                            processAffixes(rune.armorEffects);
                        }
                    }
                }

                if (item.defense > 0) {
                    ApplyStatModifier(calcs, StatType::Armor, ModifierMode::Flat, item.defense);
                }

                // 盾牌逻辑：增加格挡几率和格挡值
                if (item.type == ItemType::Shield) {
                    combat.block_chance += 0.20f; // 基础 20% 格挡几率
                    combat.block_amount += item.defense; // 使用防御值作为格挡减免值
                }

                // 统计套装
                if (item.rarity == Rarity::Set && !item.setName.empty()) {
                    setCounts[item.setName]++;
                    // 缓存套装定义 (假设同名套装的定义是一致的，取第一个遇到的即可)
                    if (setDefinitions.find(item.setName) == setDefinitions.end()) {
                        setDefinitions[item.setName] = &item.setBonuses;
                    }
                }
            }
        }
    }

    // 应用套装奖励
    for (const auto& [setName, count] : setCounts) {
        const auto* bonuses = setDefinitions[setName];
        if (bonuses) {
            for (const auto& sb : *bonuses) {
                if (count >= sb.requiredCount) {
                    processAffixes(sb.bonuses);
                }
            }
        }
    }

    // 计算最终武器伤害
    if (hasMainHandWeapon) {
        if (hasOffHandWeapon) {
            // 双持 (Dual Wielding)
            // 逻辑：平均伤害 + 15% 攻速奖励
            float avgAttack = (mainHandAttack + offHandAttack) * 0.5f;
            combat.min_weapon_damage = avgAttack * 0.9f;
            combat.max_weapon_damage = avgAttack * 1.1f;
            ApplyStatModifier(calcs, StatType::AttackSpeed, ModifierMode::PercentAdd, 15.0f);
        } else {
            // 单持主手
            combat.min_weapon_damage = mainHandAttack * 0.9f;
            combat.max_weapon_damage = mainHandAttack * 1.1f;

            // 双手武器奖励：额外 25% 基础伤害
            if (isTwoHanded) {
                combat.min_weapon_damage *= 1.25f;
                combat.max_weapon_damage *= 1.25f;
            }
        }
    } else {
        // 处理空手情况：如果未装备主手武器，使用 WeaponComponent 的默认值
        // 如果是玩家（有装备栏），给予合理的空手伤害
        if (registry.all_of<EquipmentComponent>(entity)) {
            combat.min_weapon_damage = 2.0f;
            combat.max_weapon_damage = 3.0f;
            combat.knockback = 10.0f; // 空手击退
        } else if (registry.all_of<WeaponComponent>(entity)) {
            // 怪物回退逻辑
            const auto& wc = registry.get<WeaponComponent>(entity);
            combat.min_weapon_damage = wc.damage;
            combat.max_weapon_damage = wc.damage;
        }
    }
    
    // 如果有武器但没设置击退（ItemComponent目前没有击退字段），给个默认值
    // 未来可以在 ItemComponent 中添加 knockback
    if (hasMainHandWeapon && combat.knockback < 0.1f) combat.knockback = 20.0f;

    // 2. 累积通用修饰符
    if (registry.all_of<ModifierList>(entity)) {
        const auto& list = registry.get<ModifierList>(entity);
        for (const auto& mod : list.modifiers) {
            if (mod.required_tags == Tag::None) {
                ApplyStatModifier(calcs, mod.type, mod.mode, mod.value);
            }
        }
    }

    // 2.5 处理星盘天赋 (Astrolabe Nodes)
    if (registry.all_of<AstrolabeComponent>(entity)) {
        const auto& astrolabe = registry.get<AstrolabeComponent>(entity);
        const auto& registry_instance = AstrolabeRegistry::Get();
        for (uint32_t node_id : astrolabe.activated_nodes) {
            const auto* node = registry_instance.GetNode(node_id);
            if (node) {
                for (const auto& mod : node->modifiers) {
                    if (mod.required_tags == Tag::None) {
                        ApplyStatModifier(calcs, mod.type, mod.mode, mod.value);
                    }
                }
                // Bake DamageModifiers for Pipeline
                for (const auto& dmod : node->damage_modifiers) {
                    global_mods.modifiers.push_back(dmod);
                }
            }
        }
    }

    // 3. 解析主要属性
    float str = calcs[static_cast<size_t>(StatType::Strength)].Result();
    float dex = calcs[static_cast<size_t>(StatType::Dexterity)].Result();
    float vit = calcs[static_cast<size_t>(StatType::Vitality)].Result();
    float intel = calcs[static_cast<size_t>(StatType::Intelligence)].Result();

    // 存储有效属性供 UI 显示
    combat.effective_strength = str;
    combat.effective_dexterity = dex;
    combat.effective_intelligence = intel;
    combat.effective_vitality = vit;

    // 4. 应用主要属性的缩放
    calcs[static_cast<size_t>(StatType::Armor)].base += str * 2.0f; // 1力 = 2护甲
    calcs[static_cast<size_t>(StatType::MaxHealth)].base += vit * 15.0f; // 1体 = 15血
    calcs[static_cast<size_t>(StatType::MaxMana)].base += intel * 5.0f;
    
    // 回复缩放 (Regeneration Scaling)
    combat.health_regen += vit * 0.2f;   // 1体 = 0.2 生命回复/秒
    combat.mana_regen += intel * 0.2f;  // 1智 = 0.2 法力回复/秒

    // 属性对伤害的加成
    ApplyStatModifier(calcs, StatType::PhysicalDamage, ModifierMode::PercentAdd, str * 1.0f); // 1力 = 1% 物理伤害
    
    // 敏捷加成
    ApplyStatModifier(calcs, StatType::CritChance, ModifierMode::Flat, dex * 0.2f); // 1敏 = 0.2% 暴击率
    ApplyStatModifier(calcs, StatType::Accuracy, ModifierMode::Flat, dex * 0.1f); // 1敏 = 0.1% 命中

    // --- Sword Heart Mechanic ---
    if (registry.all_of<SwordHeartComponent>(entity)) {
        // Condition: Main Hand is Weapon AND Off Hand is Empty
        // TODO: In the future, check if Main Hand is specifically a "Sword" tag
        bool isSwordHeartActive = hasMainHandWeapon && offHandIsEmpty;
        
        if (isSwordHeartActive) {
            // 1. 50% More Weapon Damage
            combat.min_weapon_damage *= 1.5f;
            combat.max_weapon_damage *= 1.5f;
            
            // 2. Base Block Chance (Shield Equivalent ~20%)
            combat.block_chance += 0.20f;
            
            // 3. Spell Damage Bonus (50% of Attack Damage Bonus)
            // Get the physical damage % increase (which represents "Attack Damage" for Blade Ascendant)
            float phys_inc = calcs[static_cast<size_t>(StatType::PhysicalDamage)].percent_add;
            float spell_bonus = phys_inc * 0.5f;
            
            // Apply to all elemental/shadow/poison types (Spell types)
            for (int i = 1; i < 6; ++i) {
                calcs[static_cast<size_t>(StatType::PhysicalDamage) + i].percent_add += spell_bonus;
            }
            
            LOG_DEBUG("Sword Heart active: +50% Weapon Dmg, +20% Block, +{:.1f}% Spell Dmg", spell_bonus * 100.0f);
        }
    }

    // 5. 最终确定次要属性
    combat.max_health = calcs[static_cast<size_t>(StatType::MaxHealth)].Result();
    combat.max_mana = calcs[static_cast<size_t>(StatType::MaxMana)].Result();
    combat.armor = calcs[static_cast<size_t>(StatType::Armor)].Result();
    combat.move_speed = calcs[static_cast<size_t>(StatType::MoveSpeed)].Result();
    
    combat.crit_chance = calcs[static_cast<size_t>(StatType::CritChance)].Result() / 100.0f;
    combat.crit_damage = calcs[static_cast<size_t>(StatType::CritDamage)].Result() / 100.0f;
    combat.attack_speed = calcs[static_cast<size_t>(StatType::AttackSpeed)].Result() / 100.0f;
    combat.cast_speed = calcs[static_cast<size_t>(StatType::CastSpeed)].Result() / 100.0f;
    combat.accuracy = calcs[static_cast<size_t>(StatType::Accuracy)].Result() / 100.0f;
    combat.mana_on_hit = calcs[static_cast<size_t>(StatType::ManaOnHit)].Result();

    // 伤害乘数
    for (int i = 0; i < 6; ++i) {
        combat.damage_multipliers[i] = calcs[static_cast<size_t>(StatType::PhysicalDamage) + i].Result() / 100.0f;
    }

    // 抗性
    float resAll = calcs[static_cast<size_t>(StatType::ResistAll)].Result();
    for (int i = 0; i < 6; ++i) {
        combat.resistances[i] = (calcs[static_cast<size_t>(StatType::ResistPhysical) + i].Result() + resAll) / 100.0f;
    }

    combat.knockback += str * 0.5f; // 力量增加击退

    // Finalize Regeneration (Apply Pct)
    combat.health_regen *= (1.0f + combat.health_regen_pct);
    combat.mana_regen *= (1.0f + combat.mana_regen_pct);

    LOG_INFO("StatsSystem: Recalculated for entity {}. Dmg: {:.1f}-{:.1f}, Str: {:.1f}, HP: {:.1f}", 
        (uint32_t)entity, combat.min_weapon_damage, combat.max_weapon_damage, 
        str, combat.max_health);
}

float StatsSystem::GetStatWithTags(entt::registry& registry, entt::entity entity, StatType type, Tag tags) {
    auto* combat = registry.try_get<CombatStats>(entity);
    if (!combat) return 0.0f;

    StatCalculation dynamic_calc;
    
    // 1. 获取 CombatStats 中已烘焙的基础值或百分比值
    switch (type) {
        case StatType::PhysicalDamage: dynamic_calc.base = 100.0f; dynamic_calc.percent_mult = combat->damage_multipliers[0]; break;
        case StatType::FireDamage:     dynamic_calc.base = 100.0f; dynamic_calc.percent_mult = combat->damage_multipliers[1]; break;
        case StatType::ColdDamage:     dynamic_calc.base = 100.0f; dynamic_calc.percent_mult = combat->damage_multipliers[2]; break;
        case StatType::LightningDamage: dynamic_calc.base = 100.0f; dynamic_calc.percent_mult = combat->damage_multipliers[3]; break;
        case StatType::PoisonDamage:    dynamic_calc.base = 100.0f; dynamic_calc.percent_mult = combat->damage_multipliers[4]; break;
        case StatType::ShadowDamage:    dynamic_calc.base = 100.0f; dynamic_calc.percent_mult = combat->damage_multipliers[5]; break;
        
        case StatType::CritChance:    dynamic_calc.base = combat->crit_chance * 100.0f; break;
        case StatType::CritDamage:    dynamic_calc.base = combat->crit_damage * 100.0f; break;
        case StatType::AttackSpeed:   dynamic_calc.base = combat->attack_speed * 100.0f; break;
        case StatType::CastSpeed:     dynamic_calc.base = combat->cast_speed * 100.0f; break;
        case StatType::Accuracy:      dynamic_calc.base = combat->accuracy * 100.0f; break;
        case StatType::ManaOnHit:     dynamic_calc.base = combat->mana_on_hit; break;
        case StatType::MoveSpeed:     dynamic_calc.base = combat->move_speed; break;
        case StatType::Armor:         dynamic_calc.base = combat->armor; break;
        case StatType::MaxHealth:     dynamic_calc.base = combat->max_health; break;
        case StatType::MaxMana:       dynamic_calc.base = combat->max_mana; break;

        default: break;
    }

    // 2. 累加动态标签修饰符
    auto apply_if_tags_match = [&](const std::vector<StatModifier>& modifiers) {
        for (const auto& mod : modifiers) {
            if (mod.type == type && (mod.required_tags == Tag::None || HasTag(tags, mod.required_tags))) {
                ApplyStatCalculation(dynamic_calc, mod.mode, mod.value);
            }
        }
    };

    if (auto* list = registry.try_get<ModifierList>(entity)) {
        apply_if_tags_match(list->modifiers);
    }

    if (auto* astrolabe = registry.try_get<AstrolabeComponent>(entity)) {
        const auto& reg = AstrolabeRegistry::Get();
        for (uint32_t node_id : astrolabe->activated_nodes) {
            if (const auto* node = reg.GetNode(node_id)) {
                apply_if_tags_match(node->modifiers);
            }
        }
    }

    return dynamic_calc.Result();
}

void StatsSystem::update(entt::registry& registry) {
    auto view = registry.view<StatsDirty>();
    for (auto entity : view) {
        Recalculate(registry, entity);
    }
    registry.clear<StatsDirty>();
}

} // namespace NoMoreDay