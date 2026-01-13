#include "engine/render/RenderSystem.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/Projectile.hpp" // For Projectile visualization
#include "game/systems/skill/SkillSystem.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/systems/item/LootFilter.hpp"
#include "raymath.h"
#include <string>
#include <cmath>

#include "game/systems/ui/PlayerHUD.hpp"

// Static member initialization
float RenderSystem::s_trauma = 0.0f;

void RenderSystem::AddScreenShake(float intensity) {
    s_trauma = std::min(s_trauma + intensity, 1.0f);
}

void RenderSystem::UpdateShake(float dt) {
    if (s_trauma > 0.0f) {
        s_trauma -= dt * 1.5f; // Decay speed
        if (s_trauma < 0.0f) s_trauma = 0.0f;
    }
}

Vector2 RenderSystem::GetShakeOffset() {
    if (s_trauma <= 0.0f) return { 0.0f, 0.0f };
    
    // Square the trauma to make the shake feel more impactful at high values
    float shake = s_trauma * s_trauma;
    
    // Max shake offset (e.g., 20 pixels)
    float maxOffset = 20.0f;
    
    // Simple random noise using GetTime
    float time = (float)GetTime();
    float offsetX = maxOffset * shake * (2.0f * ((float)(std::rand() % 100) / 100.0f) - 1.0f);
    float offsetY = maxOffset * shake * (2.0f * ((float)(std::rand() % 100) / 100.0f) - 1.0f);
    
    return { offsetX, offsetY };
}

void RenderSystem::render(entt::registry& registry, const NoMoreDay::SharedContext& context, const Camera2D& camera) {
    float cameraZoom = (context.settings) ? context.settings->cameraZoom : 1.5f;
    float fontScale = 1.0f / cameraZoom;

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

    // GPU 粒子渲染
    NoMoreDay::systems::GPUParticleSystem::Get().Render(camera);
    NoMoreDay::systems::GPUEntitySystem::Get().Render();

    // 2. 绘制基础颜色形状 (具有 Position 和 ColorComponent，但没有 SpriteComponent)
    // 排除已由 GPU 渲染的实体 (GPUIndex)
    auto pixelView = registry.view<const Position, const ColorComponent>(entt::exclude<SpriteComponent, GPUIndex>);
    for (auto entity : pixelView) {
        auto pos = pixelView.get<Position>(entity); // Copy to modify for visual offset
        const auto& col = pixelView.get<ColorComponent>(entity);

        // Spec 2.2: Visual De-stacking Offset
        // Prevent z-fighting and perfect overlap for mass units
        uint32_t id = (uint32_t)entity;
        float offsetX = (float)((id % 11) - 5) * 1.5f;
        float offsetY = (float)((id % 7) - 3) * 1.5f;
        pos.x += offsetX;
        pos.y += offsetY;
        
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

    // 2.5. 绘制浮游灵剑实体 (Spirit Sword Entities)
    auto swordView = registry.view<const NoMoreDay::SpiritSwordTag, const Position>();
    for (auto entity : swordView) {
        // Skip shadows/proxies that are not actual summons
        if (!registry.any_of<NoMoreDay::SummonComponent>(entity)) continue;

        const auto& pos = swordView.get<Position>(entity);
        
        // Determine properties (e.g. from owner or self)
        bool isEmpowered = false; 
        bool isGiant = false;
        
        if (auto* summon = registry.try_get<NoMoreDay::SummonComponent>(entity)) {
            if (registry.valid(summon->owner)) {
                if (auto* form = registry.try_get<NoMoreDay::BladeFormationComponent>(summon->owner)) {
                    isEmpowered = form->is_empowered;
                    isGiant = form->has_giant_sword;
                }
            }
        }

        // Calculate rotation based on orbit angle if we had it, but here we just point "forward" relative to orbit?
        // Actually, we don't have the orbit angle here easily unless we read SpiritSwordAI.
        float rotation = 0.0f;
        if (auto* ai = registry.try_get<NoMoreDay::SpiritSwordAI>(entity)) {
            rotation = ai->orbit_angle * (180.0f / PI) + 90.0f;
        }

        Color swordCol = isEmpowered ? GOLD : SKYBLUE;
        float scale = isGiant ? 2.5f : 1.0f;

        // Draw a simple sword shape using 3 points
        Vector2 p1 = { pos.x + cosf(rotation * DEG2RAD) * 15.0f * scale, pos.y + sinf(rotation * DEG2RAD) * 15.0f * scale };
        Vector2 p2 = { pos.x + cosf((rotation + 150) * DEG2RAD) * 7.0f * scale, pos.y + sinf((rotation + 150) * DEG2RAD) * 7.0f * scale };
        Vector2 p3 = { pos.x + cosf((rotation - 150) * DEG2RAD) * 7.0f * scale, pos.y + sinf((rotation - 150) * DEG2RAD) * 7.0f * scale };

        DrawTriangle(p1, p2, p3, swordCol);
        DrawTriangleLines(p1, p2, p3, WHITE);

        // Occasional Spark
        if (GetRandomValue(0, 100) < 5) {
             NoMoreDay::components::GPUParticle p;
             p.position = { pos.x, pos.y };
             p.velocity = { 0, 0 };
             p.color = Fade(swordCol, 0.5f);
             p.lifetime = 0.3f;
             p.maxLifetime = 0.3f;
             p.scale = 2.0f * scale;
             p.flags = 2; 
             NoMoreDay::systems::GPUParticleSystem::Get().Emit(p);
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
    for (auto entity : visualEffectView) {
        const auto& pos = visualEffectView.get<Position>(entity);
        const auto& effect = visualEffectView.get<VisualEffect>(entity);
        
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
            case VisualEffectType::AoeArray: {
                if (auto* array = registry.try_get<ArrayEffect>(entity)) {
                    static Shader arrayShader = { 0 };
                    if (arrayShader.id == 0 && context.resources) {
                        arrayShader = context.resources->getShader(entt::hashed_string("sh_aoe_array"));
                    }

                    if (arrayShader.id != 0) {
                        float radius = array->radius;
                        float thickness = array->thickness;
                        float shaderTime = effect.timer; // Use effect local timer
                        
                        // Set uniforms
                        int timeLoc = GetShaderLocation(arrayShader, "time");
                        int radiusLoc = GetShaderLocation(arrayShader, "radius");
                        int thickLoc = GetShaderLocation(arrayShader, "thickness");
                        int colorLoc = GetShaderLocation(arrayShader, "baseColor");

                        Vector4 colVec = ColorNormalize(color);

                        SetShaderValue(arrayShader, timeLoc, &shaderTime, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(arrayShader, radiusLoc, &radius, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(arrayShader, thickLoc, &thickness, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(arrayShader, colorLoc, &colVec, SHADER_UNIFORM_VEC4);

                        BeginShaderMode(arrayShader);
                        // Using DrawTexturePro with a white texture to ensure UVs are passed correctly
                        Texture2D whiteTex = { rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
                        Rectangle src = { 0, 0, 1, 1 };
                        Rectangle dest = { pos.x, pos.y, radius * 2.0f, radius * 2.0f };
                        Vector2 origin = { radius, radius }; // Center the quad
                        DrawTexturePro(whiteTex, src, dest, origin, 0.0f, WHITE);
                        EndShaderMode();
                    }
                }
                break;
            }
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
    }

    // 4. 绘制伤害飘字
    Font font = UISystem::GetFont(); // Move font retrieval up
    
    auto popupView = registry.view<const Position, const DamagePopup>();
    popupView.each([&font, fontScale](const auto& pos, const auto& popup) {
        float alpha = 1.0f;
        // 后半段生命周期淡出 // Fade out during the second half of its lifetime
        if (popup.timer > popup.lifeTime * 0.5f) {
            alpha = 1.0f - ((popup.timer - popup.lifeTime * 0.5f) / (popup.lifeTime * 0.5f));
        }
        
        Color color = popup.color;
        color.a = (unsigned char)(255 * alpha);
        // 绘制文字 - 使用 TextFormat 避免 std::string 分配 // Draw text - Use TextFormat to avoid std::string allocation
        const char* text;
        if (popup.isStatus) {
            text = popup.statusText.c_str();
        } else if (popup.isDodge) {
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
        if (popup.isStatus) baseSize = 22.0f; // Status text slightly smaller
        
        float fontSize = baseSize * popup.currentScale * fontScale;
        if (fontSize < 12.0f) fontSize = 12.0f;
        
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
    itemView.each([&registry, &font, fontScale](const auto entity, const auto& pos, const auto& item) {
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
        float baseItemFontSize = 18.0f;
        int fontSize = (int)(baseItemFontSize * scale * fontScale);
        if (fontSize < 12) fontSize = 12;

        float spacing = 1.0f;
        Vector2 textSize = MeasureTextEx(font, name, (float)fontSize, spacing);
        Vector2 textPos = { pos.x - textSize.x / 2.0f, pos.y - 30.0f * scale }; // 物品上方

        // 绘制背景框以提高可读性 // Draw background box for readability
        bool isHovered = (entity == UISystem::State.hoveredItem);
        DrawRectangleRec({ textPos.x - 4, textPos.y - 2, textSize.x + 8, textSize.y + 4 }, Fade(BLACK, 0.7f));
        
        if (isHovered) {
            // 高亮显示
            DrawRectangleLinesEx({ textPos.x - 4, textPos.y - 2, textSize.x + 8, textSize.y + 4 }, 2.0f, WHITE);
        } else {
            DrawRectangleLinesEx({ textPos.x - 4, textPos.y - 2, textSize.x + 8, textSize.y + 4 }, 1.0f, Fade(rarityColor, 0.5f));
        }

        if (IsFontValid(font)) {
            DrawTextEx(font, name, textPos, (float)fontSize, spacing, rarityColor);
        } else {
            DrawText(name, (int)textPos.x, (int)textPos.y, fontSize, rarityColor);
        }
    });

    // 金币
    auto goldView = registry.view<const Position, const GoldComponent>();
    goldView.each([&font, fontScale](const auto& pos, const auto& gold) {
        const char* text = TextFormat("%d 金币", gold.amount);
        float baseGoldFontSize = 16.0f;
        int fontSize = (int)(baseGoldFontSize * fontScale);
        if (fontSize < 10) fontSize = 10;
        
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

    // 6. Debug: GPU Flow Field Visualization
    auto& flowSystem = NoMoreDay::systems::GPUFlowFieldSystem::Get();
    if (flowSystem.m_debugDraw) {
        std::vector<Vector2> flowField = flowSystem.DownloadFlowField();
        int width = flowSystem.GetWidth();
        int height = flowSystem.GetHeight();
        Vector2 origin = flowSystem.GetGridOrigin();
        // Assuming cell size is 64x64 or similar. Need to know cell size or assume 1 unit = 1 pixel if logic dictates.
        // Spec says "Cell Specification: Each cell represents a 64x64 pixel area".
        // But in GPUFlowFieldSystem::Update, we passed world coords.
        // Let's assume 64.0f for now.
        float cellSize = 64.0f; 

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = y * width + x;
                Vector2 flow = flowField[index];
                
                if (Vector2Length(flow) > 0.01f) {
                    float posX = origin.x + x * cellSize + cellSize * 0.5f;
                    float posY = origin.y + y * cellSize + cellSize * 0.5f;
                    
                    Vector2 start = { posX, posY };
                    Vector2 end = { posX + flow.x * (cellSize * 0.4f), posY + flow.y * (cellSize * 0.4f) };
                    
                    DrawLineV(start, end, GREEN);
                    DrawCircleV(end, 2.0f, GREEN); // Arrow head
                }
            }
        }
        
        // Draw Grid Bounds
        DrawRectangleLines(origin.x, origin.y, width * cellSize, height * cellSize, RED);
    }

    // 7. 高性能伤害飘字
    NoMoreDay::DamagePopupManager::Get().Draw(font);
}
