#include "game/systems/combat/MovementStanceSystem.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "core/logging/Logger.hpp"
#include <cmath>

namespace NoMoreDay {

void MovementStanceSystem::Update(entt::registry& registry, float dt) {
    // 1. Update Movement Stances
    auto stance_view = registry.view<MovementStanceComponent, Velocity>();
    for (auto entity : stance_view) {
        auto& stanceComp = stance_view.get<MovementStanceComponent>(entity);
        const auto& vel = stance_view.get<Velocity>(entity);

        float speedSq = vel.vx * vel.vx + vel.vy * vel.vy;
        using namespace NoMoreDay::Constants::Movement;
        bool isMoving = speedSq > STANCE_THRESHOLD_SPEED_SQ; 

        if (isMoving) {
            if (stanceComp.stance == MovementStance::Walking) {
                stanceComp.movingTimer += dt;
                if (stanceComp.movingTimer >= stanceComp.requiredMoveTime) {
                    stanceComp.stance = MovementStance::SwordRiding;
                    registry.get_or_emplace<StatsDirty>(entity);
                    
                    if (auto* color = registry.try_get<ColorComponent>(entity)) {
                        color->color = SKYBLUE;
                    }
                    LOG_INFO("Entity {} entered Sword Riding stance", (uint32_t)entity);
                }
            }
        } else {
            if (stanceComp.stance != MovementStance::Walking) {
                stanceComp.stance = MovementStance::Walking;
                registry.get_or_emplace<StatsDirty>(entity);
                if (auto* color = registry.try_get<ColorComponent>(entity)) {
                    color->color = WHITE;
                }
                LOG_INFO("Entity {} exited Sword Riding stance", (uint32_t)entity);
            }
            stanceComp.movingTimer = 0.0f;
        }
    }

    // 2. Track Movement Distance for Events
    auto acc_view = registry.view<MovementAccumulator, Velocity>();
    for (auto entity : acc_view) {
        auto& acc = acc_view.get<MovementAccumulator>(entity);
        const auto& vel = acc_view.get<Velocity>(entity);

        float speed = std::sqrt(vel.vx * vel.vx + vel.vy * vel.vy);
        if (speed < 1.0f) continue;

        float dist = speed * dt;
        acc.distance += dist;

        if (acc.distance >= acc.threshold) {
            float overflow = acc.distance - acc.threshold;
            acc.distance = overflow; // Keep overflow but reset base

            // Dispatch Event
            CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateMoveDistance(entity, acc.threshold));
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
