#include "game/systems/combat/StatsSystem.hpp"
#include "core/utils/FrameRateUtils.hpp" // Frame-rate independent utilities
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/ItemStats.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Progression.hpp"
#include "game/components/Stats.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/registry/GroupRegistry.hpp"       // Added
#include "game/systems/combat/CombatFormula.hpp" // Added for Formula Refactor
#include "game/systems/stats/AttributePipeline.hpp"
#include "game/utils/MonsterScaling.hpp"
#include <Taskflow/algorithm/for_each.hpp>
#include <Taskflow/taskflow.hpp>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <vector>


namespace NoMoreDay {

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

// 辅助函数：将通用 StatModifier 应用到计算结构
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

// 辅助函数：将通用 StatModifier 应用到计算数组
static void ApplyStatModifier(
    std::array<StatCalculation, static_cast<size_t>(StatType::Count)> &calcs,
    StatType type, ModifierMode mode, float value) {
  ApplyStatCalculation(calcs[static_cast<size_t>(type)], mode, value);
}

// Helper Context for Affix Dispatch

// Helper: Check if StatType is a damage scaling stat
static bool IsDamageStat(StatType type) {
  return type >= StatType::PhysicalDamage && type <= StatType::ShadowDamage;
}

// Helper: Get Tag for Damage Stat
static Tag GetTagFromDamageStat(StatType type) {
  switch (type) {
  case StatType::PhysicalDamage:
    return Tag::Physical;
  case StatType::FireDamage:
    return Tag::Fire;
  case StatType::ColdDamage:
    return Tag::Cold;
  case StatType::LightningDamage:
    return Tag::Lightning;
  case StatType::PoisonDamage:
    return Tag::Poison;
  case StatType::ShadowDamage:
    return Tag::Shadow;
  default:
    return Tag::None;
  }
}

void StatsSystem::Recalculate(entt::registry &registry, entt::entity entity) {
  ClearCache(registry, entity);
  AttributePipeline::Calculate(registry, entity);
}
float StatsSystem::GetStatWithTags(entt::registry &registry,
                                   entt::entity entity, StatType type, Tag tags,
                                   uint32_t skill_id,
                                   entt::entity source_entity) {
  auto *combat = registry.try_get<CombatStats>(entity);
  if (!combat)
    return 0.0f;

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
  
  // --- Thread-Safe Cache Read ---
  {
    std::shared_lock lock(s_cacheMutex);
    auto it_entity = s_tagStatCache.find(entity_id);
    if (it_entity != s_tagStatCache.end()) {
      auto it_stat = it_entity->second.find(key);
      if (it_stat != it_entity->second.end()) {
        return it_stat->second;
      }
    }
  }

  StatCalculation dynamic_calc;

  Tag combined_query_tags = tags;
  if (auto *stanceComp = registry.try_get<MovementStanceComponent>(entity)) {
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

  case StatType::CritChance:
    dynamic_calc.base = combat->crit_chance * 100.0f;
    break;
  case StatType::CritDamage:
    dynamic_calc.base = combat->crit_damage * 100.0f;
    break;
  case StatType::AttackSpeed:
    dynamic_calc.base = combat->attack_speed * 100.0f;
    break;
  case StatType::CastSpeed:
    dynamic_calc.base = combat->cast_speed * 100.0f;
    break;
  case StatType::Accuracy:
    dynamic_calc.base = combat->accuracy * 100.0f;
    break;
  case StatType::ManaOnHit:
    dynamic_calc.base = combat->mana_on_hit;
    break;
  case StatType::ArmorPenetration:
    dynamic_calc.base = combat->armor_pen;
    break;
  case StatType::MoveSpeed:
    dynamic_calc.base = combat->move_speed;
    break;
  case StatType::Armor:
    dynamic_calc.base = combat->armor;
    break;
  case StatType::MaxHealth:
    dynamic_calc.base = combat->max_health;
    break;
  case StatType::MaxMana:
    dynamic_calc.base = combat->max_mana;
    break;
  case StatType::CooldownReduction:
    dynamic_calc.base = combat->cooldown_reduction * 100.0f;
    break;
  case StatType::ResourceCostReduction:
    dynamic_calc.base = combat->resource_cost_reduction * 100.0f;
    break;
  case StatType::ProjectileCount:
    // If skill_id is provided, use the base count for that specific skill
    if (skill_id == 2)
      dynamic_calc.base = 1.0f; // Rending Wave base
    else
      dynamic_calc.base = 0.0f;
    break;
  case StatType::AreaScale:
    dynamic_calc.base = combat->area_scale * 100.0f;
    break;
  case StatType::ProjectileSpeed:
    dynamic_calc.base = combat->projectile_speed * 100.0f;
    break;
  case StatType::DurationScale:
    dynamic_calc.base = combat->duration_scale * 100.0f;
    break;
  case StatType::DodgeChance:
    dynamic_calc.base = combat->dodge_chance * 100.0f;
    break;
  case StatType::BlockChance:
    dynamic_calc.base = combat->block_chance * 100.0f;
    break;
  case StatType::LifeSteal:
    dynamic_calc.base = combat->life_steal * 100.0f;
    break;
  case StatType::LifeOnHit:
    dynamic_calc.base = combat->life_on_hit;
    break;
  case StatType::HealthRegen:
    dynamic_calc.base = combat->health_regen;
    break;
  case StatType::ManaRegen:
    dynamic_calc.base = combat->mana_regen;
    break;
  case StatType::Thorns:
    dynamic_calc.base = combat->thorns;
    break;
  case StatType::MagicFind:
    dynamic_calc.base = combat->magic_find;
    break;

  default:
    break;
  }

  // 2. 累加动态标签修饰符
  auto apply_if_tags_match = [&](const std::vector<StatModifier> &modifiers,
                                 float scale = 1.0f) {
    for (const auto &mod : modifiers) {
      bool type_match = (mod.type == type);

      // Conversion Inheritance: If querying a Damage Stat, also apply modifiers
      // for other Damage Types if the source tags contain that type.
      if (!type_match && IsDamageStat(type) && IsDamageStat(mod.type)) {
        Tag mod_tag = GetTagFromDamageStat(mod.type);
        if (mod_tag != Tag::None && HasTag(combined_query_tags, mod_tag)) {
          type_match = true;
        }
      }

      if (type_match) {
        bool tags_match = (mod.required_tags == Tag::None ||
                           HasTag(combined_query_tags, mod.required_tags));

        if (tags_match) {
          ApplyStatCalculation(dynamic_calc, mod.mode, mod.value * scale);
        }
      }
    }
  };

  if (auto *list = registry.try_get<ModifierList>(entity)) {
    apply_if_tags_match(list->modifiers);
  }

  if (auto *astrolabe = registry.try_get<AstrolabeComponent>(entity)) {
    const auto &reg = AstrolabeRegistry::Get();
    for (uint32_t node_id : astrolabe->activated_nodes) {
      if (const auto *node = reg.GetNode(node_id)) {
        apply_if_tags_match(node->modifiers);
      }
    }
  }

  // 2.2 处理源实体修饰符 (SkillModifierComponent on source_entity)
  if (registry.valid(source_entity)) {
    if (auto *skillMods =
            registry.try_get<SkillModifierComponent>(source_entity)) {
      apply_if_tags_match(skillMods->stat_modifiers);
    }
  }

  // 2.3 NEW: 处理条件装备词缀 (GlobalModifierComponent.stat_modifiers)
  if (auto *global = registry.try_get<GlobalModifierComponent>(entity)) {
    apply_if_tags_match(global->stat_modifiers);
  }

  // 2.4 NEW: 处理 Avenger (复仇者) 词缀加成
  if (IsDamageStat(type)) {
    if (auto *avenger = registry.try_get<AvengerComponent>(entity)) {
      // 将加成应用到 percent_add，确保是加法叠加
      dynamic_calc.percent_add += (avenger->GetDamageMultiplier() - 1.0f);
    }
  }

  // 2.5 处理 Soul Eater (噬魂) 动态加成
  if (auto *soulEater = registry.try_get<SoulEaterComponent>(entity)) {
    if (soulEater->stacks > 0) {
      if (IsDamageStat(type)) {
        dynamic_calc.percent_add +=
            (soulEater->stacks * soulEater->damagePerStack / 100.0f);
      } else if (type == StatType::AttackSpeed) {
        dynamic_calc.percent_add +=
            (soulEater->stacks * soulEater->attackSpeedPerStack / 100.0f);
      }
    }
  }

  // 3. 处理技能专精天赋 (Skill Specialization Talents)
  if (skill_id != 0) {
    if (auto *active = registry.try_get<ActiveSkillsComponent>(entity)) {
      bool found_specialized = false;
      for (const auto &specialized : active->specialized_slots) {
        if (specialized.skill_id == skill_id) {
          found_specialized = true;
          const auto *tree = SkillRegistry::Get().GetSkillTree(skill_id);
          if (tree) {
            for (auto [node_id, pts] : specialized.allocated_points) {
              auto node_it = tree->nodes.find(node_id);
              if (node_it != tree->nodes.end()) {
                apply_if_tags_match(node_it->second.stat_modifiers,
                                    static_cast<float>(pts));
              }
            }
          }
          break;
        }
      }
    }
  }

  float result = dynamic_calc.Result();
  
  // --- Thread-Safe Cache Write ---
  {
    std::unique_lock lock(s_cacheMutex);
    s_tagStatCache[entity_id][key] = result;
  }
  
  return result;
}

void StatsSystem::update(entt::registry &registry) {
  // Phase 2 Optimization: Use Owning Group traversal
  // This iterates only entities with BOTH StatsDirty and CombatStats.
  // Since they are grouped, CombatStats are accessed linearly in memory.
  auto group = registry.group<StatsDirty, CombatStats>();
  for (auto entity : group) {
    Recalculate(registry, entity);
  }
  registry.clear<StatsDirty>();
}

void StatsSystem::UpdateBuffs(entt::registry &registry, float dt) {
  auto view = registry.view<ActiveEffectsComponent>();
  for (auto entity : view) {
    auto &effects = view.get<ActiveEffectsComponent>(entity);
    size_t before = effects.effects.size();

    effects.Update(dt);

    if (effects.effects.size() != before) {
      registry.get_or_emplace<StatsDirty>(entity);
    }

    // Visuals for Status Effects
    if (registry.all_of<Position>(entity)) {
      const auto &pos = registry.get<Position>(entity);
      for (const auto &buff : effects.effects) {
        if (buff.type == BuffType::Freeze) {
          // Time-based: ~30% at 60 FPS
          if (utils::FrameRateUtils::ShouldTrigger(30.0f, dt)) {
            components::GPUParticle p;
            p.position = {pos.x + GetRandomValue(-10, 10),
                          pos.y + GetRandomValue(-10, 10)};
            p.velocity = {0, -10.0f};
            p.acceleration = {0, 0};
            p.color = SKYBLUE;
            p.lifetime = 0.5f;
            p.maxLifetime = 0.5f;
            p.scale = 1.2f;
            p.flags = 0; // Soft
            systems::GPUParticleSystem::Get().Emit(p);
          }
        } else if (buff.type == BuffType::Burn) {
          // Time-based: ~30% at 60 FPS
          if (utils::FrameRateUtils::ShouldTrigger(30.0f, dt)) {
            components::GPUParticle p;
            p.position = {pos.x + GetRandomValue(-8, 8),
                          pos.y + GetRandomValue(-5, 5)};
            p.velocity = {0, -30.0f}; // Rise fast
            p.acceleration = {0, 0};
            p.color = ORANGE;
            p.lifetime = 0.4f;
            p.maxLifetime = 0.4f;
            p.scale = 1.5f;
            p.flags = 0; // Soft
            systems::GPUParticleSystem::Get().Emit(p);
          }
        } else if (buff.type == BuffType::Stun ||
                   buff.type == BuffType::Shock) {
          // Time-based: ~20% at 60 FPS
          if (utils::FrameRateUtils::ShouldTrigger(20.0f, dt)) {
            components::GPUParticle p;
            p.position = {pos.x + GetRandomValue(-10, 10),
                          pos.y + GetRandomValue(-20, 0)};
            p.velocity = {0, 0};
            p.acceleration = {0, 0};
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

void StatsSystem::ClearCache(entt::registry &, entt::entity entity) {
  std::unique_lock lock(s_cacheMutex);
  uint32_t entity_id = static_cast<uint32_t>(entity);
  s_tagStatCache.erase(entity_id);
}

void StatsSystem::Initialize(entt::registry &registry) {
  registry.on_destroy<CombatStats>().connect<&StatsSystem::ClearCache>();
}

void StatsSystem::Shutdown(entt::registry &registry) {
  registry.on_destroy<CombatStats>().disconnect<&StatsSystem::ClearCache>();
}

void StatsSystem::Reset() { 
  std::unique_lock lock(s_cacheMutex);
  s_tagStatCache.clear(); 
}

} // namespace NoMoreDay