#include "MovementStanceSystem.hpp"
#include "../components/PlayerState.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../tools/Logger.hpp"
#include <cmath>

namespace NoMoreDay {

void MovementStanceSystem::Update(entt::registry& registry, float dt) {
    auto view = registry.view<MovementStanceComponent, Velocity>();
    for (auto entity : view) {
        auto& stanceComp = view.get<MovementStanceComponent>(entity);
        const auto& vel = view.get<Velocity>(entity);

        float speedSq = vel.vx * vel.vx + vel.vy * vel.vy;
        // More lenient threshold: speed > 50 (standard is 300)
        bool isMoving = speedSq > 2500.0f; 

        if (isMoving) {
            if (stanceComp.stance == MovementStance::Walking) {
                stanceComp.movingTimer += dt;
                if (stanceComp.movingTimer >= stanceComp.requiredMoveTime) {
                    stanceComp.stance = MovementStance::SwordRiding;
                    registry.get_or_emplace<StatsDirty>(entity);
                    
                    // Visual feedback: Color change
                    if (auto* color = registry.try_get<ColorComponent>(entity)) {
                        color->color = SKYBLUE;
                    }

                    LOG_INFO("Entity {} entered Sword Riding stance (2s move completed)", (uint32_t)entity);
                }
            }
        } else {
            // Stopped moving: reset timer and stance
            if (stanceComp.stance != MovementStance::Walking) {
                stanceComp.stance = MovementStance::Walking;
                registry.get_or_emplace<StatsDirty>(entity);
                
                // Reset visual
                if (auto* color = registry.try_get<ColorComponent>(entity)) {
                    color->color = WHITE;
                }

                LOG_INFO("Entity {} exited Sword Riding stance (stopped)", (uint32_t)entity);
            }
            stanceComp.movingTimer = 0.0f;
        }
    }
}

void MovementStanceSystem::OnTakeDamage(entt::registry& registry, entt::entity entity) {
    if (auto* stanceComp = registry.try_get<MovementStanceComponent>(entity)) {
        if (stanceComp->stance != MovementStance::Walking) {
            stanceComp->stance = MovementStance::Walking;
            stanceComp->movingTimer = 0.0f;
            registry.get_or_emplace<StatsDirty>(entity);
            
            // Reset visual
            if (auto* color = registry.try_get<ColorComponent>(entity)) {
                color->color = WHITE;
            }

            LOG_INFO("Entity {} Sword Riding stance interrupted by damage", (uint32_t)entity);
        } else {
            stanceComp->movingTimer = 0.0f;
        }
    }
}

}
