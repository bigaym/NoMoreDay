#include "DamagePipeline.hpp"
#include <algorithm>
#include <cmath>
#include <bit>
#include "spdlog/spdlog.h"
#include <array>

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
    const DamagePool& base_pool,
    Tag hit_tags
) {
    // Optimization: Access modifiers directly instead of copying to a vector
    auto* global_mods = registry.try_get<GlobalModifierComponent>(attacker);
    
    const auto& attacker_stats = registry.get<CombatStats>(attacker);
    const auto& defender_stats = registry.get<CombatStats>(defender);

    // Initial Instances from Base Pool
    struct Instance {
        float amount;
        Tag tags;
        Tag final_type;
    };
    
    FixedVector<Instance, 64> instances;
    
    for (int i = 0; i < 16; ++i) {
        if (base_pool.values[i] > 0.0f) {
            Tag type_tag = static_cast<Tag>(1ULL << i);
            instances.push_back({base_pool.values[i], type_tag | hit_tags, type_tag});
        }
    }

    // 2. Conversion & Gain Extra
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

    // 3 & 4. Apply Multipliers
    DamageResult result;
    float total_final_damage = 0.0f;

    for (size_t i = 0; i < instances.size; ++i) {
        auto& inst = instances[i];
        if (inst.amount <= 0.0f) continue;

        float increased = 0.0f;
        float more = 1.0f;

        if (global_mods) {
            for (const auto& mod : global_mods->modifiers) {
                if (mod.type == ModifierType::Increased || mod.type == ModifierType::More) {
                    if (HasTag(inst.tags, mod.source_tag)) {
                        if (mod.type == ModifierType::Increased) {
                            increased += mod.value;
                        } else {
                            more *= (1.0f + mod.value);
                        }
                    }
                }
            }
        }

        inst.amount *= (1.0f + increased) * more;

        // 5. Final Settlement (Crit & Defense)
        float crit_mult = 1.0f;
        if (HasTag(inst.tags, Tag::Hit) && !HasTag(inst.tags, Tag::DamageOverTime)) {
             if (HasTag(hit_tags, Tag::Critical)) {
                 crit_mult = attacker_stats.crit_damage;
                 result.is_crit = true;
             }
        }
        inst.amount *= crit_mult;
        
        int type_idx = std::countr_zero(static_cast<uint64_t>(inst.final_type));
        float res = 0.0f;
        if (type_idx < 6) {
            res = defender_stats.resistances[type_idx];
        }
        
        float damage_after_res = inst.amount * (1.0f - res);
        total_final_damage += damage_after_res;
        
        if (type_idx < 16) {
            result.final_pool.values[type_idx] += damage_after_res;
        }
    }

    result.total_damage = total_final_damage;
    return result;
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
