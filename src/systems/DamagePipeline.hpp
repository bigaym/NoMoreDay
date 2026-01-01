#pragma once
#include <vector>
#include <entt/entt.hpp>
#include "../components/SkillSystem.hpp"
#include "../components/Stats.hpp"

namespace NoMoreDay {

struct DamageResult {
    float total_damage = 0.0f;
    bool is_crit = false;
    DamagePool final_pool; // Damage broken down by type
};

class DamagePipeline {
public:
    /**
     * @brief Executes the 5-step damage calculation.
     * 
     * @param skill_id The entity representing the skill (contains SkillModifierComponent)
     * @param attacker The entity performing the attack (contains GlobalModifierComponent, CombatStats)
     * @param defender The entity receiving the damage (contains CombatStats)
     * @param base_pool The initial flat damage from the skill/weapon.
     * @param tags The tags associated with this specific hit (e.g., Melee, Spell, Hit)
     */
    static DamageResult Calculate(
        entt::registry& registry,
        entt::entity attacker,
        entt::entity defender,
        const DamagePool& base_pool,
        Tag hit_tags
    );

private:
    // Step 2: Conversion & Gain Extra
    static DamagePool ApplyConversion(const DamagePool& pool, const std::vector<DamageModifier>& mods);

    // Step 3 & 4: Apply Inc and More
    static DamagePool ApplyMultipliers(const DamagePool& pool, const std::vector<DamageModifier>& mods, Tag hit_tags);

    // Step 5: Final settlement (Crit & Defense)
    static DamageResult Settle(const DamagePool& pool, const CombatStats& attacker_stats, const CombatStats& defender_stats, Tag hit_tags);
};

} // namespace NoMoreDay
