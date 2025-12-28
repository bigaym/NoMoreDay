#include "RenderSystem.hpp"
#include "../components/Common.hpp"
#include "../components/EffectComponent.hpp"
#include <string>

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

    // 3. Draw Attack Effects (Sword Slash)
    auto effectView = registry.view<const Position, const AttackEffect>();
    effectView.each([](const auto& pos, const auto& effect) {
        // 计算透明度：随时间淡出
        float alpha = 1.0f - (effect.timer / effect.lifeTime);
        Color color = effect.color;
        color.a = (unsigned char)(255 * alpha);

        // 绘制扇形 (模拟挥剑)
        // Raylib 的 DrawCircleSector 需要起始角和结束角
        float startAngle = effect.rotation - (effect.arcAngle / 2.0f);
        float endAngle = effect.rotation + (effect.arcAngle / 2.0f);
        
        // 转换角度适应 Raylib (Raylib 0度在右边，顺时针增加? 需要测试，通常数学上逆时针)
        // DrawCircleSector 绘制实心扇形，我们可能想要一个空心扇形或者半透明实心
        DrawCircleSector({pos.x, pos.y}, effect.range, startAngle, endAngle, 10, Fade(color, 0.5f * alpha));
        DrawCircleSectorLines({pos.x, pos.y}, effect.range, startAngle, endAngle, 10, color);
    });

    // 4. Draw Damage Popups
    auto popupView = registry.view<const Position, const DamagePopup>();
    popupView.each([](const auto& pos, const auto& popup) {
        float alpha = 1.0f;
        // 后半段生命周期淡出
        if (popup.timer > popup.lifeTime * 0.5f) {
            alpha = 1.0f - ((popup.timer - popup.lifeTime * 0.5f) / (popup.lifeTime * 0.5f));
        }
        
        Color color = popup.color;
        color.a = (unsigned char)(255 * alpha);
        
        // 绘制文字 - Use TextFormat to avoid std::string allocation
        const char* text = TextFormat("%d", (int)popup.damage);
        int fontSize = 20;
        // 简单的阴影效果
        DrawText(text, (int)pos.x + 1, (int)pos.y + 1, fontSize, Fade(BLACK, alpha));
        DrawText(text, (int)pos.x, (int)pos.y, fontSize, color);
    });
}
