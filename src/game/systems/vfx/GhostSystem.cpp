#include "GhostSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/vfx/VisualGhostComponent.hpp"

namespace NoMoreDay::systems {

void GhostSystem::Update(entt::registry& registry, float dt) {
    auto view = registry.view<components::VisualGhost>();
    for (auto entity : view) {
        auto& ghost = view.get<components::VisualGhost>(entity);
        ghost.alpha -= ghost.fadeSpeed * dt;
        if (ghost.alpha <= 0.0f) {
            registry.destroy(entity);
        }
    }
}

void GhostSystem::Render(entt::registry& registry) {
    auto view = registry.view<const Position, const components::VisualGhost>();
    for (auto entity : view) {
        const auto& pos = view.get<const Position>(entity);
        const auto& ghost = view.get<const components::VisualGhost>(entity);
        
        float width = ghost.source.width * ghost.scale;
        float height = ghost.source.height * ghost.scale;
        Rectangle dest = { pos.x, pos.y, width, height };
        Vector2 origin = { width / 2.0f, height / 2.0f };
        
        Color tint = Fade(ghost.color, ghost.alpha);
        DrawTexturePro(ghost.texture, ghost.source, dest, origin, ghost.rotation, tint);
    }
}

} // namespace NoMoreDay::systems
