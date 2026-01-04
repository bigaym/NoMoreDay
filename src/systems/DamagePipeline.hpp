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
     * @param attacker The entity performing the attack
     * @param defender The entity receiving the damage
     * @param skill_id The ID of the skill being used (to look up base tags/mods)
     * @param base_pool The initial flat damage from the skill/weapon.
     * @param additional_tags Extra tags from the specific hit (e.g., Critical, Hit)
     * @param source_entity The entity representing the skill execution (e.g., Projectile, Shadow)
     */
    static DamageResult Calculate(
        entt::registry& registry,
        entt::entity attacker,
        entt::entity defender,
        uint32_t skill_id,
        const DamagePool& base_pool,
        Tag additional_tags = Tag::None,
        entt::entity source_entity = entt::null,
        bool is_simulation = false
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
