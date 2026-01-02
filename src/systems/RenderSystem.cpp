#include "RenderSystem.hpp"
#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/EffectComponent.hpp"
#include "../components/ItemComponent.hpp"
#include "../components/Projectile.hpp" // For Projectile visualization
#include "../components/SkillSystem.hpp"
#include "../core/AssetLoadingSystem.hpp"
#include "../core/LootFilter.hpp"
#include "raymath.h"
#include <string>
#include <cmath>

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

    // 2. 绘制基础颜色形状 (具有 Position 和 ColorComponent，但没有 SpriteComponent)
    auto pixelView = registry.view<const Position, const ColorComponent>(entt::exclude<SpriteComponent>);
    for (auto entity : pixelView) {
        const auto& pos = pixelView.get<Position>(entity);
        const auto& col = pixelView.get<ColorComponent>(entity);
        
        // 如果是投射物，绘制成剑气波形状
        if (auto* proj = registry.try_get<NoMoreDay::Projectile>(entity)) {
            if (auto* vel = registry.try_get<Velocity>(entity)) {
                // Raylib's DrawCircleSector starts 0 at 3 o'clock (Right) and goes clockwise.
                // atan2f also starts 0 at 3 o'clock (Right) in screen coordinates (Y down).
                float angle = atan2f(vel->vy, vel->vx) * (180.0f / PI);
                
                // 绘制一个弧形表示剑气
                float arcRange = proj->radius * 1.5f;
                float arcWidth = 60.0f; // Default (Rending Wave)
                
                // Skill specific adjustments
                uint32_t skill_id = 0;
                if (auto* skillComp = registry.try_get<NoMoreDay::SkillComponent>(entity)) {
                    skill_id = skillComp->skill_id;
                }
                
                if (skill_id == 1) arcWidth = 90.0f; // Flowing Thrust is wider
                
                DrawCircleSector({pos.x, pos.y}, arcRange, angle - arcWidth/2, angle + arcWidth/2, 8, Fade(col.color, 0.6f));
                DrawCircleSectorLines({pos.x, pos.y}, arcRange, angle - arcWidth/2, angle + arcWidth/2, 8, col.color);
                
                // 添加一个明亮的核心
                DrawCircleSector({pos.x, pos.y}, arcRange * 0.4f, angle - arcWidth/3, angle + arcWidth/3, 6, WHITE);
            } else {
                DrawCircle((int)pos.x, (int)pos.y, proj->radius, col.color);
            }
        } else {
            DrawCircle((int)pos.x, (int)pos.y, 8.0f, col.color);
        }
    }

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

    // 3.5. 绘制通用视觉特效 (Visual Effects)
    auto visualEffectView = registry.view<const Position, const VisualEffect>();
    visualEffectView.each([](const auto& pos, const auto& effect) {
        float lifeRatio = effect.timer / effect.lifeTime;
        
        // 简单的线性插值
        float currentScale = effect.startScale + (effect.endScale - effect.startScale) * lifeRatio;
        
        // 透明度淡出 (最后 30% 时间快速淡出)
        float alpha = 1.0f;
        if (lifeRatio > 0.7f) {
            alpha = 1.0f - ((lifeRatio - 0.7f) / 0.3f);
        }
        
        Color color = effect.color;
        color.a = (unsigned char)(255 * alpha);

        switch (effect.type) {
            case VisualEffectType::Pickup: {
                // 扩散的圆环 (Expanding Ring)
                // DrawRing(center, innerRadius, outerRadius, startAngle, endAngle, segments, color)
                float radius = currentScale * 30.0f;
                float thickness = 2.0f + (1.0f - lifeRatio) * 3.0f; // 初始厚，逐渐变细
                DrawRing({pos.x, pos.y}, radius, radius + thickness, 0, 360, 32, color);
                break;
            }
            case VisualEffectType::DropPillar: {
                // 瞬间光柱 (Transient Pillar) - 掉落瞬间的闪光
                float height = effect.param1 > 0 ? effect.param1 : 150.0f;
                float width = 30.0f * (1.0f - lifeRatio); // 逐渐变细
                
                // 核心亮光
                DrawRectangleGradientV((int)(pos.x - width/2), (int)(pos.y - height), (int)width, (int)height, Fade(WHITE, 0.0f), color);
                // 底部光晕
                DrawCircleGradient((int)pos.x, (int)pos.y, width, color, Fade(color, 0.0f));
                break;
            }
            case VisualEffectType::GoldSparkle: {
                // 闪烁星星 (Sparkle)
                // 旋转效果
                float rotation = lifeRatio * 180.0f;
                DrawPoly({pos.x, pos.y}, 4, 15.0f * currentScale, rotation, color);
                DrawPoly({pos.x, pos.y}, 4, 8.0f * currentScale, -rotation, WHITE); // 内部亮白
                break;
            }
            case VisualEffectType::LevelUp: {
                 // 升级特效 (围绕角色的光旋)
                 // TODO: 实现更复杂的升级特效
                 DrawRing({pos.x, pos.y}, 40.0f, 45.0f, lifeRatio * 360.0f, lifeRatio * 360.0f + 180.0f, 16, color);
                 break;
            }
            default:
                break;
        }
    });

    // 4. 绘制伤害飘字
    Font font = UISystem::GetFont(); // Move font retrieval up
    
    auto popupView = registry.view<const Position, const DamagePopup>();
    popupView.each([&font](const auto& pos, const auto& popup) {
        float alpha = 1.0f;
        // 后半段生命周期淡出 // Fade out during the second half of its lifetime
        if (popup.timer > popup.lifeTime * 0.5f) {
            alpha = 1.0f - ((popup.timer - popup.lifeTime * 0.5f) / (popup.lifeTime * 0.5f));
        }
        
        Color color = popup.color;
        color.a = (unsigned char)(255 * alpha);
        // 绘制文字 - 使用 TextFormat 避免 std::string 分配 // Draw text - Use TextFormat to avoid std::string allocation
        const char* text;
        if (popup.isDodge) {
            text = "闪避";
        } else if (popup.isMiss) {
            text = "未命中";
        } else if (popup.isBlock) {
            text = TextFormat("格挡 %d", (int)popup.damage);
        } else {
            text = TextFormat("%d", (int)popup.damage);
        }

        float baseSize = 28.0f; // Increased from 20
        if (popup.isCrit) baseSize = 36.0f; // Increased from 24
        
        float fontSize = baseSize * popup.currentScale;
        
        // 增加阴影或边框效果
        if (IsFontValid(font)) {
            DrawTextEx(font, text, { pos.x + 2, pos.y + 2 }, fontSize, 1.0f, Fade(BLACK, alpha * 0.8f));
            DrawTextEx(font, text, { pos.x, pos.y }, fontSize, 1.0f, color);
        } else {
            DrawText(text, (int)pos.x + 2, (int)pos.y + 2, (int)fontSize, Fade(BLACK, alpha * 0.8f));
            DrawText(text, (int)pos.x, (int)pos.y, (int)fontSize, color);
        }
    });

    // 5. 绘制物品和金币的世界标签
    // Font font = UISystem::GetFont(); // Already retrieved

    // 物品
    auto itemView = registry.view<const Position, const NoMoreDay::ItemComponent>();
    itemView.each([&registry, &font](const auto entity, const auto& pos, const auto& item) {
        Color rarityColor = UISystem::GetRarityColor(item.rarity);
        float scale = 1.0f;
        bool emphasized = false;

        // Apply Loot Filter Result
        const auto* filterResult = registry.try_get<NoMoreDay::LootFilterResultComponent>(entity);
        if (filterResult) {
            if (!filterResult->visible) return; // Skip rendering entirely
            
            if (filterResult->scale > 1.0f) {
                scale = filterResult->scale;
                emphasized = true;
                rarityColor = filterResult->color; // Use emphasize color
            }
        }
        
        // --- 光柱特效 (Rare及以上 或 被过滤器高亮) ---
        // 仅对稀有(Rare)及更高品质的物品显示光柱，方便远处识别
        if (item.rarity >= NoMoreDay::Rarity::Rare || emphasized) {
            float time = (float)GetTime();
            // 呼吸效果 (Alpha 0.3 ~ 0.6)
            float alpha = 0.45f + 0.15f * std::sin(time * 3.0f);
            
            float beamHeight = 120.0f * scale; // 光柱高度
            float beamWidth = 24.0f * scale;   // 光柱宽度
            
            // 颜色渐变：底部实色 -> 顶部透明
            Color colBottom = rarityColor;
            colBottom.a = (unsigned char)(255 * alpha);
            Color colTop = rarityColor;
            colTop.a = 0;
            
            // 绘制光柱 (从下往上渐变)
            // 坐标为左上角，所以 y = pos.y - height 让底部对齐物体
            DrawRectangleGradientV((int)(pos.x - beamWidth * 0.5f), (int)(pos.y - beamHeight), 
                                   (int)beamWidth, (int)beamHeight, colTop, colBottom);
            
            // 底部光晕
            DrawCircleGradient((int)pos.x, (int)pos.y, beamWidth * 0.8f, colBottom, Fade(colBottom, 0.0f));
            
            // 核心高亮 (更细更亮，增加立体感)
            Color coreCol = WHITE;
            coreCol.a = (unsigned char)(255 * alpha * 0.8f);
            DrawRectangleGradientV((int)(pos.x - 2 * scale), (int)(pos.y - beamHeight), (int)(4 * scale), (int)beamHeight, Fade(WHITE, 0.0f), coreCol);
        }

        const char* name = item.name.c_str();
        int fontSize = (int)(18 * scale);
        float spacing = 1.0f;
        Vector2 textSize = MeasureTextEx(font, name, (float)fontSize, spacing);
        Vector2 textPos = { pos.x - textSize.x / 2.0f, pos.y - 30.0f * scale }; // 物品上方

        // 绘制背景框以提高可读性 // Draw background box for readability
        DrawRectangleRec({ textPos.x - 4, textPos.y - 2, textSize.x + 8, textSize.y + 4 }, Fade(BLACK, 0.6f));
        DrawRectangleLinesEx({ textPos.x - 4, textPos.y - 2, textSize.x + 8, textSize.y + 4 }, 1.0f, Fade(rarityColor, 0.5f));

        if (IsFontValid(font)) {
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
        
        if (IsFontValid(font)) {
            DrawTextEx(font, text, textPos, (float)fontSize, spacing, goldColor);
        } else {
            DrawText(text, (int)textPos.x, (int)textPos.y, fontSize, goldColor);
        }
    });
}
