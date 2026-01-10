#include "DamagePipeline.hpp"
#include "CombatSystem.hpp"
#include <xsimd/xsimd.hpp>
#include <algorithm>
#include <cmath>
#include <bit>
#include "spdlog/spdlog.h"
#include <array>
#include "../core/SkillRegistry.hpp"
#include "../components/Common.hpp"
#include "StatsSystem.hpp"

namespace NoMoreDay {

// Simple fixed-capacity vector helper to avoid allocations
template<typename T, size_t N>
struct FixedVector {
    std::array<T, N> data;
    size_t size = 0;

    void push_back(const T& value) {
        if (size < N) {
            data[size++] = value;
        } else {
            spdlog::warn("FixedVector overflow! Capacity: {}", N);
        }
    }

    T& operator[](size_t index) { return data[index]; }
    const T& operator[](size_t index) const { return data[index]; }
    
    T* begin() { return data.data(); }
    T* end() { return data.data() + size; }
    const T* begin() const { return data.data(); }
    const T* end() const { return data.data() + size; }
    
    bool empty() const { return size == 0; }
    void clear() { size = 0; }
};

DamageResult DamagePipeline::Calculate(
    entt::registry& registry,
    entt::entity attacker,
    entt::entity defender,
    uint32_t skill_id,
    const DamagePool& base_pool,
    Tag additional_tags,
    entt::entity source_entity,
    bool is_simulation
) {
    const auto* skill_data = SkillRegistry::Get().GetSkill(skill_id);
    Tag skill_tags = skill_data ? skill_data->tags : Tag::None;
    Tag combined_hit_tags = skill_tags | additional_tags;

    // Optimization: Access modifiers directly instead of copying to a vector
    auto* global_mods = registry.try_get<GlobalModifierComponent>(attacker);
    
    auto* attacker_stats = registry.try_get<CombatStats>(attacker);
    auto* defender_stats = registry.try_get<CombatStats>(defender);

    // Default stats if missing
    CombatStats default_stats;

    // Initial Instances from Base Pool
    struct Instance {
        float amount;
        Tag tags;
        Tag final_type;
    };
    
    FixedVector<Instance, 64> instances;
    
    // 1. Add instances from provided base_pool
    for (int i = 0; i < 16; ++i) {
        if (base_pool.values[i] > 0.0f) {
            Tag type_tag = static_cast<Tag>(1ULL << i);
            instances.push_back({base_pool.values[i], type_tag | combined_hit_tags, type_tag});
        }
    }

    // 2. Add Skill Base Damage
    if (skill_data) {
        // Calculate Weapon Damage part
        float min_w = attacker_stats ? attacker_stats->min_weapon_damage : 0.0f;
        float max_w = attacker_stats ? attacker_stats->max_weapon_damage : 0.0f;
        float weapon_avg = (min_w + max_w) * 0.5f;
        
        float base_dmg = skill_data->base_damage + (weapon_avg * skill_data->weapon_damage_mult);
        
        // Find the primary damage type of the skill
        Tag primary_type = Tag::Physical;
        for (int i = 0; i < 6; ++i) {
            Tag t = static_cast<Tag>(1ULL << i);
            if (HasTag(skill_data->tags, t)) {
                primary_type = t;
                break;
            }
        }

        if (base_dmg > 0.0f) {
            instances.push_back({base_dmg, primary_type | combined_hit_tags, primary_type});
        }
    }

    // 2. Conversion & Gain Extra
    // (Ordering: Phys -> Lightning -> Cold -> Fire -> Chaos -> Void) 
    // Simplified order: 0-5
    std::array<int, 6> order = {0, 3, 2, 1, 5, 4}; 

    for (int type_idx : order) {
        Tag current_source_type = static_cast<Tag>(1ULL << type_idx);
        
        FixedVector<const DamageModifier*, 32> conv_mods;
        FixedVector<const DamageModifier*, 32> gain_mods;
        float total_conv_pct = 0.0f;

        if (global_mods) {
            for (const auto& mod : global_mods->modifiers) {
                if (mod.source_tag == current_source_type) {
                    if (mod.type == ModifierType::Convert && mod.target_tag != Tag::None) {
                        conv_mods.push_back(&mod);
                        total_conv_pct += mod.value;
                    } else if (mod.type == ModifierType::GainExtra && mod.target_tag != Tag::None) {
                        gain_mods.push_back(&mod);
                    }
                }
            }
        }

        // --- NEW: Also check source_entity for conversion/gain ---
        if (registry.valid(source_entity)) {
            if (auto* skillMods = registry.try_get<SkillModifierComponent>(source_entity)) {
                for (const auto& mod : skillMods->damage_modifiers) {
                    if (mod.source_tag == current_source_type) {
                        if (mod.type == ModifierType::Convert && mod.target_tag != Tag::None) {
                            conv_mods.push_back(&mod);
                            total_conv_pct += mod.value;
                        } else if (mod.type == ModifierType::GainExtra && mod.target_tag != Tag::None) {
                            gain_mods.push_back(&mod);
                        }
                    }
                }
            }
        }

        if (conv_mods.empty() && gain_mods.empty()) continue;

        float conv_scale = 1.0f;
        if (total_conv_pct > 1.0f) {
            conv_scale = 1.0f / total_conv_pct;
        }

        size_t current_count = instances.size;
        for (size_t i = 0; i < current_count; ++i) {
            if (instances[i].final_type == current_source_type) {
                float original_amount = instances[i].amount;
                if (original_amount <= 0.0f) continue;
                
                for (auto* mod : gain_mods) {
                    instances.push_back({
                        original_amount * mod->value,
                        instances[i].tags | mod->target_tag,
                        mod->target_tag
                    });
                }

                float actual_conv_total = 0.0f;
                for (auto* mod : conv_mods) {
                    float amount_to_convert = original_amount * mod->value * conv_scale;
                    instances.push_back({
                        amount_to_convert,
                        instances[i].tags | mod->target_tag,
                        mod->target_tag
                    });
                    actual_conv_total += amount_to_convert;
                }
                
                instances[i].amount -= actual_conv_total;
            }
        }
    }

    // 3 & 4. Apply Multipliers (Dynamic via StatsSystem)
    DamageResult result;
    float total_final_damage = 0.0f;

    float shadow_multiplier = 1.0f;
    if (registry.all_of<ShadowCloneComponent>(attacker)) {
        shadow_multiplier = 0.5f;
    }

    for (size_t i = 0; i < instances.size; ++i) {
        auto& inst = instances[i];
        if (inst.amount <= 0.0f) continue;

        inst.amount *= shadow_multiplier;

        StatType dmg_stat = StatType::PhysicalDamage;
        switch (inst.final_type) {
            case Tag::Physical: dmg_stat = StatType::PhysicalDamage; break;
            case Tag::Fire:     dmg_stat = StatType::FireDamage; break;
            case Tag::Cold:     dmg_stat = StatType::ColdDamage; break;
            case Tag::Lightning: dmg_stat = StatType::LightningDamage; break;
            case Tag::Poison:    dmg_stat = StatType::PoisonDamage; break;
            case Tag::Shadow:    dmg_stat = StatType::ShadowDamage; break;
            default: break;
        }

        float multiplier_pct = StatsSystem::GetStatWithTags(registry, attacker, dmg_stat, inst.tags, skill_id, source_entity);
        inst.amount *= (multiplier_pct / 100.0f);

        // --- NEW: Apply Talent-Specific Damage Modifiers (Convert, More) ---
        // DOCUMENTATION: 'More' modifiers are applied here (after 'Increased') to ensure they act as independent multipliers.
        // This strictly follows the "Flat -> Increased -> More" hierarchy.
        if (auto* active = registry.try_get<ActiveSkillsComponent>(attacker)) {
            for (const auto& specialized : active->specialized_slots) {
                if (specialized.skill_id == skill_id) {
                    const auto* tree = SkillRegistry::Get().GetSkillTree(skill_id);
                    if (tree) {
                        for (auto [node_id, pts] : specialized.allocated_points) {
                            auto node_it = tree->nodes.find(node_id);
                            if (node_it != tree->nodes.end()) {
                                const auto& node = node_it->second;
                                for (const auto& dmod : node.damage_modifiers) {
                                    if (dmod.source_tag == Tag::None || HasTag(inst.tags, dmod.source_tag)) {
                                        float value = dmod.value * pts;
                                        if (dmod.type == ModifierType::More) {
                                            inst.amount *= (1.0f + value);
                                        }
                                        // TODO: Conversion in talents might need to be handled earlier in the pipeline if we want complex chaining
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        // --- NEW: Apply Skill-Specific Damage Modifiers from Source Entity (More) ---
        // DOCUMENTATION: Skill-specific 'More' modifiers (e.g. from Projectile source) are applied here.
        if (registry.valid(source_entity)) {
            if (auto* skillMods = registry.try_get<SkillModifierComponent>(source_entity)) {
                for (const auto& dmod : skillMods->damage_modifiers) {
                    if (dmod.source_tag == Tag::None || HasTag(inst.tags, dmod.source_tag)) {
                        if (dmod.type == ModifierType::More) {
                            inst.amount *= (1.0f + dmod.value);
                        }
                    }
                }
            }
        }

        // 5. Final Settlement (Crit & Defense)
        float crit_mult = 1.0f;
        if (HasTag(inst.tags, Tag::Hit) && !HasTag(inst.tags, Tag::DamageOverTime)) {
             bool is_crit = HasTag(additional_tags, Tag::Critical);
             
             // Dynamic Crit Check if not already marked as critical
             if (!is_crit && attacker_stats) {
                 float crit_chance = StatsSystem::GetStatWithTags(registry, attacker, StatType::CritChance, inst.tags, skill_id, source_entity);
                 
                 // Talent: Weakness Insight (ID 130)
                 if (skill_id == 1) {
                     if (auto* active = registry.try_get<ActiveSkillsComponent>(attacker)) {
                         for (const auto& spec : active->specialized_slots) {
                             if (spec.skill_id == 1) {
                                 if (spec.allocated_points.contains(130) && spec.allocated_points.at(130) > 0) {
                                     // Check if defender is full health
                                     if (auto* hp = registry.try_get<HealthComponent>(defender)) {
                                         if (hp->current >= hp->max) {
                                             crit_chance += 10.0f * spec.allocated_points.at(130); // +10% per point
                                         }
                                     }
                                 }
                                 break;
                             }
                         }
                     }
                 }

                 if (is_simulation) {
                     // Calculate expected damage multiplier
                     float chance = std::clamp(crit_chance, 0.0f, 100.0f) / 100.0f;
                     float dmg_mult = attacker_stats ? attacker_stats->crit_damage : 1.5f;
                     // Expected = 1 * (1-P) + Mult * P = 1 + P * (Mult - 1)
                     crit_mult = 1.0f + chance * (dmg_mult - 1.0f);
                 } else {
                     if ((GetRandomValue(0, 10000) / 100.0f) < crit_chance) {
                         is_crit = true;
                     }
                 }
             }

             if (is_crit) {
                 crit_mult = attacker_stats ? attacker_stats->crit_damage : 1.5f;
                 result.is_crit = true;
             }
        }
        inst.amount *= crit_mult;
        
        int type_idx = std::countr_zero(static_cast<uint64_t>(inst.final_type));
        float res = 0.0f;
        if (type_idx < 6) {
            res = defender_stats ? defender_stats->resistances[type_idx] : 0.0f;
            // Resistance Cap: -100% to +75%
            res = std::clamp(res, -1.0f, 0.75f);
        }
        
        float damage_after_res = inst.amount * (1.0f - res);
        
        // --- NEW: Robust Armor Calculation for Physical Damage ---
        if (inst.final_type == Tag::Physical && defender_stats) {
            float armor = defender_stats->armor;
            // Retrieve attacker's Flat Armor Penetration
            float pen = StatsSystem::GetStatWithTags(registry, attacker, StatType::ArmorPenetration, inst.tags, skill_id, source_entity);
            float effective_armor = armor - pen;
            
            float armor_multiplier = 1.0f;
            if (effective_armor >= 0.0f) {
                // Positive Armor: Standard diminishing returns
                armor_multiplier = 100.0f / (100.0f + effective_armor);
            } else {
                // Negative Armor: Increased damage taken
                // Formula ensures 0 -> 1.0, -100 -> 1.5, -infinity -> 2.0
                armor_multiplier = 2.0f - (100.0f / (100.0f - effective_armor));
            }
            damage_after_res *= armor_multiplier;
        }

        // Global DR
        if (defender_stats && defender_stats->damage_reduction > 0.0f) {
            damage_after_res *= (1.0f - std::min(0.9f, defender_stats->damage_reduction));
        }

        total_final_damage += damage_after_res;
        
        if (type_idx < 16) {
            result.final_pool.values[type_idx] += damage_after_res;
        }
    }

    result.total_damage = total_final_damage;
    return result;
}

DamagePipeline::AttackerSnapshot DamagePipeline::CreateSnapshot(entt::registry& registry, entt::entity attacker, uint32_t skill_id, const DamagePool& base_pool, Tag hit_tags, entt::entity source_entity) {
    AttackerSnapshot snap;
    snap.hit_tags = hit_tags;

    // We run a "simulation" calculation on a dummy target to get the attacker's final output per type
    // This is a bit of a hack but it reuse the existing complex logic of Calculate()
    DamageResult res = Calculate(registry, attacker, entt::null, skill_id, base_pool, hit_tags, source_entity, true);
    
    for(int i=0; i<6; ++i) snap.base_damage[i] = res.final_pool.values[i];
    
    auto* stats = registry.try_get<CombatStats>(attacker);
    snap.crit_chance = stats ? stats->crit_chance : 0.0f;
    snap.crit_damage = stats ? stats->crit_damage : 1.5f;
    snap.armor_pen = stats ? stats->armor_pen : 0.0f; // Simplified for now, should use GetStatWithTags if possible

    return snap;
}

void DamagePipeline::CalculateBatch(
    entt::registry& registry,
    entt::entity attacker,
    const std::vector<entt::entity>& defenders,
    uint32_t skill_id,
    const DamagePool& base_pool,
    Tag additional_tags,
    entt::entity source_entity,
    tf::Executor* executor
) {
    if (defenders.empty()) return;

    const auto* skill_data = SkillRegistry::Get().GetSkill(skill_id);
    Tag combined_tags = (skill_data ? skill_data->tags : Tag::None) | additional_tags;
    
    // 1. Snapshot Attacker
    AttackerSnapshot snap = CreateSnapshot(registry, attacker, skill_id, base_pool, combined_tags, source_entity);

    struct BatchResult {
        entt::entity target = entt::null;
        float damage = 0.0f;
        bool is_crit = false;
    };
    std::vector<BatchResult> results(defenders.size());

    auto process_range = [&](size_t start, size_t end) {
        using batch_type = xsimd::batch<float>;
        size_t inc = batch_type::size;

        for (size_t i = start; i < end; ) {
            if (i + inc <= end) {
                std::array<float, batch_type::size> res_batch_data;
                std::array<float, batch_type::size> armor_batch_data;
                std::array<float, batch_type::size> final_dmg_sum;
                final_dmg_sum.fill(0.0f);

                for (int j = 0; j < 6; ++j) {
                    float base_amt = snap.base_damage[j];
                    if (base_amt <= 0.0f) continue;

                    for (size_t k = 0; k < inc; ++k) {
                        auto* ds = registry.try_get<CombatStats>(defenders[i + k]);
                        res_batch_data[k] = ds ? ds->resistances[j] : 0.0f;
                        armor_batch_data[k] = (j == 0 && ds) ? ds->armor : 0.0f;
                    }

                    auto amt_v = batch_type(base_amt);
                    auto raw_res_v = batch_type::load_unaligned(res_batch_data.data());
                    // Robust clamp via select to avoid namespace issues with min/max
                    auto res_v = xsimd::select(raw_res_v > batch_type(0.75f), batch_type(0.75f), 
                                 xsimd::select(raw_res_v < batch_type(-1.0f), batch_type(-1.0f), raw_res_v));
                    auto current_v = amt_v * (batch_type(1.0f) - res_v);

                    if (j == 0) {
                        auto pen_v = batch_type(snap.armor_pen);
                        auto eff_armor_v = batch_type::load_unaligned(armor_batch_data.data()) - pen_v;
                        auto positive_mask = eff_armor_v >= batch_type(0.0f);
                        auto pos_mult = batch_type(100.0f) / (batch_type(100.0f) + eff_armor_v);
                        auto neg_mult = batch_type(2.0f) - (batch_type(100.0f) / (batch_type(100.0f) - eff_armor_v));
                        current_v *= xsimd::select(positive_mask, pos_mult, neg_mult);
                    }
                    auto sum_v = batch_type::load_unaligned(final_dmg_sum.data()) + current_v;
                    sum_v.store_unaligned(final_dmg_sum.data());
                }

                for (size_t k = 0; k < inc; ++k) {
                    auto* ds = registry.try_get<CombatStats>(defenders[i + k]);
                    float dr = ds ? ds->damage_reduction : 0.0f;
                    float damage = final_dmg_sum[k] * (1.0f - std::min(0.9f, dr));
                    bool is_crit = (snap.crit_chance > 0.0f && (GetRandomValue(0, 10000) / 100.0f < snap.crit_chance));
                    results[i + k] = { defenders[i + k], is_crit ? (damage * snap.crit_damage) : damage, is_crit };
                }
                i += inc;
            } else {
                auto defender = defenders[i];
                if (registry.valid(defender)) {
                    auto* def_stats = registry.try_get<CombatStats>(defender);
                    // Use defaults if stats are missing to avoid "invincible" bugs
                    float final_damage = 0.0f;
                    float dr = def_stats ? def_stats->damage_reduction : 0.0f;
                    float armor = def_stats ? def_stats->armor : 0.0f;

                    for (int j = 0; j < 6; ++j) {
                        float amt = snap.base_damage[j];
                        if (amt <= 0.0f) continue;
                        float res = def_stats ? std::clamp(def_stats->resistances[j], -1.0f, 0.75f) : 0.0f;
                        float after_res = amt * (1.0f - res);
                        if (j == 0) {
                            float effective_armor = armor - snap.armor_pen;
                            float armor_mult = (effective_armor >= 0.0f) ? (100.0f / (100.0f + effective_armor)) : (2.0f - (100.0f / (100.0f - effective_armor)));
                            after_res *= armor_mult;
                        }
                        final_damage += after_res;
                    }
                    final_damage *= (1.0f - std::min(0.9f, dr));
                    bool is_crit = (snap.crit_chance > 0.0f && (GetRandomValue(0, 10000) / 100.0f < snap.crit_chance));
                    results[i] = { defender, is_crit ? (final_damage * snap.crit_damage) : final_damage, is_crit };
                }
                i++;
            }
        }
    };

    // 2. Execution
    if (executor && defenders.size() >= 32) {
        // Parallel Math
        tf::Taskflow taskflow;
        size_t grainSize = 32;
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
    for (const auto& res : results) {
        if (res.target != entt::null) {
            CombatSystem::ApplyDamage(registry, res.target, res.damage, attacker, res.is_crit);
        }
    }
}

DamagePool DamagePipeline::ApplyConversion(const DamagePool& pool, const std::vector<DamageModifier>& mods) {
    return pool; 
}

DamagePool DamagePipeline::ApplyMultipliers(const DamagePool& pool, const std::vector<DamageModifier>& mods, Tag hit_tags) {
    return pool; 
}

DamageResult DamagePipeline::Settle(const DamagePool& pool, const CombatStats& attacker_stats, const CombatStats& defender_stats, Tag hit_tags) {
    return {}; 
}

} // namespace NoMoreDay
