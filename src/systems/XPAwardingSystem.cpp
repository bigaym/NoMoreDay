#include "XPAwardingSystem.hpp"
#include "../components/Common.hpp"
#include "../components/PlayerState.hpp"
#include "../components/Stats.hpp"
#include "ProgressionSystem.hpp"
#include "../tools/Logger.hpp"

namespace NoMoreDay {

void XPAwardingSystem::update(entt::registry& registry) {
    auto view = registry.view<KilledTag>();

    for (auto entity : view) {
        const auto& killedTag = view.get<KilledTag>(entity);
        entt::entity killer = killedTag.killer;

        // Ensure killer is valid and is a player
        if (registry.valid(killer) && registry.all_of<PlayerTag>(killer)) {
            // Ensure both player and killed entity have level components
            if (registry.all_of<PlayerLevel>(killer) && registry.all_of<PlayerLevel>(entity)) {
                int playerLevel = registry.get<PlayerLevel>(killer).value;
                int enemyLevel = registry.get<PlayerLevel>(entity).value; 
                float baseXP = 50.0f; // Base XP for killing an enemy (can be component-driven later)
                
                // Retrieve player's CombatStats for XP bonus (e.g., from Magic Find)
                float xpBonus = 0.0f;
                if (registry.all_of<CombatStats>(killer)) {
                    xpBonus = registry.get<CombatStats>(killer).experience_gain_mult;
                }

                float awardedXP = ProgressionSystem::CalculateAwardedXP(playerLevel, enemyLevel, baseXP);
                awardedXP *= (1.0f + xpBonus); // Apply any XP gain modifiers
                LOG_DEBUG("XPAwardingSystem: Calculated awardedXP before adding to player: {}", awardedXP);

                ProgressionSystem::AddExperience(registry, killer, awardedXP);
                LOG_DEBUG("Player {} (Lvl {}) awarded {:.2f} XP for killing Enemy {} (Lvl {})", 
                          (uint32_t)killer, playerLevel, awardedXP, (uint32_t)entity, enemyLevel);
            } else {
                 LOG_WARN("Killer or killed entity missing PlayerLevel component for XP calculation. Killer Valid: {}, Killer PlayerLevel: {}, Killed Valid: {}, Killed PlayerLevel: {}",
                          registry.valid(killer), registry.all_of<PlayerLevel>(killer), registry.valid(entity), registry.all_of<PlayerLevel>(entity));
            }
        }
        
        // Remove KilledTag as it has been processed
        registry.remove<KilledTag>(entity);
        // Destroy the entity after XP and other post-death processes are handled
        registry.destroy(entity);
    }
}

} // namespace NoMoreDay
