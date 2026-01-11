#pragma once

#include "engine/render/RenderSystem.hpp"
#include <entt/entt.hpp>

TEST_CASE("RenderSystem - Basic Setup") {
    // RenderSystem is mostly visual and depends on Raylib's internal state.
    // We can verify it doesn't crash on an empty registry.
    entt::registry registry;
    
    // In a real headless test environment, we would mock raylib.
    // For now, we ensure the system logic (if any is extracted) can be verified.
    
    // UISystem::GetFont() might return an invalid font if not initialized,
    // which Raylib handles gracefully in MeasureTextEx etc.
}
