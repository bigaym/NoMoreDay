#include "game/systems/combat/DamagePipeline.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "game/components/AdvancedAffixComponents.hpp" // InvulnerableComponent, SuppressorComponent
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp" // PhantomFlashComponent
#include "game/components/Projectile.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/CombatAntiMeta.hpp"
#include "game/systems/combat/CombatFormula.hpp" // Added
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/CombatTelemetry.hpp"
#include "game/systems/combat/EndgameModifierContract.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp" // ShadowCast
#include "spdlog/spdlog.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <xsimd/xsimd.hpp>

namespace NoMoreDay {

#ifndef COMBAT_DEFENSE_DEBUG
#define COMBAT_DEFENSE_DEBUG 0
#endif

#if COMBAT_DEFENSE_DEBUG
#define COMBAT_DEFENSE_LOG(...) spdlog::debug(__VA_ARGS__)
#else
#define COMBAT_DEFENSE_LOG(...) ((void)0)
#endif

namespace {

struct DefenseResolution {
  bool dodged = false;
  bool blocked = false;
  float block_multiplier = 1.0f;
  float effective_dodge = 0.0f;
  float block_effectiveness = 0.0f;
  float block_amount = 0.0f;
};

float ClampMoreToMultiplier(float more) {
  return std::max(0.0f, 1.0f + more);
}

bool ShouldResolveDefenseRolls(Tag hit_tags, bool skip_mitigation,
                               bool is_simulation) {
  if (skip_mitigation || is_simulation) {
    return false;
  }

  if (!HasTag(hit_tags, Tag::Hit)) {
    return false;
  }

  return !HasTag(hit_tags, Tag::DamageOverTime);
}

float ResolveBlockEffectiveness(const CombatStats &defender_stats) {
  float block_effectiveness = defender_stats.effective_block_eff;
  if (block_effectiveness <= 0.0f && defender_stats.block_amount > 0.0f) {
    const int area_level = (std::max)(1, defender_stats.cached_area_level);
    block_effectiveness = CombatFormula::CalculateBlockEffectiveness(
        defender_stats.block_amount, area_level);
  }
  return std::clamp(block_effectiveness, 0.0f, 1.0f);
}

DefenseResolution ResolveDefenseResolution(
    entt::registry &registry, entt::entity attacker, entt::entity defender,
    const CombatStats *attacker_stats, const CombatStats *defender_stats,
    Tag hit_tags, bool skip_mitigation, bool is_simulation,
    bool dispatch_events) {
  DefenseResolution resolution;
  if (!defender_stats || !ShouldResolveDefenseRolls(hit_tags, skip_mitigation,
                                                    is_simulation)) {
    return resolution;
  }

  if (defender_stats->dodge_chance <= 0.0f &&
      defender_stats->block_chance <= 0.0f) {
    return resolution;
  }

  const float attacker_accuracy = attacker_stats ? attacker_stats->accuracy : 1.0f;
  resolution.effective_dodge = std::clamp(
      defender_stats->dodge_chance - (attacker_accuracy - 1.0f), 0.0f, 1.0f);
  COMBAT_DEFENSE_LOG(
      "[DefenseChain] step=1 attacker={} defender={} dodgeChance={:.4f}",
      static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
      resolution.effective_dodge);

  if (resolution.effective_dodge > 0.0f &&
      utils::ThreadSafeRandom::GetFloat01() < resolution.effective_dodge) {
    resolution.dodged = true;
    COMBAT_DEFENSE_LOG(
        "[DefenseChain] step=1 attacker={} defender={} dodged=true",
        static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender));

    if (dispatch_events) {
      CombatEventDispatcher::Dispatch(
          registry, CombatEventFactory::CreateOnDodge(defender, attacker));
    }
    return resolution;
  }

  const float block_chance = std::clamp(defender_stats->block_chance, 0.0f, 1.0f);
  COMBAT_DEFENSE_LOG(
      "[DefenseChain] step=2 attacker={} defender={} blockChance={:.4f}",
      static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
      block_chance);

  if (block_chance > 0.0f &&
      utils::ThreadSafeRandom::GetFloat01() < block_chance) {
    resolution.blocked = true;
    resolution.block_amount = defender_stats->block_amount;
    resolution.block_effectiveness = ResolveBlockEffectiveness(*defender_stats);
    resolution.block_multiplier = 1.0f - resolution.block_effectiveness;

    COMBAT_DEFENSE_LOG(
        "[DefenseChain] step=2 attacker={} defender={} blocked=true "
        "blockMultiplier={:.4f}",
        static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
        resolution.block_multiplier);

    if (dispatch_events) {
      CombatEventDispatcher::Dispatch(
          registry, CombatEventFactory::CreateOnBlock(
                        defender, attacker, resolution.block_amount));
    }
  }

  return resolution;
}

struct SummonAttributionTuple {
  entt::entity owner = entt::null;
  entt::entity summon = entt::null;
  uint32_t source_skill_id = 0;

  [[nodiscard]] bool IsValid() const {
    return owner != entt::null && summon != entt::null && source_skill_id != 0;
  }
};

SummonAttributionTuple ResolveSummonAttribution(const entt::registry &registry,
                                                entt::entity attacker,
                                                entt::entity source_entity,
                                                uint32_t skill_id) {
  auto fromContext = [&](entt::entity entity) -> SummonAttributionTuple {
    if (!registry.valid(entity)) {
      return {};
    }
    if (const auto *ctx = registry.try_get<SummonAttributionContext>(entity)) {
      if (registry.valid(ctx->owner) && registry.valid(ctx->summon) &&
          ctx->source_skill_id != 0) {
        return {ctx->owner, ctx->summon, ctx->source_skill_id};
      }
    }
    if (const auto *summon = registry.try_get<SummonComponent>(entity)) {
      if (registry.valid(summon->owner)) {
        return {summon->owner, entity,
                summon->skill_id != 0 ? summon->skill_id : skill_id};
      }
    }
    return {};
  };

  if (const auto direct = fromContext(attacker); direct.IsValid()) {
    return direct;
  }
  if (const auto sourceDirect = fromContext(source_entity); sourceDirect.IsValid()) {
    return sourceDirect;
  }

  if (registry.valid(source_entity)) {
    if (const auto *proj = registry.try_get<Projectile>(source_entity)) {
      if (const auto ownerContext = fromContext(proj->owner);
          ownerContext.IsValid()) {
        return ownerContext;
      }
    }
  }

  return {};
}

void AttachSummonAttributionIfAny(CombatEvent &event,
                                  const SummonAttributionTuple &attribution) {
  if (attribution.IsValid()) {
    CombatEventFactory::AttachSummonAttribution(event, attribution.owner,
                                                attribution.summon,
                                                attribution.source_skill_id);
  }
}

} // namespace

class ScopedDamageTelemetryTimer {
public:
  ScopedDamageTelemetryTimer() noexcept {
#if COMBAT_TELEMETRY_ENABLED
    m_enabled = CombatTelemetry::Get().IsRuntimeEnabled();
    if (m_enabled) {
      m_start = Clock::now();
    }
#endif
  }

  ~ScopedDamageTelemetryTimer() {
#if COMBAT_TELEMETRY_ENABLED
    if (!m_enabled) {
      return;
    }
    const auto elapsed_us = std::chrono::duration<double, std::micro>(
        Clock::now() - m_start).count();
    CombatTelemetry::Get().RecordDamagePipelineDurationUs(elapsed_us);
#endif
  }

private:
#if COMBAT_TELEMETRY_ENABLED
  using Clock = std::chrono::steady_clock;
  Clock::time_point m_start{};
  bool m_enabled = false;
#endif
};

// Simple fixed-capacity vector helper to avoid allocations
template <typename T, size_t N> struct FixedVector {
  std::array<T, N> data;
  size_t size = 0;

  void push_back(const T &value) {
    if (size < N) {
      data[size++] = value;
    } else {
      spdlog::warn("FixedVector overflow! Capacity: {}", N);
    }
  }

  T &operator[](size_t index) { return data[index]; }
  const T &operator[](size_t index) const { return data[index]; }

  T *begin() { return data.data(); }
  T *end() { return data.data() + size; }
  const T *begin() const { return data.data(); }
  const T *end() const { return data.data() + size; }

  bool empty() const { return size == 0; }
  void clear() { size = 0; }
};

uint64_t ResolveCastIdFromSourceEntity(const entt::registry &registry,
                                       entt::entity source_entity) {
  if (!registry.valid(source_entity)) {
    return 0;
  }
  if (const auto *exec = registry.try_get<SkillExecution>(source_entity)) {
    return exec->cast_id;
  }
  if (const auto *proj = registry.try_get<Projectile>(source_entity)) {
    return proj->cast_id;
  }
  if (const auto *array = registry.try_get<SwordArrayComponent>(source_entity)) {
    return array->cast_id;
  }
  if (const auto *chan = registry.try_get<ChannelingComponent>(source_entity)) {
    return chan->cast_id;
  }
  return 0;
}

DamageResult
DamagePipeline::Calculate(entt::registry &registry, entt::entity attacker,
                          entt::entity defender, uint32_t skill_id,
                          const DamagePool &base_pool, Tag additional_tags,
                          entt::entity source_entity, bool is_simulation) {
  DamageRequest request;
  request.attacker = attacker;
  request.defender = defender;
  request.skill_id = skill_id;
  request.base_pool = base_pool;
  request.additional_tags = additional_tags;
  request.source_entity = source_entity;
  request.is_simulation = is_simulation;
  return Calculate(registry, request);
}

DamageResult DamagePipeline::Calculate(entt::registry &registry,
                                       const DamageRequest &request) {
  ScopedDamageTelemetryTimer telemetryTimer;

  const entt::entity attacker = request.attacker;
  const entt::entity defender = request.defender;
  const uint32_t skill_id = request.skill_id;
  const DamagePool &base_pool = request.base_pool;
  const Tag additional_tags = request.additional_tags;
  const entt::entity source_entity = request.source_entity;
  const bool is_simulation = request.is_simulation;
  const bool thorns_like_damage = request.thorns_like_damage;
  const bool skip_mitigation =
      request.skip_mitigation || request.thorns_like_damage;

  // === PRE-CALCULATION INTERCEPTORS ===

  // 1. Invulnerable Check (Shielding, Clone Invulnerability)
  if (!is_simulation && registry.valid(defender)) {
    if (registry.all_of<InvulnerableComponent>(defender)) {
      // Defender is invulnerable, negate all damage
      DamageResult result;
      result.total_damage = 0.0f;
      result.is_crit = false;
      return result;
    }
  }

  // 2. Suppressor Check (Distance-based damage reduction)
  float suppressor_multiplier = 1.0f;
  if (registry.valid(attacker) && registry.valid(defender)) {
    if (auto *suppressor = registry.try_get<SuppressorComponent>(defender)) {
      // Calculate distance between attacker and defender
      auto *attPos = registry.try_get<Position>(attacker);
      auto *defPos = registry.try_get<Position>(defender);

      if (attPos && defPos) {
        float dx = attPos->x - defPos->x;
        float dy = attPos->y - defPos->y;
        float distance = std::sqrt(dx * dx + dy * dy);

        // If attacker is beyond threshold, apply damage reduction
        if (distance > suppressor->threshold) {
          suppressor_multiplier = 1.0f - suppressor->damageReduction;
        }
      }
    }
  }

  const auto *skill_data = SkillRegistry::Get().GetSkill(skill_id);
  if (!skill_data) {
    bool empty_pool = true;
    for (float v : base_pool.values)
      if (v > 0.0f) {
        empty_pool = false;
        break;
      }

    if (empty_pool) {
      spdlog::warn("DamagePipeline: Calculating damage for invalid skill ID {} "
                   "with empty base pool. Result will be 0.",
                   skill_id);
    }
  }
  const float skill_added_effectiveness =
      skill_data ? (std::max)(0.0f, skill_data->added_damage_effectiveness)
                 : 1.0f;
  const float added_effectiveness =
      (std::max)(0.0f, request.added_effectiveness) * skill_added_effectiveness;
  float trigger_effectiveness = (std::max)(0.0f, request.trigger_effectiveness);
  const uint64_t source_cast_id =
      ResolveCastIdFromSourceEntity(registry, source_entity);
  if (source_cast_id != 0) {
    trigger_effectiveness *=
        SkillSystem::GetTriggerEffectivenessForCast(source_cast_id);
  } else if (registry.valid(source_entity)) {
    if (const auto *exec = registry.try_get<SkillExecution>(source_entity)) {
      trigger_effectiveness *= (std::max)(0.0f, exec->trigger_effectiveness);
    }
  }
  Tag skill_tags = skill_data ? skill_data->tags : Tag::None;
  Tag combined_hit_tags = skill_tags | additional_tags;
  const SummonAttributionTuple summon_attribution =
      ResolveSummonAttribution(registry, attacker, source_entity, skill_id);
  auto can_apply_scope = [&](ScopePolicy scope, uint32_t source_skill_id) {
    switch (scope) {
    case ScopePolicy::SkillOnly:
      return source_skill_id == skill_id;
    case ScopePolicy::GlobalAlways:
      return true;
    case ScopePolicy::GlobalWhileBuffActive:
      if (const auto *chan = registry.try_get<ChannelingComponent>(attacker)) {
        if (chan->skill_id == source_skill_id) {
          return true;
        }
      }
      if (source_skill_id == 9) {
        if (const auto *pf = registry.try_get<PhantomFlashComponent>(attacker)) {
          return pf->counter_window > 0.0f && !pf->triggered;
        }
      }
      return false;
    default:
      return false;
    }
  };
  auto is_keystone_excluded =
      [&](const SpecializedSkill &specialized, uint32_t source_skill_id,
          uint32_t node_id, const NodeContractData *node_contract) -> bool {
    if (!node_contract || node_contract->keystone_exclusion_group == 0) {
      return false;
    }
    uint32_t selected_node = 0;
    for (const auto &[candidate_node_id, points] : specialized.allocated_points) {
      if (points <= 0) {
        continue;
      }
      const auto *candidate_contract = SkillRegistry::Get().GetNodeContract(
          source_skill_id, candidate_node_id);
      if (!candidate_contract ||
          candidate_contract->keystone_exclusion_group !=
              node_contract->keystone_exclusion_group) {
        continue;
      }
      if (selected_node == 0 || candidate_node_id < selected_node) {
        selected_node = candidate_node_id;
      }
    }
    return selected_node != 0 && selected_node != node_id;
  };

  // Optimization: Access modifiers directly instead of copying to a vector
  auto *global_mods = registry.try_get<GlobalModifierComponent>(attacker);

  auto *attacker_stats = registry.try_get<CombatStats>(attacker);
  auto *defender_stats = registry.try_get<CombatStats>(defender);
  const DefenseResolution defense_resolution = ResolveDefenseResolution(
      registry, attacker, defender, attacker_stats, defender_stats,
      combined_hit_tags, skip_mitigation, is_simulation, !is_simulation);
  auto &endgameRegistry = systems::EndgameModifierRegistry::Get();
  (void)endgameRegistry.EnsureLoaded();
  const auto endgameResolution =
      endgameRegistry.ResolveForEntities(registry, attacker, defender);
  const auto &endgame = endgameResolution.aggregate;
  const float endgameDamageMoreMultiplier =
      ClampMoreToMultiplier(endgame.outgoing_damage_more);
  const float endgameDamageTakenMultiplier =
      ClampMoreToMultiplier(endgame.incoming_damage_taken_more);

  if (defense_resolution.dodged) {
    DamageResult dodged_result;
    dodged_result.was_dodged = true;
    dodged_result.block_multiplier = defense_resolution.block_multiplier;
    COMBAT_DEFENSE_LOG(
        "[DefenseChain] attacker={} defender={} step=6 hpDamage=0.0000 "
        "(dodged)",
        static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender));
    return dodged_result;
  }

  // Initial Instances from Base Pool
  struct Instance {
    float amount;
    Tag tags;
    Tag final_type;
  };

  using namespace NoMoreDay::Constants::Combat::Pipeline;
  FixedVector<Instance, MAX_INSTANCES> instances;

  // 1. Add instances from provided base_pool
  using namespace NoMoreDay::Constants::Combat::Pipeline;
  for (int i = 0; i < DAMAGE_POOL_SIZE; ++i) {
    if (base_pool.values[i] > 0.0f) {
      Tag type_tag = static_cast<Tag>(1ULL << i);
      instances.push_back(
          {base_pool.values[i], type_tag | combined_hit_tags, type_tag});
    }
  }

  // 2. Add Skill Base Damage
  if (skill_data) {
    // Calculate Weapon Damage part
    float min_w = attacker_stats ? attacker_stats->min_weapon_damage : 0.0f;
    float max_w = attacker_stats ? attacker_stats->max_weapon_damage : 0.0f;
    float weapon_avg = (min_w + max_w) * 0.5f;

    float base_dmg =
        skill_data->base_damage + (weapon_avg * skill_data->weapon_damage_mult);

    // Find the primary damage type of the skill
    using namespace NoMoreDay::Constants::Combat::Pipeline;
    Tag primary_type = Tag::Physical;
    for (int i = 0; i < ELEMENTAL_TYPE_COUNT; ++i) {
      Tag t = static_cast<Tag>(1ULL << i);
      if (HasTag(skill_data->tags, t)) {
        primary_type = t;
        break;
      }
    }

    if (base_dmg > 0.0f) {
      instances.push_back(
          {base_dmg, primary_type | combined_hit_tags, primary_type});
    }
  }

  // 3. Add flat damage from stats with Added Effectiveness scaling.
  if (attacker_stats && added_effectiveness > 0.0f) {
    for (int i = 0; i < ELEMENTAL_TYPE_COUNT; ++i) {
      const float scaled_added = attacker_stats->flat_damage[i] * added_effectiveness;
      if (scaled_added <= 0.0f) {
        continue;
      }
      const Tag type_tag = static_cast<Tag>(1ULL << i);
      instances.push_back({scaled_added, type_tag | combined_hit_tags, type_tag});
    }
  }

  // 4. Conversion and Gain Logic (Ordered Chain)
  // Physical(0) -> Lightning(3) -> Cold(2) -> Fire(1) -> Poison(4) -> Shadow(5)
  using namespace Constants::Combat::Conversion;

  for (int source_idx : CONVERSION_ORDER) {
    Tag current_source_type = static_cast<Tag>(1ULL << source_idx);

    // Aggregate modifiers for this source type
    FixedVector<const DamageModifier *, 16> conv_mods;
    FixedVector<const DamageModifier *, 16> gain_mods;
    float total_conv_pct = 0.0f;

    auto ProcessMod = [&](const DamageModifier &mod) {
      if (mod.source_tag == current_source_type &&
          mod.target_tag != Tag::None) {
        int target_idx =
            std::countr_zero(static_cast<uint64_t>(mod.target_tag));
        if (IsValidConversion(source_idx, target_idx)) {
          if (mod.type == ModifierType::Convert) {
            conv_mods.push_back(&mod);
            total_conv_pct += mod.value;
          } else if (mod.type == ModifierType::GainExtra) {
            gain_mods.push_back(&mod);
          }
        } else if (mod.target_tag != current_source_type) {
          spdlog::warn("DamagePipeline: Illegal conversion loop detected ({} "
                       "-> {}). Skipping.",
                       source_idx, target_idx);
        }
      }
    };

    // Modifier sources
    if (global_mods)
      for (const auto &mod : global_mods->modifiers)
        ProcessMod(mod);
    if (registry.valid(source_entity)) {
      if (auto *sm = registry.try_get<SkillModifierComponent>(source_entity))
        for (const auto &mod : sm->damage_modifiers)
          ProcessMod(mod);
    }

    // Talent modifiers
    if (auto *active = registry.try_get<ActiveSkillsComponent>(attacker)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == 0) {
          continue;
        }
        const uint32_t source_skill_id = spec.skill_id;
        if (const auto *tree = SkillRegistry::Get().GetSkillTree(source_skill_id)) {
          for (auto [node_id, pts] : spec.allocated_points) {
            if (pts <= 0 || !tree->nodes.contains(node_id)) {
              continue;
            }
            ScopePolicy scope = ScopePolicy::SkillOnly;
            const NodeContractData *node_contract =
                SkillRegistry::Get().GetNodeContract(source_skill_id, node_id);
            if (node_contract) {
              scope = node_contract->scope_policy;
            }
            if (!can_apply_scope(scope, source_skill_id)) {
              continue;
            }
            if (node_contract &&
                node_contract->role == SpecNodeRole::Transmuter) {
              const uint32_t active_transmuter =
                  SkillSystem::GetActiveTransmuterNode(registry, attacker,
                                                       source_skill_id);
              if (active_transmuter != 0 && active_transmuter != node_id) {
                continue;
              }
            }
            if (is_keystone_excluded(spec, source_skill_id, node_id,
                                     node_contract)) {
              continue;
            }
            for (const auto &mod : tree->nodes.at(node_id).damage_modifiers) {
              ProcessMod(mod);
            }
          }
        }
      }
    }

    if (conv_mods.empty() && gain_mods.empty())
      continue;

    float conv_scale = (total_conv_pct > 1.0f) ? (1.0f / total_conv_pct) : 1.0f;
    size_t current_count = instances.size;

    for (size_t i = 0; i < current_count; ++i) {
      if (instances[i].final_type == current_source_type) {
        float original_amount = instances[i].amount;
        if (original_amount <= 0.0f)
          continue;

        // Gain Extra
        for (auto *mod : gain_mods) {
          instances.push_back({original_amount * mod->value,
                               instances[i].tags | mod->target_tag,
                               mod->target_tag});
        }

        // Convert
        float actual_conv_total = 0.0f;
        for (auto *mod : conv_mods) {
          float amount_to_convert = original_amount * mod->value * conv_scale;
          if (amount_to_convert > 0.0f) {
            instances.push_back({amount_to_convert,
                                 instances[i].tags | mod->target_tag,
                                 mod->target_tag});
            actual_conv_total += amount_to_convert;
          }
        }
        instances[i].amount -= actual_conv_total;
      }
    }
  }

  // 3 & 4. Apply Multipliers (Dynamic via StatsSystem)
  DamageResult result;
  result.was_blocked = defense_resolution.blocked;
  result.block_multiplier = defense_resolution.block_multiplier;
  float total_final_damage = 0.0f;

  using namespace NoMoreDay::Constants::Combat::Pipeline;
  float shadow_multiplier = 1.0f;

  if (!thorns_like_damage) {
    if (auto *sc = registry.try_get<ShadowComponent>(attacker)) {
      shadow_multiplier = sc->damage_scale;
    } else if (registry.all_of<ShadowCloneComponent>(attacker)) {
      shadow_multiplier = SHADOW_MULTIPLIER;
    }
  }

  for (size_t i = 0; i < instances.size; ++i) {
    auto &inst = instances[i];
    if (inst.amount <= 0.0f)
      continue;

    if (!thorns_like_damage) {
      inst.amount *= shadow_multiplier;

      StatType dmg_stat = StatType::PhysicalDamage;
      switch (inst.final_type) {
      case Tag::Physical:
        dmg_stat = StatType::PhysicalDamage;
        break;
      case Tag::Fire:
        dmg_stat = StatType::FireDamage;
        break;
      case Tag::Cold:
        dmg_stat = StatType::ColdDamage;
        break;
      case Tag::Lightning:
        dmg_stat = StatType::LightningDamage;
        break;
      case Tag::Poison:
        dmg_stat = StatType::PoisonDamage;
        break;
      case Tag::Shadow:
        dmg_stat = StatType::ShadowDamage;
        break;
      default:
        break;
      }

      float multiplier_pct = StatsSystem::GetStatWithTags(
          registry, attacker, dmg_stat, inst.tags, skill_id, source_entity);
      inst.amount *= (multiplier_pct / 100.0f);

      // Final More accumulation per instance
      float final_more = 1.0f;
      struct MoreBucket {
        Tag source_tag = Tag::None;
        float actual = 0.0f;
      };
      FixedVector<MoreBucket, 8> cost_affix_buckets;
      auto accumulate_cost_affix_bucket = [&](Tag source_tag, float value) {
        if (value <= 0.0f) {
          return;
        }
        for (size_t idx = 0; idx < cost_affix_buckets.size; ++idx) {
          if (static_cast<uint64_t>(cost_affix_buckets[idx].source_tag) ==
              static_cast<uint64_t>(source_tag)) {
            cost_affix_buckets[idx].actual += value;
            return;
          }
        }
        cost_affix_buckets.push_back({source_tag, value});
      };

      if (global_mods) {
        for (const auto &dmod : global_mods->modifiers) {
          if (dmod.type == ModifierType::More &&
              (dmod.source_tag == Tag::None ||
               HasTag(inst.tags, dmod.source_tag))) {
            final_more *= (1.0f + dmod.value);
          }
        }
      }
      if (auto *active = registry.try_get<ActiveSkillsComponent>(attacker)) {
        for (const auto &specialized : active->specialized_slots) {
          if (specialized.skill_id == 0) {
            continue;
          }
          const uint32_t source_skill_id = specialized.skill_id;
          if (const auto *tree =
                  SkillRegistry::Get().GetSkillTree(source_skill_id)) {
            for (auto [node_id, pts] : specialized.allocated_points) {
              if (pts <= 0 || !tree->nodes.contains(node_id)) {
                continue;
              }
              ScopePolicy scope = ScopePolicy::SkillOnly;
              const NodeContractData *node_contract =
                  SkillRegistry::Get().GetNodeContract(source_skill_id,
                                                       node_id);
              if (node_contract) {
                scope = node_contract->scope_policy;
              }
              if (!can_apply_scope(scope, source_skill_id)) {
                continue;
              }
              if (node_contract &&
                  node_contract->role == SpecNodeRole::Transmuter) {
                const uint32_t active_transmuter =
                    SkillSystem::GetActiveTransmuterNode(registry, attacker,
                                                         source_skill_id);
                if (active_transmuter != 0 && active_transmuter != node_id) {
                  continue;
                }
              }
              if (is_keystone_excluded(specialized, source_skill_id, node_id,
                                       node_contract)) {
                continue;
              }
              for (const auto &dmod :
                   tree->nodes.at(node_id).damage_modifiers) {
                if (dmod.type == ModifierType::More &&
                    (dmod.source_tag == Tag::None ||
                     HasTag(inst.tags, dmod.source_tag))) {
                  final_more *=
                      std::pow(1.0f + dmod.value, static_cast<float>(pts));
                }
              }
              if (node_contract &&
                  node_contract->cost_affix != CostAffixPreset::None) {
                const auto &cost_affix = CombatAntiMeta::GetCostAffixConfig(
                    node_contract->cost_affix);
                if (cost_affix.damage_more_value > 0.0f &&
                    (cost_affix.damage_more_source_tag == Tag::None ||
                     HasTag(inst.tags, cost_affix.damage_more_source_tag))) {
                  accumulate_cost_affix_bucket(
                      cost_affix.damage_more_source_tag,
                      cost_affix.damage_more_value * static_cast<float>(pts));
                }
              }
            }
          }
        }
      }
      if (registry.valid(source_entity)) {
        if (auto *skillMods =
                registry.try_get<SkillModifierComponent>(source_entity)) {
          for (const auto &dmod : skillMods->damage_modifiers) {
            if (dmod.type == ModifierType::More &&
                (dmod.source_tag == Tag::None ||
                 HasTag(inst.tags, dmod.source_tag))) {
              final_more *= (1.0f + dmod.value);
            }
          }
        }
      }
      for (size_t bucket_idx = 0; bucket_idx < cost_affix_buckets.size;
           ++bucket_idx) {
        const float effective = CombatAntiMeta::ApplyDiminishingReturns(
            cost_affix_buckets[bucket_idx].actual);
        final_more *= (1.0f + effective);
      }
      inst.amount *= final_more;
      inst.amount *= endgameDamageMoreMultiplier;
      inst.amount *= trigger_effectiveness;
    }

    // 5. Final Settlement (Crit & Defense)
    float crit_mult = 1.0f;
    if (!thorns_like_damage && HasTag(inst.tags, Tag::Hit) &&
        !HasTag(inst.tags, Tag::DamageOverTime)) {
      bool is_crit = HasTag(additional_tags, Tag::Critical);

      // Dynamic Crit Check if not already marked as critical
      if (!is_crit && attacker_stats) {
        float crit_chance = StatsSystem::GetStatWithTags(
            registry, attacker, StatType::CritChance, inst.tags, skill_id,
            source_entity);

        // Talent: Vital Sense (ID 150)
        if (skill_id == 1) {
          if (auto *active =
                  registry.try_get<ActiveSkillsComponent>(attacker)) {
            for (const auto &spec : active->specialized_slots) {
              if (spec.skill_id == 1) {
                if (spec.allocated_points.contains(150) &&
                    spec.allocated_points.at(150) > 0) {
                  bool isFullHealth = false;
                  if (auto *hp = registry.try_get<HealthComponent>(defender)) {
                    isFullHealth = hp->current >= hp->max * 0.99f;
                  } else if (auto *defStats =
                                 registry.try_get<CombatStats>(defender)) {
                    isFullHealth =
                        defStats->health >= defStats->max_health * 0.99f;
                  }

                  bool isControlled = false;
                  if (auto *effects =
                          registry.try_get<ActiveEffectsComponent>(defender)) {
                    for (const auto &effect : effects->effects) {
                      if (effect.type == BuffType::Stun ||
                          effect.type == BuffType::Freeze ||
                          effect.type == BuffType::Root) {
                        isControlled = true;
                        break;
                      }
                    }
                  }

                  if (isFullHealth || isControlled) {
                    crit_chance = std::min(100.0f, crit_chance * 2.0f);
                  }
                }
                break;
              }
            }
          }
        }

        if (is_simulation) {
          // Calculate expected damage multiplier
          using namespace NoMoreDay::Constants::Combat::Pipeline;
          float chance = std::clamp(crit_chance, 0.0f, 100.0f) / 100.0f;
          float dmg_mult =
              attacker_stats ? attacker_stats->crit_damage : DEFAULT_CRIT_MULT;
          // Expected = 1 * (1-P) + Mult * P = 1 + P * (Mult - 1)
          crit_mult = 1.0f + chance * (dmg_mult - 1.0f);
        } else {
          if ((utils::ThreadSafeRandom::GetFloat01()) <
              (crit_chance / 100.0f)) {
            is_crit = true;
          }
        }
      }

      if (is_crit) {
        using namespace NoMoreDay::Constants::Combat::Pipeline;
        crit_mult =
            attacker_stats ? attacker_stats->crit_damage : DEFAULT_CRIT_MULT;
        result.is_crit = true;
      }
    }
    inst.amount *= crit_mult;

    using namespace NoMoreDay::Constants::Combat::Pipeline;
    int type_idx = std::countr_zero(static_cast<uint64_t>(inst.final_type));
    float damage_after_res = inst.amount;
    if (!skip_mitigation) {
      if (defense_resolution.blocked) {
        damage_after_res *= defense_resolution.block_multiplier;
      }

      float res = 0.0f;
      if (type_idx < ELEMENTAL_TYPE_COUNT) {
        res = defender_stats ? defender_stats->resistances[type_idx] : 0.0f;
        res += endgame.incoming_resistance_bonus;
        res -= endgame.outgoing_resistance_reduction;
        // Resistance Cap: -100% to +75%
        res = std::clamp(res, RESISTANCE_MIN, RESISTANCE_MAX);
      }

      damage_after_res *= (1.0f - res);

      if (inst.final_type == Tag::Physical && defender_stats) {
        float armor = defender_stats->armor + endgame.incoming_armor_bonus;
        // Retrieve attacker's Flat Armor Penetration
        float pen = StatsSystem::GetStatWithTags(
            registry, attacker, StatType::ArmorPenetration, inst.tags, skill_id,
            source_entity);
        float effective_armor =
            armor - pen - endgame.outgoing_armor_reduction;

        int area_level = defender_stats->cached_area_level; // Use cached level
        float armor_multiplier =
            NoMoreDay::CombatFormula::CalculateArmorMultiplier(effective_armor,
                                                               area_level);

        damage_after_res *= armor_multiplier;
      }

      // Global DR
      using namespace NoMoreDay::Constants::Combat::Pipeline;
      if (defender_stats && defender_stats->damage_reduction > 0.0f) {
        const float effectiveDr = std::clamp(
            defender_stats->damage_reduction +
                endgame.incoming_global_damage_reduction_bonus -
                endgame.outgoing_global_damage_reduction_reduction,
            0.0f, DR_MAX);
        damage_after_res *= (1.0f - effectiveDr);
      } else {
        const float effectiveDr = std::clamp(
            endgame.incoming_global_damage_reduction_bonus -
                endgame.outgoing_global_damage_reduction_reduction,
            0.0f, DR_MAX);
        damage_after_res *= (1.0f - effectiveDr);
      }

      damage_after_res *= endgameDamageTakenMultiplier;

      COMBAT_DEFENSE_LOG(
          "[DefenseChain] attacker={} defender={} step=3/4 typeIdx={} "
          "postMitigation={:.4f}",
          static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
          type_idx, damage_after_res);
    }

    total_final_damage += damage_after_res;

    if (type_idx < 16) {
      result.final_pool.values[type_idx] += damage_after_res;
    }
  }

  // === FINAL MULTIPLIERS (Suppressor, etc) ===
  total_final_damage *= suppressor_multiplier;
  for (int i = 0; i < 6; ++i) {
    result.final_pool.values[i] *= suppressor_multiplier;
  }

  COMBAT_DEFENSE_LOG(
      "[DefenseChain] attacker={} defender={} step=5 barrier=delegated "
      "step=6 hpDamage={:.4f} blocked={} blockMultiplier={:.4f}",
      static_cast<uint32_t>(attacker), static_cast<uint32_t>(defender),
      total_final_damage, result.was_blocked ? "true" : "false",
      result.block_multiplier);

  // --- Phantom Flash Counter Logic (Single Target) ---
  if (!is_simulation && registry.valid(defender)) {
    if (auto *pf = registry.try_get<PhantomFlashComponent>(defender)) {
      if (pf->counter_window > 0.0f && !pf->triggered) {
        pf->triggered = true;
        total_final_damage = 0.0f; // Negate damage
        result.final_pool.Clear(); // Clear pools

        spdlog::info("Phantom Flash: Counter Triggered by entity {}",
                     (uint32_t)defender);

        // Trigger Counter Attack (Visual/Logic)
        if (registry.valid(attacker) && registry.all_of<Position>(attacker) &&
            registry.all_of<Position>(defender)) {
          const auto &attPos = registry.get<Position>(attacker);
          const auto &defPos = registry.get<Position>(defender);
          NoMoreDay::SkillSystem::ShadowCast(registry, defender, 2,
                                             {defPos.x, defPos.y},
                                             {attPos.x, attPos.y});
        }
      }
    }

    // --- Blade Ward Interception Logic (Projectiles only) ---
    if (HasTag(combined_hit_tags, Tag::Projectile)) {
      if (auto *ward = registry.try_get<BladeWardComponent>(defender)) {
        if (ward->sword_count > 0) {
          if (utils::ThreadSafeRandom::GetFloat01() <
              ward->interception_chance) {
            if (!ward->is_solidified) {
              ward->sword_count--;
            }
            total_final_damage = 0.0f; // Negate damage
            result.final_pool.Clear();

            spdlog::info(
                "Blade Ward: Projectile intercepted! Swords remaining: {}",
                ward->sword_count);
          }
        }
      }
    }
  }

  result.total_damage = total_final_damage;

  // --- Event System: Dispatch combat events ---
  if (!is_simulation) {
    if (!HasTag(combined_hit_tags, Tag::DamageOverTime)) {
      uint64_t cast_id = 0;
      entt::entity actual_attacker = attacker; // Default to the attacker param

      if (summon_attribution.IsValid()) {
        actual_attacker = summon_attribution.owner;
      }

      if (registry.valid(source_entity)) {
        // Get the real caster (owner) from skill entities
        if (auto *proj = registry.try_get<Projectile>(source_entity)) {
          cast_id = proj->cast_id;
          if (!summon_attribution.IsValid() && registry.valid(proj->owner)) {
            actual_attacker = proj->owner;
          }
        }
      }

      CombatEvent hit_evt = CombatEventFactory::CreateSkillHit(
          actual_attacker, defender, skill_id, combined_hit_tags,
          result.is_crit, cast_id);
      AttachSummonAttributionIfAny(hit_evt, summon_attribution);
      CombatEventDispatcher::Dispatch(registry, hit_evt);
    }

    if (total_final_damage > 0.0f) {
      // OnDealDamage (from attacker's perspective)
      CombatEvent deal_evt = CombatEventFactory::CreateDealDamage(
          attacker, defender, skill_id, combined_hit_tags, total_final_damage,
          result.is_crit, source_entity);
      AttachSummonAttributionIfAny(deal_evt, summon_attribution);
      CombatEventDispatcher::Dispatch(registry, deal_evt);

      // OnTakeDamage (from defender's perspective)
      CombatEvent take_evt = CombatEventFactory::CreateTakeDamage(
          defender, attacker, skill_id, combined_hit_tags, total_final_damage,
          result.is_crit);
      AttachSummonAttributionIfAny(take_evt, summon_attribution);
      CombatEventDispatcher::Dispatch(registry, take_evt);

      // OnCrit (if critical hit)
      if (result.is_crit) {
        CombatEvent crit_evt = CombatEventFactory::CreateOnCrit(
            attacker, defender, skill_id, combined_hit_tags,
            total_final_damage);
        AttachSummonAttributionIfAny(crit_evt, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, crit_evt);
      }
    }
  }

  return result;
}

DamagePipeline::AttackerSnapshot
DamagePipeline::CreateSnapshot(entt::registry &registry, entt::entity attacker,
                               uint32_t skill_id, const DamagePool &base_pool,
                               Tag hit_tags, entt::entity source_entity) {
  using namespace NoMoreDay::Constants::Combat::Pipeline;
  AttackerSnapshot snap;
  snap.hit_tags = hit_tags;

  // We run a "simulation" calculation on a dummy target to get the attacker's
  // final output per type This is a bit of a hack but it reuse the existing
  // complex logic of Calculate()
  DamageResult res = Calculate(registry, attacker, entt::null, skill_id,
                               base_pool, hit_tags, source_entity, true);

  for (int i = 0; i < ELEMENTAL_TYPE_COUNT; ++i)
    snap.base_damage[i] = res.final_pool.values[i];

  auto *stats = registry.try_get<CombatStats>(attacker);
  snap.crit_chance = stats ? stats->crit_chance : 0.0f;
  snap.crit_damage = stats ? stats->crit_damage : DEFAULT_CRIT_MULT;
  snap.armor_pen =
      stats
          ? stats->armor_pen
          : 0.0f; // Simplified for now, should use GetStatWithTags if possible
  snap.accuracy = stats ? stats->accuracy : 1.0f;

  return snap;
}

void DamagePipeline::CalculateBatch(
    entt::registry &registry, entt::entity attacker,
    const std::vector<entt::entity> &defenders, uint32_t skill_id,
    const DamagePool &base_pool, Tag additional_tags,
    entt::entity source_entity, tf::Executor *executor) {
  ScopedDamageTelemetryTimer telemetryTimer;

  using namespace NoMoreDay::Constants::Combat::Pipeline;
  if (defenders.empty())
    return;

  const auto *skill_data = SkillRegistry::Get().GetSkill(skill_id);
  Tag combined_tags =
      (skill_data ? skill_data->tags : Tag::None) | additional_tags;
  auto &endgameRegistry = systems::EndgameModifierRegistry::Get();
  (void)endgameRegistry.EnsureLoaded();
  const SummonAttributionTuple summon_attribution =
      ResolveSummonAttribution(registry, attacker, source_entity, skill_id);

  // 1. Snapshot Attacker
  AttackerSnapshot snap = CreateSnapshot(
      registry, attacker, skill_id, base_pool, combined_tags, source_entity);

  struct BatchResult {
    entt::entity target = entt::null;
    float damage = 0.0f;
    bool is_crit = false;
    bool was_dodged = false;
    bool was_blocked = false;
    float block_multiplier = 1.0f;
  };
  std::vector<BatchResult> results(defenders.size());

  auto process_range = [&](size_t start, size_t end) {
    using batch_type = xsimd::batch<float>;
    size_t inc = batch_type::size;

    for (size_t i = start; i < end;) {
      if (i + inc <= end) {
        alignas(32) std::array<float, batch_type::size> res_batch_data;
        alignas(32) std::array<float, batch_type::size> armor_batch_data;
        alignas(32) std::array<float, batch_type::size>
            level_batch_data; // Added for Level Scaling
        alignas(32) std::array<float, batch_type::size> endgame_res_delta_data;
        alignas(32) std::array<float, batch_type::size> endgame_armor_delta_data;
        alignas(32) std::array<float, batch_type::size> endgame_dr_delta_data;
        alignas(32) std::array<float, batch_type::size>
            endgame_damage_taken_mult_data;
        alignas(32) std::array<float, batch_type::size> final_dmg_sum;
        final_dmg_sum.fill(0.0f);

        for (size_t k = 0; k < inc; ++k) {
          const auto defender = defenders[i + k];
          if (!registry.valid(defender)) {
            endgame_res_delta_data[k] = 0.0f;
            endgame_armor_delta_data[k] = 0.0f;
            endgame_dr_delta_data[k] = 0.0f;
            endgame_damage_taken_mult_data[k] = 1.0f;
            continue;
          }
          const auto endgame =
              endgameRegistry.ResolveForEntities(registry, attacker, defender)
                  .aggregate;
          endgame_res_delta_data[k] = endgame.incoming_resistance_bonus -
                                      endgame.outgoing_resistance_reduction;
          endgame_armor_delta_data[k] =
              endgame.incoming_armor_bonus - endgame.outgoing_armor_reduction;
          endgame_dr_delta_data[k] =
              endgame.incoming_global_damage_reduction_bonus -
              endgame.outgoing_global_damage_reduction_reduction;
          endgame_damage_taken_mult_data[k] =
              ClampMoreToMultiplier(endgame.incoming_damage_taken_more);
        }

        using namespace NoMoreDay::Constants::Combat::Pipeline;
        for (int j = 0; j < ELEMENTAL_TYPE_COUNT; ++j) {
          float base_amt = snap.base_damage[j];
          if (base_amt <= 0.0f)
            continue;

          for (size_t k = 0; k < inc; ++k) {
            auto *ds = registry.try_get<CombatStats>(defenders[i + k]);
            res_batch_data[k] =
                (ds ? ds->resistances[j] : 0.0f) + endgame_res_delta_data[k];
            armor_batch_data[k] =
                (j == 0 && ds) ? (ds->armor + endgame_armor_delta_data[k]) : 0.0f;
            level_batch_data[k] = (j == 0 && ds) ? (float)ds->cached_area_level
                                                 : 1.0f; // Default level 1
          }

          using namespace NoMoreDay::Constants::Combat::Pipeline;
          auto amt_v = batch_type(base_amt);
          auto raw_res_v = batch_type::load_aligned(res_batch_data.data());
          // Robust clamp via select to avoid namespace issues with min/max
          auto res_v = xsimd::select(
              raw_res_v > batch_type(RESISTANCE_MAX),
              batch_type(RESISTANCE_MAX),
              xsimd::select(raw_res_v < batch_type(RESISTANCE_MIN),
                            batch_type(RESISTANCE_MIN), raw_res_v));
          auto current_v = amt_v * (batch_type(1.0f) - res_v);

          if (j == 0) {
            using namespace NoMoreDay::Constants::Combat::
                Scaling; // Use Scaling namespace for formula consts if needed,
                         // but we implement formula logic directly here
            auto pen_v = batch_type(snap.armor_pen);
            auto eff_armor_v =
                batch_type::load_aligned(armor_batch_data.data()) - pen_v;

            // Level Factor: 10 + 0.5*L + 0.05*L^2
            auto L_v = batch_type::load_aligned(level_batch_data.data());
            auto LF_v = batch_type(10.0f) + batch_type(0.5f) * L_v +
                        batch_type(0.05f) * L_v * L_v;

            auto abs_armor_v = xsimd::abs(eff_armor_v);
            auto denom_v = abs_armor_v + LF_v;

            // Positive: LF / (Armor + LF) = LF / denom
            auto pos_mult = LF_v / denom_v;

            // Negative: 1 + |Armor| / (|Armor| + LF) = 1 + abs_armor / denom
            auto neg_mult = batch_type(1.0f) + (abs_armor_v / denom_v);

            auto positive_mask = eff_armor_v >= batch_type(0.0f);
            current_v *= xsimd::select(positive_mask, pos_mult, neg_mult);
          }
          auto sum_v =
              batch_type::load_aligned(final_dmg_sum.data()) + current_v;
          sum_v.store_aligned(final_dmg_sum.data());
        }

        for (size_t k = 0; k < inc; ++k) {
          using namespace NoMoreDay::Constants::Combat::Pipeline;
          auto defender = defenders[i + k];
          auto *ds = registry.try_get<CombatStats>(defender);
          float dr = ds ? ds->damage_reduction : 0.0f;
          const float effectiveDr =
              std::clamp(dr + endgame_dr_delta_data[k], 0.0f, DR_MAX);
          float damage = final_dmg_sum[k] * (1.0f - effectiveDr);
          damage *= endgame_damage_taken_mult_data[k];

          // === Suppressor Check (Batch) ===
          if (auto *suppressor =
                  registry.try_get<SuppressorComponent>(defender)) {
            auto *attPos = registry.try_get<Position>(attacker);
            auto *defPos = registry.try_get<Position>(defender);
            if (attPos && defPos) {
              float dx = attPos->x - defPos->x;
              float dy = attPos->y - defPos->y;
              float distanceSq = dx * dx + dy * dy;
              if (distanceSq > suppressor->threshold * suppressor->threshold) {
                damage *= (1.0f - suppressor->damageReduction);
              }
            }
          }

          bool is_crit = (snap.crit_chance > 0.0f &&
                          (utils::ThreadSafeRandom::GetFloat01() <
                           (snap.crit_chance / 100.0f)));
          results[i + k] = {defender,
                            is_crit ? (damage * snap.crit_damage) : damage,
                            is_crit};
        }
        i += inc;
      } else {
        auto defender = defenders[i];
        if (registry.valid(defender)) {
          auto *def_stats = registry.try_get<CombatStats>(defender);
          const auto endgame =
              endgameRegistry.ResolveForEntities(registry, attacker, defender)
                  .aggregate;
          const float endgameResDelta = endgame.incoming_resistance_bonus -
                                        endgame.outgoing_resistance_reduction;
          const float endgameArmorDelta =
              endgame.incoming_armor_bonus - endgame.outgoing_armor_reduction;
          const float endgameDrDelta =
              endgame.incoming_global_damage_reduction_bonus -
              endgame.outgoing_global_damage_reduction_reduction;
          const float endgameDamageTakenMultiplier =
              ClampMoreToMultiplier(endgame.incoming_damage_taken_more);
          // Use defaults if stats are missing to avoid "invincible" bugs
          float final_damage = 0.0f;
          float dr = def_stats ? def_stats->damage_reduction : 0.0f;
          float armor = (def_stats ? def_stats->armor : 0.0f) + endgameArmorDelta;

          using namespace NoMoreDay::Constants::Combat::Pipeline;
          for (int j = 0; j < ELEMENTAL_TYPE_COUNT; ++j) {
            float amt = snap.base_damage[j];
            if (amt <= 0.0f)
              continue;
            float res = def_stats ? def_stats->resistances[j] : 0.0f;
            res += endgameResDelta;
            res = std::clamp(res, RESISTANCE_MIN, RESISTANCE_MAX);
            float after_res = amt * (1.0f - res);
            if (j == 0) {
              float effective_armor = armor - snap.armor_pen;
              int area_level = def_stats ? def_stats->cached_area_level : 1;
              float armor_mult =
                  NoMoreDay::CombatFormula::CalculateArmorMultiplier(
                      effective_armor, area_level);
              after_res *= armor_mult;
            }
            final_damage += after_res;
          }
          const float effectiveDr = std::clamp(dr + endgameDrDelta, 0.0f, DR_MAX);
          final_damage *= (1.0f - effectiveDr);
          final_damage *= endgameDamageTakenMultiplier;

          // === Suppressor Check (Fallback) ===
          if (auto *suppressor =
                  registry.try_get<SuppressorComponent>(defender)) {
            auto *attPos = registry.try_get<Position>(attacker);
            auto *defPos = registry.try_get<Position>(defender);
            if (attPos && defPos) {
              float dx = attPos->x - defPos->x;
              float dy = attPos->y - defPos->y;
              float distanceSq = dx * dx + dy * dy;
              if (distanceSq > suppressor->threshold * suppressor->threshold) {
                final_damage *= (1.0f - suppressor->damageReduction);
              }
            }
          }

          bool is_crit = (snap.crit_chance > 0.0f &&
                          (utils::ThreadSafeRandom::GetFloat01() <
                           (snap.crit_chance / 100.0f)));
          results[i] = {defender,
                        is_crit ? (final_damage * snap.crit_damage)
                                : final_damage,
                        is_crit};
        }
        i++;
      }
    }
  };

  // 2. Execution
  if (executor && defenders.size() >= (size_t)BATCH_GRAIN_SIZE) {
    // Parallel Math
    tf::Taskflow taskflow;
    size_t grainSize = BATCH_GRAIN_SIZE;
    for (size_t i = 0; i < defenders.size(); i += grainSize) {
      size_t start = i;
      size_t end = std::min(i + grainSize, defenders.size());
      taskflow.emplace([=]() { process_range(start, end); });
    }
    executor->run(taskflow).wait();
  } else {
    process_range(0, defenders.size());
  }

  // 3. Serial Commit (Main Thread Safe)
  const auto *batch_attacker_stats = registry.try_get<CombatStats>(attacker);
  for (const auto &res : results) {
    if (res.target != entt::null) {
      float final_damage = res.damage;
      const auto *batch_defender_stats = registry.try_get<CombatStats>(res.target);
      const DefenseResolution defense_resolution = ResolveDefenseResolution(
          registry, attacker, res.target, batch_attacker_stats,
          batch_defender_stats, combined_tags, false, false, true);

      if (defense_resolution.dodged) {
        continue;
      }

      if (defense_resolution.blocked) {
        final_damage *= defense_resolution.block_multiplier;
      }

      // --- Phantom Flash Counter Logic ---
      if (auto *pf = registry.try_get<PhantomFlashComponent>(res.target)) {
        if (pf->counter_window > 0.0f && !pf->triggered) {
          pf->triggered = true;
          final_damage = 0.0f;

          if (registry.valid(attacker) && registry.all_of<Position>(attacker) &&
              registry.all_of<Position>(res.target)) {
            const auto &attPos = registry.get<Position>(attacker);
            const auto &defPos = registry.get<Position>(res.target);
            NoMoreDay::SkillSystem::ShadowCast(registry, res.target, 2,
                                               {defPos.x, defPos.y},
                                               {attPos.x, attPos.y});
          }
        }
      }

      // --- Blade Ward Interception Logic ---
      if (HasTag(combined_tags, Tag::Projectile)) {
        if (auto *ward = registry.try_get<BladeWardComponent>(res.target)) {
          if (ward->sword_count > 0) {
            if (utils::ThreadSafeRandom::GetFloat01() <
                ward->interception_chance) {
              if (!ward->is_solidified) {
                ward->sword_count--;
              }
              final_damage = 0.0f;
              spdlog::info("Blade Ward (Batch): Projectile intercepted for "
                           "entity {}! Swords remaining: {}",
                           (uint32_t)res.target, ward->sword_count);
            }
          }
        }
      }

      CombatSystem::ApplyDamage(registry, res.target, final_damage, attacker,
                                res.is_crit);

      // --- Event System: Dispatch combat events ---
      if (!HasTag(combined_tags, Tag::DamageOverTime)) {
        uint64_t cast_id = 0;
        entt::entity actual_attacker = attacker;

        if (summon_attribution.IsValid()) {
          actual_attacker = summon_attribution.owner;
        }

        if (registry.valid(source_entity)) {
          // Get the real owner from skill entities
          if (auto *proj = registry.try_get<Projectile>(source_entity)) {
            cast_id = proj->cast_id;
            if (!summon_attribution.IsValid() && registry.valid(proj->owner)) {
              actual_attacker = proj->owner;
            }
          } else if (auto *array =
                         registry.try_get<SwordArrayComponent>(source_entity)) {
            cast_id = array->cast_id;
            if (!summon_attribution.IsValid() && registry.valid(array->owner)) {
              actual_attacker = array->owner;
            }
          }
        }

        CombatEvent skillHitEvent = CombatEventFactory::CreateSkillHit(
            actual_attacker, res.target, skill_id, combined_tags, res.is_crit,
            cast_id);
        AttachSummonAttributionIfAny(skillHitEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, skillHitEvent);
      }

      // Dispatch specific hit types
      if (HasTag(combined_tags, Tag::Melee)) {
        CombatEvent meleeEvent =
            CombatEventFactory::CreateMeleeHit(attacker, res.target, skill_id,
                                               combined_tags, final_damage,
                                               res.is_crit);
        AttachSummonAttributionIfAny(meleeEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, meleeEvent);
      }
      if (HasTag(combined_tags, Tag::Projectile)) {
        CombatEvent projectileEvent = CombatEventFactory::CreateProjectileHit(
            attacker, res.target, skill_id, combined_tags, final_damage,
            res.is_crit, source_entity);
        AttachSummonAttributionIfAny(projectileEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, projectileEvent);
      }
      if (HasTag(combined_tags, Tag::Area)) {
        CombatEvent areaEvent = CombatEventFactory::CreateAreaHit(
            attacker, res.target, skill_id, combined_tags, final_damage,
            res.is_crit);
        AttachSummonAttributionIfAny(areaEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, areaEvent);
      }

      // Standard damage events
      CombatEvent dealEvent = CombatEventFactory::CreateDealDamage(
          attacker, res.target, skill_id, combined_tags, final_damage,
          res.is_crit, source_entity);
      AttachSummonAttributionIfAny(dealEvent, summon_attribution);
      CombatEventDispatcher::Dispatch(registry, dealEvent);

      CombatEvent takeEvent = CombatEventFactory::CreateTakeDamage(
          res.target, attacker, skill_id, combined_tags, final_damage,
          res.is_crit);
      AttachSummonAttributionIfAny(takeEvent, summon_attribution);
      CombatEventDispatcher::Dispatch(registry, takeEvent);

      if (res.is_crit) {
        CombatEvent critEvent = CombatEventFactory::CreateOnCrit(
            attacker, res.target, skill_id, combined_tags, final_damage);
        AttachSummonAttributionIfAny(critEvent, summon_attribution);
        CombatEventDispatcher::Dispatch(registry, critEvent);
      }
    }
  }
}

DamagePool
DamagePipeline::ApplyConversion(const DamagePool &pool,
                                const std::vector<DamageModifier> &mods) {
  return pool;
}

DamagePool
DamagePipeline::ApplyMultipliers(const DamagePool &pool,
                                 const std::vector<DamageModifier> &mods,
                                 Tag hit_tags) {
  return pool;
}

DamageResult DamagePipeline::Settle(const DamagePool &pool,
                                    const CombatStats &attacker_stats,
                                    const CombatStats &defender_stats,
                                    Tag hit_tags) {
  return {};
}

} // namespace NoMoreDay
