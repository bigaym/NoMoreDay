#include "RenderSystem.hpp"
#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/EffectComponent.hpp"
#include "../components/ItemComponent.hpp"
#include "../core/AssetLoadingSystem.hpp"
#include <string>

void RenderSystem::render(entt::registry& registry) {
    // 1. 绘制精灵 (具有 Position 和 SpriteComponent 的实体)
    auto spriteView = registry.view<const Position, const SpriteComponent>();
    spriteView.each([](const auto& pos, const auto& sprite) {
        float width = (float)sprite.texture.width * sprite.scale; // 宽度
        float height = (float)sprite.texture.height * sprite.scale;
        
        // Center the sprite on the position
        Vector2 origin = { width / 2.0f, height / 2.0f };
        Rectangle source = { 0.0f, 0.0f, (float)sprite.texture.width, (float)sprite.texture.height };
        Rectangle dest = { pos.x, pos.y, width, height };
        
        DrawTexturePro(sprite.texture, source, dest, origin, 0.0f, WHITE);
    });

    // 2. 绘制圆形 (具有 Position 和 ColorComponent，但没有 SpriteComponent 的实体)
    auto pixelView = registry.view<const Position, const ColorComponent>(entt::exclude<SpriteComponent>);
    pixelView.each([](const auto& pos, const auto& col) {
        DrawCircle((int)pos.x, (int)pos.y, 8.0f, col.color); // 绘制圆形
    });

    // 3. 绘制攻击特效 (挥剑轨迹)
    auto effectView = registry.view<const Position, const AttackEffect>();
    effectView.each([](const auto& pos, const auto& effect) {
        // 计算透明度：随时间淡出 // Calculate transparency: fade out over time
        float alpha = 1.0f - (effect.timer / effect.lifeTime);
        Color color = effect.color;
        color.a = (unsigned char)(255 * alpha);

        // 绘制扇形 (模拟挥剑) // Draw sector (simulate sword swing)
        // Raylib 的 DrawCircleSector 需要起始角和结束角 // Raylib's DrawCircleSector requires start and end angles
        float startAngle = effect.rotation - (effect.arcAngle / 2.0f); // 起始角度
        float endAngle = effect.rotation + (effect.arcAngle / 2.0f); // 结束角度
        
        // 转换角度适应 Raylib (Raylib 0度在右边，顺时针增加? 需要测试，通常数学上逆时针) // Convert angles to suit Raylib (Raylib 0 degrees to the right, clockwise increase? Needs testing, usually counter-clockwise mathematically)
        // DrawCircleSector 绘制实心扇形，我们可能想要一个空心扇形或者半透明实心 // DrawCircleSector draws a solid sector, we might want a hollow or semi-transparent solid sector
        DrawCircleSector({pos.x, pos.y}, effect.range, startAngle, endAngle, 10, Fade(color, 0.5f * alpha));
        DrawCircleSectorLines({pos.x, pos.y}, effect.range, startAngle, endAngle, 10, color);
    });

    // 4. 绘制伤害飘字
    auto popupView = registry.view<const Position, const DamagePopup>();
    popupView.each([](const auto& pos, const auto& popup) {
        float alpha = 1.0f;
        // 后半段生命周期淡出 // Fade out during the second half of its lifetime
        if (popup.timer > popup.lifeTime * 0.5f) {
            alpha = 1.0f - ((popup.timer - popup.lifeTime * 0.5f) / (popup.lifeTime * 0.5f));
        }
        
        Color color = popup.color;
        color.a = (unsigned char)(255 * alpha);
        // 绘制文字 - 使用 TextFormat 避免 std::string 分配 // Draw text - Use TextFormat to avoid std::string allocation
        const char* text = TextFormat("%d", (int)popup.damage);
        int fontSize = 20;
        // 简单的阴影效果 // Simple shadow effect
        DrawText(text, (int)pos.x + 1, (int)pos.y + 1, fontSize, Fade(BLACK, alpha));
        DrawText(text, (int)pos.x, (int)pos.y, fontSize, color);
    });

    // 5. 绘制物品和金币的世界标签
    Font font = UISystem::GetFont();

    // 物品
    auto itemView = registry.view<const Position, const NoMoreDay::ItemComponent>();
    itemView.each([&font](const auto& pos, const auto& item) {
        Color rarityColor = UISystem::GetRarityColor(item.rarity);
        const char* name = item.name.c_str();
        int fontSize = 18;
        float spacing = 1.0f;
        Vector2 textSize = MeasureTextEx(font, name, (float)fontSize, spacing);
        Vector2 textPos = { pos.x - textSize.x / 2.0f, pos.y - 30.0f }; // 物品上方

        // 绘制背景框以提高可读性 // Draw background box for readability
        DrawRectangleRec({ textPos.x - 4, textPos.y - 2, textSize.x + 8, textSize.y + 4 }, Fade(BLACK, 0.6f));
        DrawRectangleLinesEx({ textPos.x - 4, textPos.y - 2, textSize.x + 8, textSize.y + 4 }, 1.0f, Fade(rarityColor, 0.5f));

        if (IsFontReady(font)) {
            DrawTextEx(font, name, textPos, (float)fontSize, spacing, rarityColor);
        } else {
            DrawText(name, (int)textPos.x, (int)textPos.y, fontSize, rarityColor);
        }
    });

    // 金币
    auto goldView = registry.view<const Position, const GoldComponent>();
    goldView.each([&font](const auto& pos, const auto& gold) {
        const char* text = TextFormat("%d 金币", gold.amount);
        int fontSize = 16;
        float spacing = 1.0f;
        Color goldColor = GOLD;

        Vector2 textSize = MeasureTextEx(font, text, (float)fontSize, spacing);
        Vector2 textPos = { pos.x - textSize.x / 2.0f, pos.y - 25.0f };

        DrawRectangleRec({ textPos.x - 4, textPos.y - 2, textSize.x + 8, textSize.y + 4 }, Fade(BLACK, 0.6f));
        
        if (IsFontReady(font)) {
            DrawTextEx(font, text, textPos, (float)fontSize, spacing, goldColor);
        } else {
            DrawText(text, (int)textPos.x, (int)textPos.y, fontSize, goldColor);
        }
    });
}
