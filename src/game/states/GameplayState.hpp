#pragma once

#include "engine/scene/State.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include <memory>
#include "engine/physics/SpatialGrid.hpp"

// Forward declaration
class PortalSystem;

namespace NoMoreDay {

    class GameplayState : public IState {
    public:
        using IState::IState;

        void OnEnter() override;
        void OnExit() override;
        bool OnUpdate(float dt) override;
        void OnRender() override;
        ~GameplayState() override;  // Destructor needed for unique_ptr with forward-declared type
        
        // GameplayState is opaque (draws background)
        bool IsTransparent() const override { return false; }

    private:
        void InitializeEntities();
        void UpdatePhysics(float dt);

        Camera2D m_camera = { 0 };
        tf::Taskflow m_taskflow;
        systems::SpatialHashGrid m_spatialGrid{100, 100, 50}; // Initial size, resized in OnEnter/Init
        std::vector<entt::entity> m_physicsEntities;
        
        // Portal System
        std::unique_ptr<PortalSystem> m_portalSystem;
    };

}

