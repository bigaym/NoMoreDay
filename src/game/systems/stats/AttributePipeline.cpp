#include "game/systems/stats/AttributePipeline.hpp"
#include "core/utils/FrameRateUtils.hpp"
#include "game/components/AIComponent.hpp"
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
#include "game/components/WorldState.hpp"
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

  combat.health = current_health;
  combat.mana = current_mana;
  combat.barrier = current_barrier;
}

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
  if (type < StatType::Count)
    ApplyStatCalculation(calcs[static_cast<size_t>(type)], mode, value);
}

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
    bind(AffixType::Strength, StatType::Strength, ModifierMode::Flat);
    bind(AffixType::Dexterity, StatType::Dexterity, ModifierMode::Flat);
    bind(AffixType::Intelligence, StatType::Intelligence, ModifierMode::Flat);
    bind(AffixType::Vitality, StatType::Vitality, ModifierMode::Flat);
    handlers[(int)AffixType::AllAttributes] = [](AffixContext &ctx,
                                                 const Affix &a) {
      for (auto s : {StatType::Strength, StatType::Dexterity,
                     StatType::Intelligence, StatType::Vitality})
        ApplyStatModifier(ctx.calcs, s, ModifierMode::Flat, a.value);
    };
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
    bind(AffixType::CritChance, StatType::CritChance, ModifierMode::Flat);
    bind(AffixType::CritDamage, StatType::CritDamage, ModifierMode::Flat);
    bind(AffixType::AttackSpeed, StatType::AttackSpeed,
         ModifierMode::PercentAdd);
    bind(AffixType::CastSpeed, StatType::CastSpeed, ModifierMode::PercentAdd);
    bind(AffixType::Accuracy, StatType::Accuracy, ModifierMode::Flat);
    bind(AffixType::FlatArmor, StatType::Armor, ModifierMode::Flat);
    bind(AffixType::PercentArmor, StatType::Armor, ModifierMode::PercentAdd);
    bind(AffixType::FlatHealth, StatType::MaxHealth, ModifierMode::Flat);
    bind(AffixType::PercentHealth, StatType::MaxHealth,
         ModifierMode::PercentAdd);
    bind(AffixType::FlatMana, StatType::MaxMana, ModifierMode::Flat);
    bind(AffixType::ResistFire, StatType::ResistFire, ModifierMode::Flat);
    bind(AffixType::ResistCold, StatType::ResistCold, ModifierMode::Flat);
    bind(AffixType::ResistLightning, StatType::ResistLightning,
         ModifierMode::Flat);
    bind(AffixType::ResistPoison, StatType::ResistPoison, ModifierMode::Flat);
    bind(AffixType::ResistShadow, StatType::ResistShadow, ModifierMode::Flat);
    bind(AffixType::ResistAll, StatType::ResistAll, ModifierMode::Flat);
    bind(AffixType::MoveSpeed, StatType::MoveSpeed, ModifierMode::PercentAdd);
    bind(AffixType::CooldownReduction, StatType::CooldownReduction,
         ModifierMode::Flat);
    bind(AffixType::LifeOnHit, StatType::LifeOnHit, ModifierMode::Flat);
    bind(AffixType::ManaOnHit, StatType::ManaOnHit, ModifierMode::Flat);
    bind(AffixType::HealthRegen, StatType::HealthRegen, ModifierMode::Flat);
    bind(AffixType::ManaRegen, StatType::ManaRegen, ModifierMode::Flat);
    bind(AffixType::PercentHealthRegen, StatType::HealthRegen,
         ModifierMode::PercentAdd);
    bind(AffixType::PercentManaRegen, StatType::ManaRegen,
         ModifierMode::PercentAdd);
    bind(AffixType::Thorns, StatType::Thorns, ModifierMode::Flat);
    bind(AffixType::FlatDodgeRating, StatType::DodgeRating, ModifierMode::Flat);
    bind(AffixType::PercentDodgeRating, StatType::DodgeRating,
         ModifierMode::PercentAdd);
    bind(AffixType::FlatBlockRating, StatType::BlockRating, ModifierMode::Flat);
    bind(AffixType::PercentBlockRating, StatType::BlockRating,
         ModifierMode::PercentAdd);
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
    handlers[(int)AffixType::PlusAllSkills] = [](AffixContext &ctx,
                                                 const Affix &a) {
      if (auto *active =
              ctx.registry.try_get<ActiveSkillsComponent>(ctx.entity)) {
        for (auto &s : active->specialized_slots)
          if (s.skill_id != 0)
            s.bonus_levels += (int)a.value;
      }
    };
  }
  void Dispatch(AffixContext &ctx, const Affix &affix) const {
    if (static_cast<size_t>(affix.type) < TABLE_SIZE)
      handlers[static_cast<size_t>(affix.type)](ctx, affix);
  }
  static const AffixDispatcher &Get() {
    static AffixDispatcher instance;
    return instance;
  }
};

struct SetTrack {
  uint32_t hash;
  int count;
  const std::vector<SetBonus> *defs;
};

void AttributePipeline::Calculate(entt::registry &registry,
                                  entt::entity entity) {
  auto components = registry.try_get<CombatStats, GlobalModifierComponent, ActiveSkillsComponent, EquipmentComponent, PrimaryStats, MovementStanceComponent>(entity);
  auto* statsPtr = std::get<0>(components);
  if (!statsPtr) return;
  auto& stats = *statsPtr;
  
  ResetCombatStats(stats);
  
  if (registry.all_of<TitanGripTrait>(entity))
    registry.remove<TitanGripTrait>(entity);

  auto* active = std::get<2>(components);
  if (active) {
    for (auto &s : active->specialized_slots)
      s.bonus_levels = 0;
  }

  auto* global_mods_ptr = std::get<1>(components);
  if (!global_mods_ptr) {
      global_mods_ptr = &registry.emplace<GlobalModifierComponent>(entity);
  }
  auto &global_mods = *global_mods_ptr;
  global_mods.modifiers.clear();
  global_mods.stat_modifiers.clear();

  std::array<StatCalculation, static_cast<size_t>(StatType::Count)> calcs;
  using namespace NoMoreDay::Constants::Combat;
  
  bool isPlayer = registry.all_of<PlayerTag>(entity);
  bool isEnemy = registry.all_of<EnemyTag>(entity);
  
  // Initialize base values
  calcs[static_cast<size_t>(StatType::MaxHealth)].base = isPlayer ? DEFAULT_MAX_HEALTH : 1.0f;
  calcs[static_cast<size_t>(StatType::MaxMana)].base = isPlayer ? DEFAULT_MAX_MANA : 1.0f;
  calcs[static_cast<size_t>(StatType::MoveSpeed)].base = DEFAULT_MOVE_SPEED;
  calcs[static_cast<size_t>(StatType::CritChance)].base = DEFAULT_CRIT_CHANCE * 100.0f;
  calcs[static_cast<size_t>(StatType::CritDamage)].base = DEFAULT_CRIT_DAMAGE * 100.0f;
  calcs[static_cast<size_t>(StatType::AttackSpeed)].base = DEFAULT_ATTACK_SPEED * 100.0f;
  calcs[static_cast<size_t>(StatType::CastSpeed)].base = 100.0f;
  calcs[static_cast<size_t>(StatType::Accuracy)].base = DEFAULT_ACCURACY * 100.0f;
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

  float mapHpMult = 1.0f, mapDmgMult = 1.0f, mapSpeedMult = 1.0f;
  if (auto* ms = registry.ctx().find<ActiveDimensionalState>()) {
    if (ms->isActive && isEnemy) {
      mapHpMult *= (1.0f + ms->resonance.totalEnemyDensity * 0.05f);
      for (const auto &a : ms->explicitAffixes) {
        switch (a.type) {
        case MapAffixType::Enemy_ExtraHealth: mapHpMult *= (1.0f + a.value); break;
        case MapAffixType::Enemy_ExtraDamage: mapDmgMult *= (1.0f + a.value); break;
        case MapAffixType::Enemy_Fast: mapSpeedMult *= (1.0f + a.value); break;
        default: break;
        }
      }
    }
  }

  if (auto *enemy = registry.try_get<EnemyStateComponent>(entity)) {
    auto rarity = registry.all_of<EnemyRarityComponent>(entity)
                      ? registry.get<EnemyRarityComponent>(entity).rarity
                      : EnemyRarityComponent::NORMAL;
    auto scaled = MonsterScaling::Calculate(enemy->raceType, enemy->level, rarity);
    calcs[static_cast<size_t>(StatType::MaxHealth)].base = scaled.maxHealth * mapHpMult;
    calcs[static_cast<size_t>(StatType::Armor)].base = scaled.armor;
    const auto &raceData = kRaceData[static_cast<size_t>(enemy->raceType)];
    constexpr float NATIVE_RES = 50.0f;
    float bonus = scaled.resistanceBonus * 100.0f;
    
    auto applyRes = [&](Tag tag, StatType t) {
      float val = bonus + (HasTag(raceData.resistances, tag) ? NATIVE_RES : 0.0f);
      if (val > 0) ApplyStatModifier(calcs, t, ModifierMode::Flat, val);
    };
    applyRes(Tag::Physical, StatType::ResistPhysical);
    applyRes(Tag::Fire, StatType::ResistFire);
    applyRes(Tag::Cold, StatType::ResistCold);
    applyRes(Tag::Lightning, StatType::ResistLightning);
    applyRes(Tag::Poison, StatType::ResistPoison);
    applyRes(Tag::Shadow, StatType::ResistShadow);
    calcs[static_cast<size_t>(StatType::MoveSpeed)].base = raceData.baseSpeed * mapSpeedMult;
  }

  if (auto *primary = std::get<4>(components)) {
    calcs[static_cast<size_t>(StatType::Strength)].base = primary->strength;
    calcs[static_cast<size_t>(StatType::Dexterity)].base = primary->dexterity;
    calcs[static_cast<size_t>(StatType::Intelligence)].base = primary->intelligence;
    calcs[static_cast<size_t>(StatType::Vitality)].base = primary->vitality;
  }

  bool hasTitanGrip = false;
  AffixContext ctx{calcs, stats, registry, entity, hasTitanGrip, global_mods};
  auto procAff = [&](const std::vector<Affix> &affs) {
    for (const auto &a : affs) {
      if (a.required_tags != Tag::None) {
        global_mods.stat_modifiers.push_back({
            .value = a.value,
            .type = static_cast<StatType>(a.type),
            .mode = ModifierMode::PercentAdd,
            .required_tags = a.required_tags,
            .source = ModifierSource::Item
        });
      } else {
        AffixDispatcher::Get().Dispatch(ctx, a);
      }
    }
  };

  Tag etags = Tag::None;
  if (auto *stance = std::get<5>(components)) {
    if (stance->stance == MovementStance::SwordRiding)
      etags = etags | Tag::SwordRiding;
  }
  static thread_local std::vector<SetTrack> s_set_scratch;
  s_set_scratch.clear();
  bool hasMain = false, hasOff = false, is2H = false;
  float mainAtk = 0, offAtk = 0;
  
  if (auto *eq = std::get<3>(components)) {
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
        procAff(item.implicits);
        procAff(item.affixes);
        if (item.defense > 0)
          ApplyStatModifier(calcs, StatType::Armor, ModifierMode::Flat,
                            item.defense);
        for (auto r : item.sockets) {
          if (registry.valid(r) && registry.all_of<RuneComponent>(r)) {
            const auto &rune = registry.get<RuneComponent>(r);
            if (item.type == ItemType::Weapon)
              procAff(rune.weaponEffects);
            else if (item.slot == EquipmentSlot::Neck ||
                     item.slot == EquipmentSlot::Ring1 ||
                     item.slot == EquipmentSlot::Ring2)
              procAff(rune.jewelryEffects);
            else
              procAff(rune.armorEffects);
          }
        }
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
  for (const auto &t : s_set_scratch) {
    if (t.defs) {
      for (const auto &sb : *t.defs) {
        if (t.count >= sb.requiredCount)
          procAff(sb.bonuses);
      }
    }
  }

  if (hasMain) {
    if (hasOff) {
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
  } else if (std::get<3>(components)) {
    stats.min_weapon_damage = 2.0f;
    stats.max_weapon_damage = 3.0f;
    stats.knockback = 10.0f;
  } else if (auto *wc = registry.try_get<WeaponComponent>(entity)) {
    stats.min_weapon_damage = wc->damage;
    stats.max_weapon_damage = wc->damage;
  } else if (auto *en = registry.try_get<EnemyStateComponent>(entity)) {
    float bd = kRaceData[static_cast<size_t>(en->raceType)].baseDamage *
               (1.0f + en->level * 0.1f);
    stats.min_weapon_damage = bd * 0.9f * mapDmgMult;
    stats.max_weapon_damage = bd * 1.1f * mapDmgMult;
  }

  if (auto *se = registry.try_get<SoulEaterComponent>(entity)) {
    if (se->stacks > 0) {
      float db = se->stacks * se->damagePerStack;
      float ab = se->stacks * se->attackSpeedPerStack;
      for (int i = 0; i < 6; ++i)
        ApplyStatModifier(calcs,
                          static_cast<StatType>(
                              static_cast<int>(StatType::PhysicalDamage) + i),
                          ModifierMode::PercentAdd, db);
      ApplyStatModifier(calcs, StatType::AttackSpeed, ModifierMode::PercentAdd,
                        ab);
    }
  }
  if (auto *av = registry.try_get<AvengerComponent>(entity)) {
    if (av->avengerStacks > 0) {
      float db = av->avengerStacks * av->damagePerStack * 100.0f;
      for (int i = 0; i < 6; ++i)
        ApplyStatModifier(calcs,
                          static_cast<StatType>(
                              static_cast<int>(StatType::PhysicalDamage) + i),
                          ModifierMode::PercentAdd, db);
    }
  }
  if (auto *list = registry.try_get<ModifierList>(entity)) {
    for (const auto &m : list->modifiers) {
      if (m.required_tags == Tag::None || HasTag(etags, m.required_tags))
        ApplyStatModifier(calcs, m.type, m.mode, m.value);
    }
  }
  if (auto *ac = registry.try_get<MonsterAffixComponent>(entity)) {
    for (auto at : ac->affixes) {
      const auto &def = MonsterAffixRegistry::GetAffixDef(at);
      for (int i = 0; i < def.statModCount; ++i)
        ApplyStatModifier(calcs, def.statMods[i].type, def.statMods[i].mode,
                          def.statMods[i].value);
      if (ac->isBerserk) {
        stats.min_weapon_damage *= 2.0f;
        stats.max_weapon_damage *= 2.0f;
      }
    }
  }
  if (active) {
    for (const auto &s : active->specialized_slots) {
      if (s.skill_id == 0)
        continue;
      const auto *tr = SkillRegistry::Get().GetSkillTree(s.skill_id);
      if (!tr)
        continue;
      for (const auto &[nid, pts] : s.allocated_points) {
        if (pts <= 0)
          continue;
        auto nit = tr->nodes.find(nid);
        if (nit == tr->nodes.end())
          continue;
        for (const auto &m : nit->second.stat_modifiers) {
          if (m.required_tags == Tag::None)
            ApplyStatModifier(calcs, m.type, m.mode,
                              m.value * static_cast<float>(pts));
        }
      }
    }
  }
  if (auto *as = registry.try_get<AstrolabeComponent>(entity)) {
    // New system: use nodePoints map
    if (!as->nodePoints.empty()) {
        for (const auto& [nid, points] : as->nodePoints) {
            if (points <= 0) continue;
            if (const auto *n = AstrolabeRegistry::Get().GetNode(nid)) {
                for (const auto &m : n->modifiers) {
                    if (m.required_tags == Tag::None)
                        ApplyStatModifier(calcs, m.type, m.mode, m.value * static_cast<float>(points));
                }
            }
        }
    } else {
        // Fallback for legacy data (activated_nodes set)
        for (uint32_t nid : as->activated_nodes) {
            if (const auto *n = AstrolabeRegistry::Get().GetNode(nid)) {
                for (const auto &m : n->modifiers) {
                    if (m.required_tags == Tag::None)
                        ApplyStatModifier(calcs, m.type, m.mode, m.value);
                }
            }
        }
    }
  }

  float str = calcs[static_cast<size_t>(StatType::Strength)].Result();
  float dex = calcs[static_cast<size_t>(StatType::Dexterity)].Result();
  float intel = calcs[static_cast<size_t>(StatType::Intelligence)].Result();
  float vit = calcs[static_cast<size_t>(StatType::Vitality)].Result();
  stats.effective_strength = str;
  stats.effective_dexterity = dex;
  stats.effective_intelligence = intel;
  stats.effective_vitality = vit;
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

  int al = (registry.all_of<EnemyStateComponent>(entity))
               ? registry.get<EnemyStateComponent>(entity).level
               : 1;
  if (auto *p = registry.try_get<PlayerStats>(entity))
    al = std::max(1, p->level);
  stats.cached_area_level = al;
  stats.max_health = calcs[static_cast<size_t>(StatType::MaxHealth)].Result();
  stats.max_mana = calcs[static_cast<size_t>(StatType::MaxMana)].Result();
  stats.armor = calcs[static_cast<size_t>(StatType::Armor)].Result();
  stats.max_barrier +=
      calcs[static_cast<size_t>(StatType::MaxBarrier)].Result();
  stats.barrier_regen +=
      calcs[static_cast<size_t>(StatType::BarrierRegen)].Result();
  stats.barrier_decay =
      calcs[static_cast<size_t>(StatType::BarrierDecay)].Result() / 100.0f;
  stats.barrier_delay =
      calcs[static_cast<size_t>(StatType::BarrierDelay)].Result();
  stats.barrier_retention =
      calcs[static_cast<size_t>(StatType::BarrierRetention)].Result() / 100.0f +
      intel * Attribute::INT_TO_BARRIER_RETENTION / 100.0f;
  stats.dodge_rating =
      calcs[static_cast<size_t>(StatType::DodgeRating)].Result();
  stats.block_rating =
      calcs[static_cast<size_t>(StatType::BlockRating)].Result();
  stats.effective_dodge =
      CombatFormula::CalculateDodgeChance(stats.dodge_rating, al) +
      calcs[static_cast<size_t>(StatType::DodgeChance)].Result();
  stats.dodge_chance =
      std::min(stats.effective_dodge, Scaling::DODGE_MAX_CHANCE);
  stats.effective_dodge = stats.dodge_chance;
  stats.block_chance =
      std::min(calcs[static_cast<size_t>(StatType::BlockChance)].Result(),
               Scaling::BLOCK_MAX_CHANCE);
  stats.block_amount = stats.block_rating;
  stats.effective_block_eff =
      CombatFormula::CalculateBlockEffectiveness(stats.block_rating, al);
  stats.effective_armor_dr =
      1.0f - CombatFormula::CalculateArmorMultiplier(stats.armor, al);
  stats.damage_reduction =
      calcs[static_cast<size_t>(StatType::GlobalDamageReduction)].Result() /
      100.0f;
  if (hasTitanGrip)
    registry.emplace<TitanGripTrait>(entity);
  if (auto *hp = registry.try_get<HealthComponent>(entity)) {
    hp->max = stats.max_health;
    if (hp->current > hp->max)
      hp->current = hp->max;
    stats.health = hp->current;
  }
  if (stats.max_barrier > 0 || stats.barrier > 0)
    (void)registry.get_or_emplace<BarrierComponent>(entity);
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
  for (int i = 0; i < 6; ++i) {
    auto &c = calcs[static_cast<size_t>(StatType::PhysicalDamage) + i];
    stats.damage_multipliers[i] = c.Result() / 100.0f;
    stats.damage_percent_add[i] = c.percent_add;
    stats.damage_percent_mult_component[i] = c.percent_mult;
  }
  float rall = calcs[static_cast<size_t>(StatType::ResistAll)].Result();
  for (int i = 0; i < 6; ++i) {
    float fr =
        (calcs[static_cast<size_t>(StatType::ResistPhysical) + i].Result() +
         rall) /
        100.0f;
    stats.raw_resistances[i] = fr;
    stats.resistances[i] = std::min(fr, Cap::RESISTANCE);
  }
}

void AttributePipeline::ToGPU(const CombatStats &src,
                              NoMoreDay::components::GPUVisualStats &dst) {
  dst.weaponDamage = (src.min_weapon_damage + src.max_weapon_damage) * 0.5f;
  dst.attackSpeed = src.attack_speed;
  dst.critChance = src.crit_chance;
  dst.critDamage = src.crit_damage;
  dst.defenseRating = src.effective_armor_dr;
  float br = (src.max_health > 0) ? (src.barrier / src.max_health) : 0.0f;
  dst.statusStrength = 0.0f;
  dst.glowIntensity = br;
  uint32_t r = 255, g = 255, b = 255, a = 255;
  dst.glowColorPacked = (a << 24) | (b << 16) | (g << 8) | r;
}

} // namespace NoMoreDay
