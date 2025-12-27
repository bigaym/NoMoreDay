#include "CombatSystem.hpp"
#include <cmath>
#include "../tools/Logger.hpp"

void CombatSystem::update(entt::registry& registry, systems::SpatialHashGrid& grid, const Camera2D& camera, float dt) {
    auto view = registry.view<PlayerTag, InputComponent, WeaponComponent, Position>();
    
    // Iterate over all players (usually just one)
    for (auto entity : view) {
        auto& input = view.get<InputComponent>(entity);
        auto& weapon = view.get<WeaponComponent>(entity);
        const auto& pos = view.get<Position>(entity);

        // 1. Handle Cooldown
        if (weapon.cooldownTimer > 0.0f) {
            weapon.cooldownTimer -= dt;
        }

        // 2. Process Attack
        if (input.attack && weapon.cooldownTimer <= 0.0f) {
            // Reset Cooldown
            weapon.cooldownTimer = weapon.cooldown;

            // Calculate Aim Direction (Player -> Mouse)
            Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);
            float dx = mouseWorld.x - pos.x;
            float dy = mouseWorld.y - pos.y;
            float len = std::sqrt(dx*dx + dy*dy);
            
            // Normalized Direction
            float dirX = (len > 0) ? dx / len : 1.0f;
            float dirY = (len > 0) ? dy / len : 0.0f;

            // Define Hitbox Center (Slightly in front of player)
            float hitCenterX = pos.x + dirX * (weapon.range * 0.5f);
            float hitCenterY = pos.y + dirY * (weapon.range * 0.5f);

            // Visual Debug (Draw directly here for prototype, ideally move to render system)
            // DrawCircleLines((int)hitCenterX, (int)hitCenterY, weapon.range, RED); 

            // 3. Query Grid for targets
            grid.query({hitCenterX, hitCenterY}, weapon.range, [&](entt::entity target) {
                if (target == entity) return; // Don't hit self

                // Validate Target (must have Position)
                if (!registry.valid(target) || !registry.all_of<Position>(target)) return;

                const auto& tPos = registry.get<Position>(target);
                float tDx = tPos.x - hitCenterX;
                float tDy = tPos.y - hitCenterY;
                
                // Circle Hit Test
                if (tDx*tDx + tDy*tDy < weapon.range * weapon.range) {
                    // HIT CONFIRMED
                    
                    // Apply Knockback
                    if (registry.all_of<Velocity>(target)) {
                        auto& tVel = registry.get<Velocity>(target);
                        tVel.vx += dirX * weapon.knockback;
                        tVel.vy += dirY * weapon.knockback;
                    }

                    // Apply Damage
                    if (registry.all_of<HealthComponent>(target)) {
                        auto& hp = registry.get<HealthComponent>(target);
                        hp.current -= weapon.damage;
                        
                        // Death Logic
                        if (hp.current <= 0) {
                            registry.destroy(target);
                        }
                    } else {
                        // For particles/props without health, maybe just destroy or knockback?
                        // For now, let's just knock them back hard.
                    }
                }
            });
        }
    }
}
