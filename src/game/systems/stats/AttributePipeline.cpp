#include "game/systems/stats/AttributePipeline.hpp"
#include "core/utils/FrameRateUtils.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Common.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/ItemStats.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Progression.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatFormula.hpp"
#include "game/utils/MonsterScaling.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <spdlog/spdlog.h>
#include <vector>

namespace NoMoreDay {

// -----------------------------------------------------------------------------
// Helper: Reset Stats
// -----------------------------------------------------------------------------
static void ResetCombatStats(CombatStats &combat) {
  using namespace NoMoreDay::Constants::Combat;
  // Preserve current values to clamp later
  float current_health = combat.health;
  float current_mana = combat.mana;
  float current_barrier = combat.barrier;

  combat.max_health = DEFAULT_MAX_HEALTH;
  combat.max_mana = DEFAULT_MAX_MANA;
  combat.min_weapon_damage = 0.0f;
  combat.max_weapon_damage = 0.0f;
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
  combat.pickup_range = BASE_PICKUP_RANGE;

  combat.raw_resistances.fill(0.0f);
  combat.raw_move_speed = 0.0f;
  combat.raw_cooldown_reduction = 0.0f;
  combat.raw_attack_speed = 0.0f;
  combat.raw_dodge_chance = 0.0f;
  combat.raw_block_chance = 0.0f;

  combat.health_regen = 1.0f;
  combat.mana_regen = 1.0f;
  combat.health_regen_pct = 0.0f;
  combat.mana_regen_pct = 0.0f;

  combat.max_barrier = 0.0f;
  combat.barrier_regen = 0.0f;
  combat.barrier_decay = 0.2f;
  combat.barrier_delay = 2.0f;
  combat.barrier_retention = 0.0f;

  // Restore currents (clamping handled later)
  combat.health = current_health;
  combat.mana = current_mana;
  combat.barrier = current_barrier;
}

// -----------------------------------------------------------------------------
// Helper: Apply Stat Calculation
// -----------------------------------------------------------------------------
struct StatCalculation {
  float base = 0.0f;
  float flat = 0.0f;
  float percent_add = 0.0f;
  float percent_mult = 1.0f;

  float Result() const {
    return (base + flat) * (1.0f + percent_add) * percent_mult;
  }
};

static void ApplyStatCalculation(StatCalculation &c, ModifierMode mode,
                                 float value) {
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

static void ApplyStatModifier(
    std::array<StatCalculation, static_cast<size_t>(StatType::Count)> &calcs,
    StatType type, ModifierMode mode, float value) {
  if (type < StatType::Count) {
    ApplyStatCalculation(calcs[static_cast<size_t>(type)], mode, value);
  }
}

// -----------------------------------------------------------------------------
// Affix Dispatcher (Simplified for Hybrid Pipeline)
// -----------------------------------------------------------------------------
// Helper Context for Affix Dispatch
struct AffixContext {
  std::array<StatCalculation, static_cast<size_t>(StatType::Count)> &calcs;
  CombatStats &combat;
  entt::registry &registry;
  entt::entity entity;
  bool &hasTitanGrip;
  GlobalModifierComponent &global_mods;
};

using AffixHandler = std::function<void(AffixContext &, const Affix &)>;

class AffixDispatcher {
public:
  static constexpr size_t TABLE_SIZE = 128;
  std::array<AffixHandler, TABLE_SIZE> handlers = {};

  AffixDispatcher() {
    handlers.fill([](AffixContext &, const Affix &) {});

    auto bind = [&](AffixType type, StatType stat, ModifierMode mode) {
      handlers[(int)type] = [stat, mode](AffixContext &ctx, const Affix &a) {
        ApplyStatModifier(ctx.calcs, stat, mode, a.value);
      };
    };

    // Primary
    bind(AffixType::Strength, StatType::Strength, ModifierMode::Flat);
    bind(AffixType::Dexterity, StatType::Dexterity, ModifierMode::Flat);
    bind(AffixType::Intelligence, StatType::Intelligence, ModifierMode::Flat);
    bind(AffixType::Vitality, StatType::Vitality, ModifierMode::Flat);

    // All Attributes
    handlers[(int)AffixType::AllAttributes] = [](AffixContext &ctx,
                                                 const Affix &a) {
      ApplyStatModifier(ctx.calcs, StatType::Strength, ModifierMode::Flat,
                        a.value);
      ApplyStatModifier(ctx.calcs, StatType::Dexterity, ModifierMode::Flat,
                        a.value);
      ApplyStatModifier(ctx.calcs, StatType::Intelligence, ModifierMode::Flat,
                        a.value);
      ApplyStatModifier(ctx.calcs, StatType::Vitality, ModifierMode::Flat,
                        a.value);
    };

    // Damage (Direct to flat_damage array in CombatStats for now, or use
    // StatType if mapped) StatType has PhysicalDamage (Percent), but not Flat.
    // We stick to ctx.combat for flats as per legacy
    auto bindDirect = [&](AffixType type, DamageType dt) {
      handlers[(int)type] = [dt](AffixContext &ctx, const Affix &a) {
        ctx.combat.flat_damage[(int)dt] += a.value;
      };
    };
    bindDirect(AffixType::FlatPhysicalDamage, DamageType::Physical);
    bindDirect(AffixType::FlatFireDamage, DamageType::Fire);
    bindDirect(AffixType::FlatColdDamage, DamageType::Cold);
    bindDirect(AffixType::FlatLightningDamage, DamageType::Lightning);
    bindDirect(AffixType::FlatPoisonDamage, DamageType::Poison);
    bindDirect(AffixType::FlatShadowDamage, DamageType::Shadow);

    // Percent Damage
    bind(AffixType::PercentPhysicalDamage, StatType::PhysicalDamage,
         ModifierMode::PercentAdd);
    bind(AffixType::PercentFireDamage, StatType::FireDamage,
         ModifierMode::PercentAdd);
    bind(AffixType::PercentColdDamage, StatType::ColdDamage,
         ModifierMode::PercentAdd);
    bind(AffixType::PercentLightningDamage, StatType::LightningDamage,
         ModifierMode::PercentAdd);
    bind(AffixType::PercentPoisonDamage, StatType::PoisonDamage,
         ModifierMode::PercentAdd);
    bind(AffixType::PercentShadowDamage, StatType::ShadowDamage,
         ModifierMode::PercentAdd);

    // Speed / Crit
    bind(AffixType::CritChance, StatType::CritChance, ModifierMode::Flat);
    bind(AffixType::CritDamage, StatType::CritDamage, ModifierMode::Flat);
    bind(AffixType::AttackSpeed, StatType::AttackSpeed,
         ModifierMode::PercentAdd);
    bind(AffixType::CastSpeed, StatType::CastSpeed, ModifierMode::PercentAdd);
    bind(AffixType::Accuracy, StatType::Accuracy, ModifierMode::Flat);

    // Defense
    bind(AffixType::FlatArmor, StatType::Armor, ModifierMode::Flat);
    bind(AffixType::PercentArmor, StatType::Armor, ModifierMode::PercentAdd);
    bind(AffixType::FlatHealth, StatType::MaxHealth, ModifierMode::Flat);
    bind(AffixType::PercentHealth, StatType::MaxHealth,
         ModifierMode::PercentAdd);
    bind(AffixType::FlatMana, StatType::MaxMana, ModifierMode::Flat);

    // Resists
    bind(AffixType::ResistFire, StatType::ResistFire, ModifierMode::Flat);
    bind(AffixType::ResistCold, StatType::ResistCold, ModifierMode::Flat);
    bind(AffixType::ResistLightning, StatType::ResistLightning,
         ModifierMode::Flat);
    bind(AffixType::ResistPoison, StatType::ResistPoison, ModifierMode::Flat);
    bind(AffixType::ResistShadow, StatType::ResistShadow, ModifierMode::Flat);
    bind(AffixType::ResistAll, StatType::ResistAll, ModifierMode::Flat);

    // Utility
    bind(AffixType::MoveSpeed, StatType::MoveSpeed, ModifierMode::PercentAdd);
    bind(AffixType::CooldownReduction, StatType::CooldownReduction,
         ModifierMode::Flat); // Affix value 10 = 10%
    bind(AffixType::LifeOnHit, StatType::LifeOnHit, ModifierMode::Flat);
    bind(AffixType::ManaOnHit, StatType::ManaOnHit, ModifierMode::Flat);
    bind(AffixType::HealthRegen, StatType::HealthRegen, ModifierMode::Flat);
    bind(AffixType::ManaRegen, StatType::ManaRegen, ModifierMode::Flat);
    bind(AffixType::PercentHealthRegen, StatType::HealthRegen,
         ModifierMode::PercentAdd);
    bind(AffixType::PercentManaRegen, StatType::ManaRegen,
         ModifierMode::PercentAdd); // Assuming ManaRegen stat can take
                                    // PercentAdd
    bind(AffixType::Thorns, StatType::Thorns, ModifierMode::Flat);

    // Ratings
    bind(AffixType::FlatDodgeRating, StatType::DodgeRating, ModifierMode::Flat);
    bind(AffixType::PercentDodgeRating, StatType::DodgeRating,
         ModifierMode::PercentAdd);
    bind(AffixType::FlatBlockRating, StatType::BlockRating, ModifierMode::Flat);
    bind(AffixType::PercentBlockRating, StatType::BlockRating,
         ModifierMode::PercentAdd);

    // Special
    handlers[(int)AffixType::DamageReduction] = [](AffixContext &c,
                                                   const Affix &a) {
      ApplyStatModifier(c.calcs, StatType::GlobalDamageReduction,
                        ModifierMode::Flat, a.value);
    };
    handlers[(int)AffixType::LifeSteal] = [](AffixContext &c, const Affix &a) {
      ApplyStatModifier(c.calcs, StatType::LifeSteal, ModifierMode::Flat,
                        a.value);
    };

    handlers[(int)AffixType::TitanGrip] = [](AffixContext &ctx, const Affix &) {
      ctx.hasTitanGrip = true;
    };

    // Skill bonuses - simplified
    handlers[(int)AffixType::PlusAllSkills] = [](AffixContext &ctx,
                                                 const Affix &a) {
      if (auto *active =
              ctx.registry.try_get<ActiveSkillsComponent>(ctx.entity)) {
        for (auto &specialized : active->specialized_slots) {
          if (specialized.skill_id != 0)
            specialized.bonus_levels += (int)a.value;
        }
      }
    };
  }

  void Dispatch(AffixContext &ctx, const Affix &affix) const {
    if (static_cast<size_t>(affix.type) < TABLE_SIZE) {
      handlers[static_cast<size_t>(affix.type)](ctx, affix);
    }
  }

  static const AffixDispatcher &Get() {
    static AffixDispatcher instance;
    return instance;
  }
};

// -----------------------------------------------------------------------------
// AttributePipeline Main Logic
// -----------------------------------------------------------------------------

void AttributePipeline::Calculate(entt::registry &registry,
                                  entt::entity entity) {
  if (!registry.valid(entity))
    return;

  auto &stats = registry.get_or_emplace<CombatStats>(entity);
  ResetCombatStats(stats);

  // Phase 0: Reset & Base Setup
  if (registry.all_of<TitanGripTrait>(entity))
    registry.remove<TitanGripTrait>(entity);
  if (auto *active = registry.try_get<ActiveSkillsComponent>(entity)) {
    for (auto &specialized : active->specialized_slots)
      specialized.bonus_levels = 0;
  }

  auto &global_mods = registry.get_or_emplace<GlobalModifierComponent>(entity);
  global_mods.modifiers.clear();
  global_mods.stat_modifiers.clear();

  std::array<StatCalculation, static_cast<size_t>(StatType::Count)> calcs;

  // Base Stats
  using namespace NoMoreDay::Constants::Combat;
  bool isPlayer = registry.all_of<PlayerTag>(entity);
  calcs[static_cast<size_t>(StatType::MaxHealth)].base =
      isPlayer ? DEFAULT_MAX_HEALTH : 1.0f;
  calcs[static_cast<size_t>(StatType::MaxMana)].base =
      isPlayer ? DEFAULT_MAX_MANA : 1.0f;
  calcs[static_cast<size_t>(StatType::MoveSpeed)].base = DEFAULT_MOVE_SPEED;
  calcs[static_cast<size_t>(StatType::CritChance)].base =
      DEFAULT_CRIT_CHANCE * 100.0f;
  calcs[static_cast<size_t>(StatType::CritDamage)].base =
      DEFAULT_CRIT_DAMAGE * 100.0f;
  calcs[static_cast<size_t>(StatType::AttackSpeed)].base =
      DEFAULT_ATTACK_SPEED * 100.0f;
  calcs[static_cast<size_t>(StatType::CastSpeed)].base = 100.0f;
  calcs[static_cast<size_t>(StatType::Accuracy)].base =
      DEFAULT_ACCURACY * 100.0f;
  calcs[static_cast<size_t>(StatType::ProjectileSpeed)].base = 100.0f;
  calcs[static_cast<size_t>(StatType::DurationScale)].base = 100.0f;
  calcs[static_cast<size_t>(StatType::AreaScale)].base = 100.0f;
  calcs[static_cast<size_t>(StatType::HealthRegen)].base = 1.0f;
  calcs[static_cast<size_t>(StatType::ManaRegen)].base = REGEN_BASE;
  calcs[static_cast<size_t>(StatType::MagicFind)].base = MAGIC_FIND_BASE;
  calcs[static_cast<size_t>(StatType::BarrierDecay)].base = 20.0f;
  calcs[static_cast<size_t>(StatType::BarrierDelay)].base = 2.0f;

  for (int i = 0; i < 6; ++i)
    calcs[static_cast<size_t>(StatType::PhysicalDamage) + i].base = 100.0f;

  // Enemy Base & Scaling
  if (auto *enemy = registry.try_get<EnemyStateComponent>(entity)) {
    EnemyRarityComponent::Rarity rarity = EnemyRarityComponent::NORMAL;
    if (auto *rarityComp = registry.try_get<EnemyRarityComponent>(entity))
      rarity = rarityComp->rarity;

    auto scaled =
        MonsterScaling::Calculate(enemy->raceType, enemy->level, rarity);
    calcs[static_cast<size_t>(StatType::MaxHealth)].base = scaled.maxHealth;
    calcs[static_cast<size_t>(StatType::Armor)].base = scaled.armor;

    // Resists
    const auto &raceData = kRaceData[static_cast<size_t>(enemy->raceType)];
    constexpr float NATIVE_RES = 50.0f;
    float bonus = scaled.resistanceBonus * 100.0f;
    auto applyRes = [&](Tag tag, StatType t) {
      float val = bonus;
      if (HasTag(raceData.resistances, tag))
        val += NATIVE_RES;
      if (val > 0)
        ApplyStatModifier(calcs, t, ModifierMode::Flat, val);
    };
    applyRes(Tag::Physical, StatType::ResistPhysical);
    applyRes(Tag::Fire, StatType::ResistFire);
    applyRes(Tag::Cold, StatType::ResistCold);
    applyRes(Tag::Lightning, StatType::ResistLightning);
    applyRes(Tag::Poison, StatType::ResistPoison);
    applyRes(Tag::Shadow, StatType::ResistShadow);

    calcs[static_cast<size_t>(StatType::MoveSpeed)].base = raceData.baseSpeed;
  }

  if (auto *primary = registry.try_get<PrimaryStats>(entity)) {
    calcs[static_cast<size_t>(StatType::Strength)].base = primary->strength;
    calcs[static_cast<size_t>(StatType::Dexterity)].base = primary->dexterity;
    calcs[static_cast<size_t>(StatType::Intelligence)].base =
        primary->intelligence;
    calcs[static_cast<size_t>(StatType::Vitality)].base = primary->vitality;
  }

  // Phase 1: Gather Modifiers
  bool hasTitanGrip = false;
  AffixContext ctx{calcs, stats, registry, entity, hasTitanGrip, global_mods};

  auto processAffixes = [&](const std::vector<Affix> &affixes) {
    for (const auto &affix : affixes) {
      if (affix.required_tags != Tag::None) {
        StatModifier mod;
        mod.type = static_cast<StatType>(
            affix.type); // Rough cast, handled by GetStatWithTags
        mod.mode = ModifierMode::PercentAdd;
        mod.value = affix.value;
        mod.required_tags = affix.required_tags;
        mod.source = ModifierSource::Item;
        global_mods.stat_modifiers.push_back(mod);
      } else {
        AffixDispatcher::Get().Dispatch(ctx, affix);
      }
    }
  };

  Tag entity_tags = Tag::None;
  if (auto *stance = registry.try_get<MovementStanceComponent>(entity)) {
    if (stance->stance == MovementStance::SwordRiding)
      entity_tags = entity_tags | Tag::SwordRiding;
  }

  // Equipment & Set Bonuses
  struct SetTrack {
    uint32_t hash;
    int count;
    const std::vector<SetBonus> *defs;
  };
  static thread_local std::vector<SetTrack> s_set_scratch;
  s_set_scratch.clear();

  bool hasMain = false, hasOff = false, is2H = false, offEmpty = true;
  float mainAtk = 0, offAtk = 0;

  if (auto *eq = registry.try_get<EquipmentComponent>(entity)) {
    offEmpty = !registry.valid(eq->slots[(int)EquipmentSlot::OffHand]);
    for (const auto &eItem : eq->slots) {
      if (registry.valid(eItem) && registry.all_of<ItemComponent>(eItem)) {
        const auto &item = registry.get<ItemComponent>(eItem);
        if (item.type == ItemType::Weapon && item.attack > 0) {
          if (item.slot == EquipmentSlot::MainHand) {
            mainAtk = item.attack;
            hasMain = true;
            is2H = item.isTwoHanded;
          } else if (item.slot == EquipmentSlot::OffHand) {
            offAtk = item.attack;
            hasOff = true;
          }
        }
        processAffixes(item.implicits);
        processAffixes(item.affixes);
        if (item.defense > 0)
          ApplyStatModifier(calcs, StatType::Armor, ModifierMode::Flat,
                            item.defense);

        for (auto r : item.sockets) {
          if (registry.valid(r) && registry.all_of<RuneComponent>(r)) {
            const auto &rune = registry.get<RuneComponent>(r);
            if (item.type == ItemType::Weapon)
              processAffixes(rune.weaponEffects);
            else if (item.slot == EquipmentSlot::Neck ||
                     item.slot == EquipmentSlot::Ring1 ||
                     item.slot == EquipmentSlot::Ring2)
              processAffixes(rune.jewelryEffects);
            else
              processAffixes(rune.armorEffects);
          }
        }

        // Set Bonus Tracking
        if (item.rarity == Rarity::Set && item.setNameHash != 0) {
          bool found = false;
          for (auto &t : s_set_scratch) {
            if (t.hash == item.setNameHash) {
              t.count++;
              found = true;
              break;
            }
          }
          if (!found)
            s_set_scratch.push_back({item.setNameHash, 1, &item.setBonuses});
        }
      }
    }
  }

  // Apply Set Bonuses
  for (const auto &t : s_set_scratch) {
    if (t.defs) {
      for (const auto &sb : *t.defs) {
        if (t.count >= sb.requiredCount)
          processAffixes(sb.bonuses);
      }
    }
  }

  // Weapon Logic
  if (hasMain) {
    if (hasOff) { // Dual Wield
      float avg = (mainAtk + offAtk) * 0.5f;
      stats.min_weapon_damage = avg * 0.9f;
      stats.max_weapon_damage = avg * 1.1f;
      ApplyStatModifier(calcs, StatType::AttackSpeed, ModifierMode::PercentAdd,
                        System::DUAL_WIELD_AS_BONUS);
    } else {
      stats.min_weapon_damage = mainAtk * 0.9f;
      stats.max_weapon_damage = mainAtk * 1.1f;
      if (is2H) {
        stats.min_weapon_damage *= System::TWO_HANDED_DMG_BONUS;
        stats.max_weapon_damage *= System::TWO_HANDED_DMG_BONUS;
      }
    }
    if (stats.knockback < 0.1f)
      stats.knockback = 20.0f;
  } else if (registry.all_of<EquipmentComponent>(entity)) {
    stats.min_weapon_damage = 2.0f;
    stats.max_weapon_damage = 3.0f;
    stats.knockback = 10.0f;
  } else if (auto *wc = registry.try_get<WeaponComponent>(entity)) {
    stats.min_weapon_damage = wc->damage;
    stats.max_weapon_damage = wc->damage;
  } else if (auto *en = registry.try_get<EnemyStateComponent>(entity)) {
    // Monster Fallback
    float baseDmg = kRaceData[static_cast<size_t>(en->raceType)].baseDamage *
                    (1.0f + en->level * 0.1f);
    stats.min_weapon_damage = baseDmg * 0.9f;
    stats.max_weapon_damage = baseDmg * 1.1f;
  }

  // Soul Eater & Avenger (Mechanics)
  if (auto *se = registry.try_get<SoulEaterComponent>(entity)) {
    if (se->stacks > 0) {
      float dmgBonus = se->stacks * se->damagePerStack;
      float asBonus = se->stacks * se->attackSpeedPerStack;
      for (int i = 0; i < 6; ++i)
        ApplyStatModifier(calcs,
                          static_cast<StatType>(
                              static_cast<int>(StatType::PhysicalDamage) + i),
                          ModifierMode::PercentAdd, dmgBonus);
      ApplyStatModifier(calcs, StatType::AttackSpeed, ModifierMode::PercentAdd,
                        asBonus);
    }
  }
  if (auto *av = registry.try_get<AvengerComponent>(entity)) {
    if (av->avengerStacks > 0) {
      float dmgBonus = av->avengerStacks * av->damagePerStack * 100.0f;
      for (int i = 0; i < 6; ++i)
        ApplyStatModifier(calcs,
                          static_cast<StatType>(
                              static_cast<int>(StatType::PhysicalDamage) + i),
                          ModifierMode::PercentAdd, dmgBonus);
    }
  }

  // Other Modifiers
  if (auto *list = registry.try_get<ModifierList>(entity)) {
    for (const auto &mod : list->modifiers) {
      if (mod.required_tags == Tag::None ||
          HasTag(entity_tags, mod.required_tags)) {
        ApplyStatModifier(calcs, mod.type, mod.mode, mod.value);
      }
    }
  }

  if (auto *affixComp = registry.try_get<MonsterAffixComponent>(entity)) {
    for (auto at : affixComp->affixes) {
      const auto &def = MonsterAffixRegistry::GetAffixDef(at);
      for (int i = 0; i < def.statModCount; ++i)
        ApplyStatModifier(calcs, def.statMods[i].type, def.statMods[i].mode,
                          def.statMods[i].value);
      if (affixComp->isBerserk) {
        stats.min_weapon_damage *= 2.0f;
        stats.max_weapon_damage *= 2.0f;
      }
    }
  }

  // Phase 2: Resolve Primary & Conversions (Revised)
  float str = calcs[static_cast<size_t>(StatType::Strength)].Result();
  float dex = calcs[static_cast<size_t>(StatType::Dexterity)].Result();
  float intel = calcs[static_cast<size_t>(StatType::Intelligence)].Result();
  float vit = calcs[static_cast<size_t>(StatType::Vitality)].Result();

  stats.effective_strength = str;
  stats.effective_dexterity = dex;
  stats.effective_intelligence = intel;
  stats.effective_vitality = vit;

  // Apply Stat Conversions
  calcs[static_cast<size_t>(StatType::Armor)].base +=
      str * Attribute::STR_TO_ARMOR;
  calcs[static_cast<size_t>(StatType::MaxHealth)].base +=
      vit * Attribute::VIT_TO_HEALTH;
  calcs[static_cast<size_t>(StatType::MaxMana)].base +=
      intel * Attribute::INT_TO_MANA;

  stats.health_regen += vit * Attribute::VIT_TO_HEALTH_REGEN;
  stats.mana_regen += intel * Attribute::INT_TO_MANA_REGEN;
  ApplyStatModifier(calcs, StatType::PhysicalDamage, ModifierMode::PercentAdd,
                    str * Attribute::STR_TO_PHYS_DAMAGE_INC);
  ApplyStatModifier(calcs, StatType::CritChance, ModifierMode::Flat,
                    dex * Attribute::DEX_TO_CRIT_CHANCE);
  ApplyStatModifier(calcs, StatType::Accuracy, ModifierMode::Flat,
                    dex * Attribute::DEX_TO_ACCURACY);

  // Phase 3 & 4: Finalize
  int area_level = (registry.all_of<EnemyStateComponent>(entity))
                       ? registry.get<EnemyStateComponent>(entity).level
                       : 1; // Simplified level check
  if (auto *p = registry.try_get<PlayerStats>(entity))
    area_level = std::max(1, p->level);
  stats.cached_area_level = area_level;

  stats.max_health = calcs[static_cast<size_t>(StatType::MaxHealth)].Result();
  stats.max_mana = calcs[static_cast<size_t>(StatType::MaxMana)].Result();
  stats.armor = calcs[static_cast<size_t>(StatType::Armor)].Result();

  // Barrier
  stats.max_barrier +=
      calcs[static_cast<size_t>(StatType::MaxBarrier)].Result();
  stats.barrier_regen +=
      calcs[static_cast<size_t>(StatType::BarrierRegen)].Result();
  stats.barrier_decay =
      calcs[static_cast<size_t>(StatType::BarrierDecay)].Result() / 100.0f;
  stats.barrier_delay =
      calcs[static_cast<size_t>(StatType::BarrierDelay)].Result();
  stats.barrier_retention =
      calcs[static_cast<size_t>(StatType::BarrierRetention)].Result() / 100.0f;
  stats.barrier_retention +=
      intel * Attribute::INT_TO_BARRIER_RETENTION / 100.0f;

  // Ratings
  stats.dodge_rating =
      calcs[static_cast<size_t>(StatType::DodgeRating)].Result();
  stats.block_rating =
      calcs[static_cast<size_t>(StatType::BlockRating)].Result();
  float formula_dodge =
      CombatFormula::CalculateDodgeChance(stats.dodge_rating, area_level);
  float flat_dodge = calcs[static_cast<size_t>(StatType::DodgeChance)].Result();
  stats.effective_dodge = formula_dodge + flat_dodge;
  stats.dodge_chance =
      std::min(stats.effective_dodge, Scaling::DODGE_MAX_CHANCE);
  stats.effective_dodge = stats.dodge_chance;

  float flat_block = calcs[static_cast<size_t>(StatType::BlockChance)].Result();
  stats.block_chance = std::min(flat_block, Scaling::BLOCK_MAX_CHANCE);
  stats.block_amount = stats.block_rating;
  stats.effective_block_eff = CombatFormula::CalculateBlockEffectiveness(
      stats.block_rating, area_level);

  // Armor DR
  float armor_mult =
      CombatFormula::CalculateArmorMultiplier(stats.armor, area_level);
  stats.effective_armor_dr = 1.0f - armor_mult;

  // Apply Global DR (mapped from StatType::GlobalDamageReduction)
  stats.damage_reduction =
      calcs[static_cast<size_t>(StatType::GlobalDamageReduction)].Result() /
      100.0f;

  if (hasTitanGrip)
    registry.emplace<TitanGripTrait>(entity);

  // Sync health cap
  if (auto *hp = registry.try_get<HealthComponent>(entity)) {
    hp->max = stats.max_health;
    if (hp->current > hp->max)
      hp->current = hp->max;
    stats.health = hp->current;
  }
  if (stats.max_barrier > 0 || stats.barrier > 0)
    registry.get_or_emplace<BarrierComponent>(entity);

  // Phase 5: GPU Sync
  // Rely on GPUEntitySystem to call ToGPU

  // Populate remaining simple stats
  stats.move_speed = std::min(
      calcs[static_cast<size_t>(StatType::MoveSpeed)].Result(), MOVE_SPEED_CAP);
  stats.raw_move_speed =
      calcs[static_cast<size_t>(StatType::MoveSpeed)].Result();
  stats.crit_chance = std::min(
      calcs[static_cast<size_t>(StatType::CritChance)].Result() / 100.0f,
      Cap::CRIT_CHANCE);
  stats.crit_damage =
      calcs[static_cast<size_t>(StatType::CritDamage)].Result() / 100.0f;
  stats.attack_speed = std::min(
      calcs[static_cast<size_t>(StatType::AttackSpeed)].Result() / 100.0f,
      Cap::ATTACK_SPEED);

  // Damage multipliers finalization
  for (int i = 0; i < 6; ++i) {
    auto &calc = calcs[static_cast<size_t>(StatType::PhysicalDamage) + i];
    stats.damage_multipliers[i] = calc.Result() / 100.0f;
    stats.damage_percent_add[i] = calc.percent_add;
    stats.damage_percent_mult_component[i] = calc.percent_mult;
  }
  float resAll = calcs[static_cast<size_t>(StatType::ResistAll)].Result();
  for (int i = 0; i < 6; ++i) {
    float finalRes =
        (calcs[static_cast<size_t>(StatType::ResistPhysical) + i].Result() +
         resAll) /
        100.0f;
    stats.raw_resistances[i] = finalRes;
    stats.resistances[i] = std::min(finalRes, Cap::RESISTANCE);
  }
}

// -----------------------------------------------------------------------------
// ToGPU Implementation
// -----------------------------------------------------------------------------
void AttributePipeline::ToGPU(const CombatStats &src,
                              NoMoreDay::components::GPUVisualStats &dst) {
  dst.weaponDamage = src.effective_strength;
  dst.attackSpeed = src.attack_speed;
  dst.critChance = src.crit_chance;
  dst.critDamage = src.crit_damage;
  dst.defenseRating = src.effective_armor_dr;

  float barrierRatio =
      (src.max_health > 0) ? (src.barrier / src.max_health) : 0.0f;
  dst.statusStrength = 0.0f;
  dst.glowIntensity = barrierRatio;

  // Pack Color (White default)
  uint32_t r = 255, g = 255, b = 255, a = 255;
  dst.glowColorPacked = (a << 24) | (b << 16) | (g << 8) | r;
}

} // namespace NoMoreDay