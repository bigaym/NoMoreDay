#include "game/systems/combat/StatsSystem.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/ItemStats.hpp"
#include "game/components/Progression.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/Buff.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/SkillRegistry.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "core/utils/FrameRateUtils.hpp"  // Frame-rate independent utilities
#include <algorithm>
#include <vector>
#include <Taskflow/taskflow.hpp>
#include <Taskflow/algorithm/for_each.hpp>
#include <array>
#include <unordered_map>
namespace NoMoreDay {

// Scratch space for Set Bonus tracking to avoid per-call allocations
struct SetTrack {
    uint32_t hash;
    int count;
    const std::vector<SetBonus>* definitions;
};
static thread_local std::vector<SetTrack> s_set_scratch;

// -----------------------------------------------------------------------------
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
    using namespace NoMoreDay::Constants::Combat;
    combat.max_health = DEFAULT_MAX_HEALTH;
    combat.max_mana = DEFAULT_MAX_MANA;
    combat.armor = 0.0f;
    combat.move_speed = DEFAULT_MOVE_SPEED;
    combat.crit_chance = DEFAULT_CRIT_CHANCE;
    combat.crit_damage = DEFAULT_CRIT_DAMAGE;
    combat.attack_speed = DEFAULT_ATTACK_SPEED;
    combat.cast_speed = 1.0f;
    combat.accuracy = DEFAULT_ACCURACY;
    combat.knockback = 0.0f;
    combat.resistances.fill(0.0f);
    combat.flat_damage.fill(0.0f);
    combat.damage_multipliers.fill(1.0f);
    combat.damage_percent_add.fill(0.0f);
    combat.damage_percent_mult_component.fill(1.0f);
    
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
    combat. gold_bonus = 0.0f;
    combat.experience_gain_mult = 0.0f;
    using namespace NoMoreDay::Constants::Combat;
    combat. pickup_range = BASE_PICKUP_RANGE; // Default

    combat.raw_resistances.fill(0.0f);
    combat.raw_move_speed = 0.0f;
    combat.raw_cooldown_reduction = 0.0f;
    combat.raw_attack_speed = 0.0f;
    combat.raw_dodge_chance = 0.0f;
    combat.raw_block_chance = 0.0f;

    // Reset regeneration values
    combat.health_regen = 1.0f;
    combat.mana_regen = 1.0f;
    combat.health_regen_pct = 0.0f;
    combat.mana_regen_pct = 0.0f;

    // Cache is now managed by StatsSystem, cleared via ClearCache()
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
            LOG_DEBUG("StatsSystem: Applied Attack Speed Affix +{:.1f}%", affix.value);
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

// Helper: Check if StatType is a damage scaling stat
static bool IsDamageStat(StatType type) {
    return type >= StatType::PhysicalDamage && type <= StatType::ShadowDamage;
}

// Helper: Get Tag for Damage Stat
static Tag GetTagFromDamageStat(StatType type) {
    switch(type) {
        case StatType::PhysicalDamage: return Tag::Physical;
        case StatType::FireDamage: return Tag::Fire;
        case StatType::ColdDamage: return Tag::Cold;
        case StatType::LightningDamage: return Tag::Lightning;
        case StatType::PoisonDamage: return Tag::Poison;
        case StatType::ShadowDamage: return Tag::Shadow;
        default: return Tag::None;
    }
}

void StatsSystem::Recalculate(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<CombatStats>(entity)) return;

    auto& combat = registry.get<CombatStats>(entity);
    resetCombatStats(combat);
    ClearCache(registry, entity);  // Clear the stat cache for this entity 

    // Reset Traits
    if (registry.all_of<TitanGripTrait>(entity)) registry.remove<TitanGripTrait>(entity);
    bool hasTitanGrip = false;
    
    // 0.5 Reset Skill Bonus Levels
    if (auto* active = registry.try_get<ActiveSkillsComponent>(entity)) {
        for (auto& specialized : active->specialized_slots) {
            specialized.bonus_levels = 0;
        }
    }

    // Prepare GlobalModifierComponent for DamagePipeline
    auto& global_mods = registry.get_or_emplace<GlobalModifierComponent>(entity);
    global_mods.modifiers.clear();
    global_mods.stat_modifiers.clear();  // NEW: Clear conditional stat modifiers

    std::array<StatCalculation, static_cast<size_t>(StatType::Count)> calcs; // 初始化计算数组
    
    // Determine base values (Players use test defaults, others use sane minimums)
    using namespace NoMoreDay::Constants::Combat;
    bool isPlayer = registry.all_of<PlayerTag>(entity);
    float base_hp = isPlayer ? DEFAULT_MAX_HEALTH : 1.0f;
    float base_mana = isPlayer ? DEFAULT_MAX_MANA : 1.0f;

    using namespace NoMoreDay::Constants::Combat;
    calcs[static_cast<size_t>(StatType::MaxHealth)].base = base_hp;
    calcs[static_cast<size_t>(StatType::MaxMana)].base = base_mana;
    calcs[static_cast<size_t>(StatType::MoveSpeed)].base = DEFAULT_MOVE_SPEED;
    calcs[static_cast<size_t>(StatType::Armor)].base = 0.0f;
    
    // 如果是敌人，应用敌人种族基础属性覆盖 base_hp
    if (auto* enemy = registry.try_get<EnemyStateComponent>(entity)) {
        // 使用静态查找表，避免每次分配 vector<string>
        const auto& raceData = kRaceData[static_cast<size_t>(enemy->raceType)];
        
        calcs[static_cast<size_t>(StatType::MaxHealth)].base = raceData.baseHP;
        calcs[static_cast<size_t>(StatType::MoveSpeed)].base = raceData.baseSpeed;
        calcs[static_cast<size_t>(StatType::Armor)].base = raceData.baseArmor;

        // 应用种族抗性 (Tag -> StatType)
        constexpr float NATIVE_RESISTANCE_VALUE = 50.0f; // 天生抗性数值 (50%)
        
        // 我们遍历主要的伤害类型标签来映射抗性
        // 注意：Tag::Void 目前没有对应的 StatType::ResistVoid，暂不处理
        if (HasTag(raceData.resistances, Tag::Physical)) 
            ApplyStatModifier(calcs, StatType::ResistPhysical, ModifierMode::Flat, NATIVE_RESISTANCE_VALUE);
        if (HasTag(raceData.resistances, Tag::Fire)) 
            ApplyStatModifier(calcs, StatType::ResistFire, ModifierMode::Flat, NATIVE_RESISTANCE_VALUE);
        if (HasTag(raceData.resistances, Tag::Cold)) 
            ApplyStatModifier(calcs, StatType::ResistCold, ModifierMode::Flat, NATIVE_RESISTANCE_VALUE);
        if (HasTag(raceData.resistances, Tag::Lightning)) 
            ApplyStatModifier(calcs, StatType::ResistLightning, ModifierMode::Flat, NATIVE_RESISTANCE_VALUE);
        if (HasTag(raceData.resistances, Tag::Poison)) 
            ApplyStatModifier(calcs, StatType::ResistPoison, ModifierMode::Flat, NATIVE_RESISTANCE_VALUE);
        if (HasTag(raceData.resistances, Tag::Shadow)) 
            ApplyStatModifier(calcs, StatType::ResistShadow, ModifierMode::Flat, NATIVE_RESISTANCE_VALUE);
    }

    using namespace NoMoreDay::Constants::Combat;
    calcs[static_cast<size_t>(StatType::CritChance)].base = DEFAULT_CRIT_CHANCE * 100.0f; 
    calcs[static_cast<size_t>(StatType::CritDamage)].base = DEFAULT_CRIT_DAMAGE * 100.0f; 
    calcs[static_cast<size_t>(StatType::AttackSpeed)].base = DEFAULT_ATTACK_SPEED * 100.0f; 
    calcs[static_cast<size_t>(StatType::CastSpeed)].base = 100.0f;
    calcs[static_cast<size_t>(StatType::Accuracy)].base = DEFAULT_ACCURACY * 100.0f; 
    
    calcs[static_cast<size_t>(StatType::ProjectileSpeed)].base = 100.0f;
    calcs[static_cast<size_t>(StatType::DurationScale)].base = 100.0f;
    calcs[static_cast<size_t>(StatType::AreaScale)].base = 100.0f;
    
    calcs[static_cast<size_t>(StatType::DodgeChance)].base = 0.0f;
    calcs[static_cast<size_t>(StatType::BlockChance)].base = 0.0f;
    calcs[static_cast<size_t>(StatType::LifeSteal)].base = 0.0f;
    calcs[static_cast<size_t>(StatType::LifeOnHit)].base = 0.0f;
    calcs[static_cast<size_t>(StatType::HealthRegen)].base = 1.0f;
    calcs[static_cast<size_t>(StatType::ManaRegen)].base = REGEN_BASE;
    calcs[static_cast<size_t>(StatType::Thorns)].base = 0.0f;
    calcs[static_cast<size_t>(StatType::MagicFind)].base = MAGIC_FIND_BASE;

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

    // Determine combined base tags for the entity
    Tag entity_base_tags = Tag::None;
    if (auto* stanceComp = registry.try_get<MovementStanceComponent>(entity)) {
        if (stanceComp->stance == MovementStance::SwordRiding) {
            entity_base_tags = entity_base_tags | Tag::SwordRiding;
            LOG_DEBUG("StatsSystem: Recalculating with SwordRiding tag for entity {}", (uint32_t)entity);
        }
    }

    // --- Stat Conversions (General Framework) ---
    auto apply_conversions = [&](const std::vector<StatConversion>& convs) {
        for (const auto& conv : convs) {
            if (conv.source < StatType::Count && conv.target < StatType::Count) {
                if (conv.required_tags == Tag::None || HasTag(entity_base_tags, conv.required_tags)) {
                    float sourceVal = calcs[static_cast<size_t>(conv.source)].Result();
                    ApplyStatModifier(calcs, conv.target, ModifierMode::Flat, sourceVal * conv.ratio);
                }
            }
        }
    };

    // 定义处理词缀的 Lambda，供物品 and 套装奖励复用
    auto processAffixes = [&](const std::vector<Affix>& affixes) {
        for (const auto& affix : affixes) {
            // NEW: Check if affix has tag conditions
            if (affix.required_tags != Tag::None) {
                // Store for dynamic resolution in GetStatWithTags
                StatModifier mod;
                mod.type = static_cast<StatType>(affix.type);  // Direct cast works for most common types
                mod.mode = ModifierMode::PercentAdd;  // Default, may need refinement
                mod.value = affix.value;
                mod.required_tags = affix.required_tags;
                mod.source = ModifierSource::Item;
                global_mods.stat_modifiers.push_back(mod);
                continue;  // Skip normal processing for conditional affixes
            }
            
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

                // Skill Levels
                case AffixType::PlusAllSkills: {
                    if (auto* active = registry.try_get<ActiveSkillsComponent>(entity)) {
                        for (auto& specialized : active->specialized_slots) {
                            if (specialized.skill_id != 0) specialized.bonus_levels += (int)affix.value;
                        }
                    }
                    break;
                }
                case AffixType::PlusFlowingThrust: {
                    if (auto* active = registry.try_get<ActiveSkillsComponent>(entity)) {
                        for (auto& specialized : active->specialized_slots) {
                            if (specialized.skill_id == 1) specialized.bonus_levels += (int)affix.value;
                        }
                    }
                    break;
                }
                case AffixType::PlusRendingWave: {
                    if (auto* active = registry.try_get<ActiveSkillsComponent>(entity)) {
                        for (auto& specialized : active->specialized_slots) {
                            if (specialized.skill_id == 2) specialized.bonus_levels += (int)affix.value;
                        }
                    }
                    break;
                }

                case AffixType::TitanGrip: {
                    hasTitanGrip = true;
                    break;
                }

                default: break;
            }
        }
    };

    s_set_scratch.clear();

    // 1.1 Equipment Stats
    if (registry.all_of<EquipmentComponent>(entity)) {
        const auto& equipment = registry.get<EquipmentComponent>(entity);
        
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

                // NEW: Process Legendary/Special Conversions from Item
                for (const auto& conv : item.conversions) {
                    apply_conversions({conv});
                }

                // NEW: Process Damage Modifiers from Item
                for (const auto& dmod : item.damage_modifiers) {
                    global_mods.modifiers.push_back(dmod);
                }

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

                // 统计套装
                if (item.rarity == Rarity::Set && item.setNameHash != 0) {
                    bool found = false;
                    for (auto& track : s_set_scratch) {
                        if (track.hash == item.setNameHash) {
                            track.count++;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        s_set_scratch.push_back({item.setNameHash, 1, &item.setBonuses});
                    }
                }
            }
        }
    }

    // 应用套装奖励
    for (const auto& track : s_set_scratch) {
        if (track.definitions) {
            for (const auto& sb : *track.definitions) {
                if (track.count >= sb.requiredCount) {
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
            using namespace NoMoreDay::Constants::Combat;
            float avgAttack = (mainHandAttack + offHandAttack) * 0.5f;
            combat.min_weapon_damage = avgAttack * 0.9f;
            combat.max_weapon_damage = avgAttack * 1.1f;
            ApplyStatModifier(calcs, StatType::AttackSpeed, ModifierMode::PercentAdd, System::DUAL_WIELD_AS_BONUS);
        } else {
            // 单持主手
            combat.min_weapon_damage = mainHandAttack * 0.9f;
            combat.max_weapon_damage = mainHandAttack * 1.1f;

            // 双手武器奖励：额外 25% 基础伤害
            using namespace NoMoreDay::Constants::Combat;
            if (isTwoHanded) {
                combat.min_weapon_damage *= System::TWO_HANDED_DMG_BONUS;
                combat.max_weapon_damage *= System::TWO_HANDED_DMG_BONUS;
            }
        }
    } else {
        // 处理空手情况：如果未装备主手武器，使用 WeaponComponent 的默认值
        // 如果是玩家（有装备栏），给予合理的空手伤害
        if (registry.all_of<EquipmentComponent>(entity)) {
            using namespace NoMoreDay::Constants::Combat::System; // Wait, I should add these to System if needed
            // Actually I didn't add UNARMED specifically, but I can add them or use defaults
            combat.min_weapon_damage = 2.0f; // UNARMED_DAMAGE_MIN
            combat.max_weapon_damage = 3.0f; // UNARMED_DAMAGE_MAX
            combat.knockback = 10.0f; // UNARMED_KNOCKBACK
        } else if (registry.all_of<WeaponComponent>(entity)) {
            // 怪物回退逻辑
            const auto& wc = registry.get<WeaponComponent>(entity);
            combat.min_weapon_damage = wc.damage;
            combat.max_weapon_damage = wc.damage;
        }
    }
    
    // 如果有武器但没设置击退（ItemComponent目前没有击退字段），给个默认值
    using namespace NoMoreDay::Constants::Combat;
    if (hasMainHandWeapon && combat.knockback < 0.1f) combat.knockback = 20.0f; // DEFAULT_WEAPON_KNOCKBACK

    // 2. 累积通用修饰符
    if (registry.all_of<ModifierList>(entity)) {
        const auto& list = registry.get<ModifierList>(entity);
        for (const auto& mod : list.modifiers) {
            if (mod.required_tags == Tag::None || HasTag(entity_base_tags, mod.required_tags)) {
                ApplyStatModifier(calcs, mod.type, mod.mode, mod.value);
            }
        }
    }

    // 2.1 处理活跃 Buff (Active Buffs)
    if (registry.all_of<ActiveEffectsComponent>(entity)) {
        const auto& effects = registry.get<ActiveEffectsComponent>(entity);
        for (const auto& effect : effects.effects) {
            for (const auto& mod : effect.modifiers) {
                if (mod.required_tags == Tag::None || HasTag(entity_base_tags, mod.required_tags)) {
                    // Buffs multiply their effect by stack count
                    ApplyStatModifier(calcs, mod.type, mod.mode, mod.value * effect.stacks);
                }
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
                    if (mod.required_tags == Tag::None || HasTag(entity_base_tags, mod.required_tags)) {
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

    // 2.6 处理技能专精天赋 (Skill Specialization Talents)
    if (registry.all_of<ActiveSkillsComponent>(entity)) {
        const auto& active = registry.get<ActiveSkillsComponent>(entity);
        const auto& skill_registry = SkillRegistry::Get();
        for (const auto& specialized : active.specialized_slots) {
            if (specialized.skill_id == 0) continue;
            const auto* tree = skill_registry.GetSkillTree(specialized.skill_id);
            if (!tree) continue;

            for (auto [node_id, pts] : specialized.allocated_points) {
                auto node_it = tree->nodes.find(node_id);
                if (node_it != tree->nodes.end()) {
                    const auto& node = node_it->second;
                    for (const auto& mod : node.stat_modifiers) {
                        if (mod.required_tags == Tag::None) {
                            // Skill-specific talent modifiers should NOT be baked globally.
                            // They are applied dynamically via GetStatWithTags(skill_id).
                            // ApplyStatModifier(calcs, mod.type, mod.mode, mod.value * pts);
                        }
                    }
                    // Global DamageModifiers from skill talents
                    for (const auto& dmod : node.damage_modifiers) {
                        global_mods.modifiers.push_back(dmod);
                    }
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
    using namespace NoMoreDay::Constants::Combat;
    calcs[static_cast<size_t>(StatType::Armor)].base += str * Attribute::STR_TO_ARMOR;
    calcs[static_cast<size_t>(StatType::MaxHealth)].base += vit * Attribute::VIT_TO_HEALTH;
    calcs[static_cast<size_t>(StatType::MaxMana)].base += intel * Attribute::INT_TO_MANA;
    
    // 回复缩放 (Regeneration Scaling)
    combat.health_regen += vit * Attribute::VIT_TO_HEALTH_REGEN;
    combat.mana_regen += intel * Attribute::INT_TO_MANA_REGEN;
 
    // 属性对伤害的加成
    ApplyStatModifier(calcs, StatType::PhysicalDamage, ModifierMode::PercentAdd, str * Attribute::STR_TO_PHYS_DAMAGE_INC);
    
    // 敏捷加成
    ApplyStatModifier(calcs, StatType::CritChance, ModifierMode::Flat, dex * Attribute::DEX_TO_CRIT_CHANCE);
    ApplyStatModifier(calcs, StatType::Accuracy, ModifierMode::Flat, dex * Attribute::DEX_TO_ACCURACY);

    // --- Sword Heart Mechanic ---
    if (registry.all_of<SwordHeartComponent>(entity)) {
        // Condition: Main Hand is Weapon AND Off Hand is Empty AND NOT Two-Handed
        bool isSwordHeartActive = hasMainHandWeapon && offHandIsEmpty && !isTwoHanded;
        
        if (isSwordHeartActive) {
            using namespace NoMoreDay::Constants::Combat::System;
            // 1. 15% More Weapon Damage
            combat.min_weapon_damage *= SWORD_HEART_MORE_DMG;
            combat.max_weapon_damage *= SWORD_HEART_MORE_DMG;
            
            // 2. Base Block Chance (Shield Equivalent ~20%)
            combat.block_chance += SHIELD_BASE_BLOCK;
            // Add block amount (sword-based parry)
            combat.block_amount += SWORD_HEART_BLOCK_AMT; 
            
            // 3. Spell Damage Bonus (50% of Attack Damage Bonus)
            float phys_inc = calcs[static_cast<size_t>(StatType::PhysicalDamage)].percent_add;
            float spell_bonus = phys_inc * SWORD_HEART_SPELL_BONUS_RATIO;
            
            for (int i = 1; i < 6; ++i) {
                calcs[static_cast<size_t>(StatType::PhysicalDamage) + i].percent_add += spell_bonus;
            }
            
            LOG_INFO("Sword Heart ACTIVE for entity {}: +15% Weapon Dmg, +20% Block (50 Amt), +{:.1f}% Spell Dmg", (uint32_t)entity, spell_bonus * 100.0f);
        } else {
            LOG_DEBUG("Sword Heart INACTIVE for entity {}: Condition (One-handed Sword + Empty Offhand) not met.", (uint32_t)entity);
        }
    }

    // 1. Apply from component
    if (auto* scc = registry.try_get<StatConversionComponent>(entity)) {
        apply_conversions(scc->conversions);
    }

    // 2. Apply from Astrolabe
    if (auto* astrolabe = registry.try_get<AstrolabeComponent>(entity)) {
        const auto& reg = AstrolabeRegistry::Get();
        for (uint32_t node_id : astrolabe->activated_nodes) {
            if (const auto* node = reg.GetNode(node_id)) {
                // Apply pre-structured conversions
                apply_conversions(node->conversions);

                // Legacy string parsing support
                for (const auto& effect : node->effects) {
                    if (effect.value.find("IntToCritMult:") == 0) {
                        float ratio = std::stof(effect.value.substr(14));
                        apply_conversions({{StatType::Intelligence, StatType::CritDamage, ratio}});
                    }
                    else if (effect.value.find("IntToArmor:") == 0) {
                        float ratio = std::stof(effect.value.substr(11));
                        apply_conversions({{StatType::Intelligence, StatType::Armor, ratio}});
                    }
                }
            }
        }
    }

    // 3. Dependent Stats (e.g., Stats based on Dynamic Stacks)
    if (auto* intent = registry.try_get<SwordIntentComponent>(entity)) {
        if (intent->stacks > 0) {
            // Default mechanic: 1% Attack Speed per stack
            ApplyStatModifier(calcs, StatType::AttackSpeed, ModifierMode::Flat, (float)intent->stacks);
            LOG_TRACE("StatsSystem: Applied {}% Attack Speed from {} Sword Intent stacks", intent->stacks, intent->stacks);
        }
    }

    // 5. 最终确定次要属性
    // 5. 最终确定次要属性
    combat.max_health = calcs[static_cast<size_t>(StatType::MaxHealth)].Result();
    combat.max_mana = calcs[static_cast<size_t>(StatType::MaxMana)].Result();
    combat.armor = calcs[static_cast<size_t>(StatType::Armor)].Result();
    
    using namespace NoMoreDay::Constants::Combat;
    float finalMoveSpeed = calcs[static_cast<size_t>(StatType::MoveSpeed)].Result();
    combat.raw_move_speed = finalMoveSpeed;
    combat.move_speed = std::min(finalMoveSpeed, MOVE_SPEED_CAP);
    
    combat.crit_chance = std::min(calcs[static_cast<size_t>(StatType::CritChance)].Result() / 100.0f, Cap::CRIT_CHANCE);
    combat.crit_damage = calcs[static_cast<size_t>(StatType::CritDamage)].Result() / 100.0f;
    
    float finalAS = calcs[static_cast<size_t>(StatType::AttackSpeed)].Result() / 100.0f;
    combat.raw_attack_speed = finalAS;
    combat.attack_speed = std::min(finalAS, Cap::ATTACK_SPEED);
    
    combat.cast_speed = calcs[static_cast<size_t>(StatType::CastSpeed)].Result() / 100.0f;
    combat.accuracy = calcs[static_cast<size_t>(StatType::Accuracy)].Result() / 100.0f;
    combat.mana_on_hit = calcs[static_cast<size_t>(StatType::ManaOnHit)].Result();
    
    float finalCDR = calcs[static_cast<size_t>(StatType::CooldownReduction)].Result() / 100.0f;
    combat.raw_cooldown_reduction = finalCDR;
    combat.cooldown_reduction = std::min(finalCDR, Cap::CDR);
    
    combat.resource_cost_reduction = calcs[static_cast<size_t>(StatType::ResourceCostReduction)].Result() / 100.0f;
    
    combat.projectile_speed = calcs[static_cast<size_t>(StatType::ProjectileSpeed)].Result() / 100.0f;
    combat.duration_scale = calcs[static_cast<size_t>(StatType::DurationScale)].Result() / 100.0f;
    combat.area_scale = calcs[static_cast<size_t>(StatType::AreaScale)].Result() / 100.0f;
    
    float finalDodge = calcs[static_cast<size_t>(StatType::DodgeChance)].Result() / 100.0f;
    combat.raw_dodge_chance = finalDodge;
    combat.dodge_chance = std::min(finalDodge, Cap::DODGE);
    
    float finalBlock = (combat.block_chance + calcs[static_cast<size_t>(StatType::BlockChance)].Result() / 100.0f);
    combat.raw_block_chance = finalBlock;
    combat.block_chance = std::min(finalBlock, Cap::BLOCK);
    
    combat.life_steal += calcs[static_cast<size_t>(StatType::LifeSteal)].Result() / 100.0f;
    combat.life_on_hit += calcs[static_cast<size_t>(StatType::LifeOnHit)].Result();
    combat.health_regen += calcs[static_cast<size_t>(StatType::HealthRegen)].Result() - 1.0f;
    combat.mana_regen += calcs[static_cast<size_t>(StatType::ManaRegen)].Result() - 1.0f;
    combat.thorns += calcs[static_cast<size_t>(StatType::Thorns)].Result();
    combat.magic_find = calcs[static_cast<size_t>(StatType::MagicFind)].Result();

    // 伤害乘数
    for (int i = 0; i < 6; ++i) {
        auto& calc = calcs[static_cast<size_t>(StatType::PhysicalDamage) + i];
        combat.damage_multipliers[i] = calc.Result() / 100.0f;
        combat.damage_percent_add[i] = calc.percent_add;
        combat.damage_percent_mult_component[i] = calc.percent_mult;
    }

    // 抗性
    float resAll = calcs[static_cast<size_t>(StatType::ResistAll)].Result();
    for (int i = 0; i < 6; ++i) {
        float finalRes = (calcs[static_cast<size_t>(StatType::ResistPhysical) + i].Result() + resAll) / 100.0f;
        combat.raw_resistances[i] = finalRes;
        combat.resistances[i] = std::min(finalRes, Cap::RESISTANCE);
    }

    combat.knockback += str * Attribute::STR_TO_KNOCKBACK; // 力量增加击退

    // Finalize Regeneration (Apply Pct)
    combat.health_regen *= (1.0f + combat.health_regen_pct);
    combat.mana_regen *= (1.0f + combat.mana_regen_pct);

    if (hasTitanGrip) {
        registry.emplace<TitanGripTrait>(entity);
    }

    LOG_TRACE("StatsSystem: Recalculated for entity {}. Dmg: {:.1f}-{:.1f}, Str: {:.1f}, HP: {:.1f}", 
        (uint32_t)entity, combat.min_weapon_damage, combat.max_weapon_damage, 
        str, combat.max_health);

    // Synchronize with HealthComponent if present
    if (auto* hp = registry.try_get<HealthComponent>(entity)) {
        hp->max = combat.max_health;
        
        // Ensure current health doesn't exceed new max
        if (hp->current > hp->max) {
            hp->current = hp->max;
        }
        
        // Sync visual value for UI
        combat.health = hp->current;
    }
}

float StatsSystem::GetStatWithTags(entt::registry& registry, entt::entity entity, StatType type, Tag tags, uint32_t skill_id, entt::entity source_entity) {
    auto* combat = registry.try_get<CombatStats>(entity);
    if (!combat) return 0.0f;

    // --- Cache Lookup ---
    // Key hash: type | tags | skill_id | source_entity
    uint64_t key = 14695981039346656037ULL;
    auto hash_combine = [&](uint64_t val) {
        key ^= val;
        key *= 1099511628211ULL;
    };
    hash_combine(static_cast<uint64_t>(type));
    hash_combine(static_cast<uint64_t>(tags));
    hash_combine(static_cast<uint64_t>(skill_id));
    hash_combine(static_cast<uint64_t>(source_entity));

    uint32_t entity_id = static_cast<uint32_t>(entity);
    auto& entity_cache = s_tagStatCache[entity_id];
    if (entity_cache.contains(key)) {
        return entity_cache.at(key);
    }

    StatCalculation dynamic_calc;
    
    Tag combined_query_tags = tags;
    if (auto* stanceComp = registry.try_get<MovementStanceComponent>(entity)) {
        if (stanceComp->stance == MovementStance::SwordRiding) {
            combined_query_tags = combined_query_tags | Tag::SwordRiding;
        }
    }

    // 1. 获取 CombatStats 中已烘焙的基础值或百分比值
    switch (type) {
        case StatType::PhysicalDamage: 
            dynamic_calc.base = 100.0f; 
            dynamic_calc.percent_add = combat->damage_percent_add[0];
            dynamic_calc.percent_mult = combat->damage_percent_mult_component[0];
            break;
        case StatType::FireDamage:     
            dynamic_calc.base = 100.0f; 
            dynamic_calc.percent_add = combat->damage_percent_add[1];
            dynamic_calc.percent_mult = combat->damage_percent_mult_component[1];
            break;
        case StatType::ColdDamage:     
            dynamic_calc.base = 100.0f; 
            dynamic_calc.percent_add = combat->damage_percent_add[2];
            dynamic_calc.percent_mult = combat->damage_percent_mult_component[2];
            break;
        case StatType::LightningDamage: 
            dynamic_calc.base = 100.0f; 
            dynamic_calc.percent_add = combat->damage_percent_add[3];
            dynamic_calc.percent_mult = combat->damage_percent_mult_component[3];
            break;
        case StatType::PoisonDamage:    
            dynamic_calc.base = 100.0f; 
            dynamic_calc.percent_add = combat->damage_percent_add[4];
            dynamic_calc.percent_mult = combat->damage_percent_mult_component[4];
            break;
        case StatType::ShadowDamage:    
            dynamic_calc.base = 100.0f; 
            dynamic_calc.percent_add = combat->damage_percent_add[5];
            dynamic_calc.percent_mult = combat->damage_percent_mult_component[5];
            break;
        
        case StatType::CritChance:    dynamic_calc.base = combat->crit_chance * 100.0f; break;
        case StatType::CritDamage:    dynamic_calc.base = combat->crit_damage * 100.0f; break;
        case StatType::AttackSpeed:   dynamic_calc.base = combat->attack_speed * 100.0f; break;
        case StatType::CastSpeed:     dynamic_calc.base = combat->cast_speed * 100.0f; break;
        case StatType::Accuracy:      dynamic_calc.base = combat->accuracy * 100.0f; break;
        case StatType::ManaOnHit:     dynamic_calc.base = combat->mana_on_hit; break;
        case StatType::ArmorPenetration: dynamic_calc.base = combat->armor_pen; break;
        case StatType::MoveSpeed:     dynamic_calc.base = combat->move_speed; break;
        case StatType::Armor:         dynamic_calc.base = combat->armor; break;
        case StatType::MaxHealth:     dynamic_calc.base = combat->max_health; break;
        case StatType::MaxMana:       dynamic_calc.base = combat->max_mana; break;
        case StatType::CooldownReduction: dynamic_calc.base = combat->cooldown_reduction * 100.0f; break;
        case StatType::ResourceCostReduction: dynamic_calc.base = combat->resource_cost_reduction * 100.0f; break;
        case StatType::ProjectileCount: 
            // If skill_id is provided, use the base count for that specific skill
            if (skill_id == 2) dynamic_calc.base = 1.0f; // Rending Wave base
            else dynamic_calc.base = 0.0f; 
            break;
        case StatType::AreaScale:       dynamic_calc.base = combat->area_scale * 100.0f; break;
        case StatType::ProjectileSpeed: dynamic_calc.base = combat->projectile_speed * 100.0f; break;
        case StatType::DurationScale:   dynamic_calc.base = combat->duration_scale * 100.0f; break;
        case StatType::DodgeChance:     dynamic_calc.base = combat->dodge_chance * 100.0f; break;
        case StatType::BlockChance:     dynamic_calc.base = combat->block_chance * 100.0f; break;
        case StatType::LifeSteal:       dynamic_calc.base = combat->life_steal * 100.0f; break;
        case StatType::LifeOnHit:       dynamic_calc.base = combat->life_on_hit; break;
        case StatType::HealthRegen:     dynamic_calc.base = combat->health_regen; break;
        case StatType::ManaRegen:       dynamic_calc.base = combat->mana_regen; break;
        case StatType::Thorns:          dynamic_calc.base = combat->thorns; break;
        case StatType::MagicFind:       dynamic_calc.base = combat->magic_find; break;

        default: break;
    }

    // 2. 累加动态标签修饰符
    auto apply_if_tags_match = [&](const std::vector<StatModifier>& modifiers, float scale = 1.0f) {
        for (const auto& mod : modifiers) {
            bool type_match = (mod.type == type);

            // Conversion Inheritance: If querying a Damage Stat, also apply modifiers for
            // other Damage Types if the source tags contain that type.
            if (!type_match && IsDamageStat(type) && IsDamageStat(mod.type)) {
                Tag mod_tag = GetTagFromDamageStat(mod.type);
                if (mod_tag != Tag::None && HasTag(combined_query_tags, mod_tag)) {
                    type_match = true;
                }
            }

            if (type_match) {
                bool tags_match = (mod.required_tags == Tag::None || HasTag(combined_query_tags, mod.required_tags));

                if (tags_match) {
                    ApplyStatCalculation(dynamic_calc, mod.mode, mod.value * scale);
                }
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

    // 2.2 处理源实体修饰符 (SkillModifierComponent on source_entity)
    if (registry.valid(source_entity)) {
        if (auto* skillMods = registry.try_get<SkillModifierComponent>(source_entity)) {
            apply_if_tags_match(skillMods->stat_modifiers);
        }
    }

    // 2.3 NEW: 处理条件装备词缀 (GlobalModifierComponent.stat_modifiers)
    if (auto* global = registry.try_get<GlobalModifierComponent>(entity)) {
        apply_if_tags_match(global->stat_modifiers);
    }

    // 2.4 NEW: 处理 Avenger (复仇者) 词缀加成
    if (IsDamageStat(type)) {
        if (auto* avenger = registry.try_get<AvengerComponent>(entity)) {
            // 将加成应用到 percent_add，确保是加法叠加
            dynamic_calc.percent_add += (avenger->GetDamageMultiplier() - 1.0f);
        }
    }

    // 3. 处理技能专精天赋 (Skill Specialization Talents)
    if (skill_id != 0) {
        if (auto* active = registry.try_get<ActiveSkillsComponent>(entity)) {
            bool found_specialized = false;
            for (const auto& specialized : active->specialized_slots) {
                if (specialized.skill_id == skill_id) {
                    found_specialized = true;
                    const auto* tree = SkillRegistry::Get().GetSkillTree(skill_id);
                    if (tree) {
                        for (auto [node_id, pts] : specialized.allocated_points) {
                            auto node_it = tree->nodes.find(node_id);
                            if (node_it != tree->nodes.end()) {
                                apply_if_tags_match(node_it->second.stat_modifiers, static_cast<float>(pts));
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    float result = dynamic_calc.Result();
    s_tagStatCache[static_cast<uint32_t>(entity)][key] = result;
    return result;
}

void StatsSystem::update(entt::registry& registry) {
    auto view = registry.view<StatsDirty>();
    for (auto entity : view) {
        Recalculate(registry, entity);
    }
    registry.clear<StatsDirty>();
}

void StatsSystem::UpdateBuffs(entt::registry& registry, float dt) {
    auto view = registry.view<ActiveEffectsComponent>();
    for (auto entity : view) {
        auto& effects = view.get<ActiveEffectsComponent>(entity);
        size_t before = effects.effects.size();
        
        effects.Update(dt);
        
        if (effects.effects.size() != before) {
            registry.get_or_emplace<StatsDirty>(entity);
        }
        
        // Visuals for Status Effects
        if (registry.all_of<Position>(entity)) {
            const auto& pos = registry.get<Position>(entity);
            for (const auto& buff : effects.effects) {
                if (buff.type == BuffType::Freeze) {
                    // Time-based: ~30% at 60 FPS
                    if (utils::FrameRateUtils::ShouldTrigger(30.0f, dt)) {
                         components::GPUParticle p;
                         p.position = { pos.x + GetRandomValue(-10, 10), pos.y + GetRandomValue(-10, 10) };
                         p.velocity = { 0, -10.0f };
                         p.acceleration = { 0, 0 };
                         p.color = SKYBLUE;
                         p.lifetime = 0.5f;
                         p.maxLifetime = 0.5f;
                         p.scale = 1.2f;
                         p.flags = 0; // Soft
                         systems::GPUParticleSystem::Get().Emit(p);
                    }
                }
                else if (buff.type == BuffType::Burn) {
                    // Time-based: ~30% at 60 FPS
                    if (utils::FrameRateUtils::ShouldTrigger(30.0f, dt)) {
                         components::GPUParticle p;
                         p.position = { pos.x + GetRandomValue(-8, 8), pos.y + GetRandomValue(-5, 5) };
                         p.velocity = { 0, -30.0f }; // Rise fast
                         p.acceleration = { 0, 0 };
                         p.color = ORANGE;
                         p.lifetime = 0.4f;
                         p.maxLifetime = 0.4f;
                         p.scale = 1.5f;
                         p.flags = 0; // Soft
                         systems::GPUParticleSystem::Get().Emit(p);
                    }
                }
                else if (buff.type == BuffType::Stun || buff.id == "shock") {
                    // Time-based: ~20% at 60 FPS
                    if (utils::FrameRateUtils::ShouldTrigger(20.0f, dt)) {
                         components::GPUParticle p;
                         p.position = { pos.x + GetRandomValue(-10, 10), pos.y + GetRandomValue(-20, 0) };
                         p.velocity = { 0, 0 };
                         p.acceleration = { 0, 0 };
                         p.color = YELLOW;
                         p.lifetime = 0.2f;
                         p.maxLifetime = 0.2f;
                         p.scale = 1.0f;
                         p.flags = 2; // Spark
                         systems::GPUParticleSystem::Get().Emit(p);
                    }
                }
            }
        }
    }
}

void StatsSystem::ClearCache(entt::registry&, entt::entity entity) {
    uint32_t entity_id = static_cast<uint32_t>(entity);
    s_tagStatCache.erase(entity_id);
}

void StatsSystem::Initialize(entt::registry &registry) {
    registry.on_destroy<CombatStats>().connect<&StatsSystem::ClearCache>();
}

void StatsSystem::Shutdown(entt::registry &registry) {
    registry.on_destroy<CombatStats>().disconnect<&StatsSystem::ClearCache>();
}

} // namespace NoMoreDay