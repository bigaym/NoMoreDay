#include "RenderSystem.hpp"
#include "../components/Common.hpp"

void RenderSystem::render(entt::registry& registry) {
    // 1. Draw Sprites (Entities with Position AND SpriteComponent)
    auto spriteView = registry.view<const Position, const SpriteComponent>();
    spriteView.each([](const auto& pos, const auto& sprite) {
        float width = (float)sprite.texture.width * sprite.scale;
        float height = (float)sprite.texture.height * sprite.scale;
        
        // Center the sprite on the position
        Vector2 origin = { width / 2.0f, height / 2.0f };
        Rectangle source = { 0.0f, 0.0f, (float)sprite.texture.width, (float)sprite.texture.height };
        Rectangle dest = { pos.x, pos.y, width, height };
        
        DrawTexturePro(sprite.texture, source, dest, origin, 0.0f, WHITE);
    });

    // 2. Draw Circles (Entities with Position AND ColorComponent, BUT NO SpriteComponent)
    auto pixelView = registry.view<const Position, const ColorComponent>(entt::exclude<SpriteComponent>);
    pixelView.each([](const auto& pos, const auto& col) {
        DrawCircle((int)pos.x, (int)pos.y, 8.0f, col.color);
    });
}
