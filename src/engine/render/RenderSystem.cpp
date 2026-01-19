#include "engine/render/RenderSystem.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/EnemyComponent.hpp" 
#include "game/components/ItemComponent.hpp"
#include "game/components/Projectile.hpp" // For Projectile visualization
#include "game/components/SkillDefs.hpp" // For SwordIntentComponent
#include "game/components/vfx/HoloBladeComponent.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp" // For MoltenTrailTag
#include "game/systems/item/LootFilter.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/vfx/HoloBladeRenderSystem.hpp"
#include "game/systems/vfx/SwordIntentVisualSystem.hpp"
#include "game/systems/vfx/TrailSystem.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <string>

#include "game/systems/ui/PlayerHUD.hpp"

// Static member initialization
float RenderSystem::s_trauma = 0.0f;

void RenderSystem::AddScreenShake(float intensity) {
  s_trauma = std::min(s_trauma + intensity, 1.0f);
}

void RenderSystem::UpdateShake(float dt) {
  if (s_trauma > 0.0f) {
    s_trauma -= dt * 1.5f; // Decay speed
    if (s_trauma < 0.0f)
      s_trauma = 0.0f;
  }
}

Vector2 RenderSystem::GetShakeOffset() {
  if (s_trauma <= 0.0f)
    return {0.0f, 0.0f};

  // Square the trauma to make the shake feel more impactful at high values
  float shake = s_trauma * s_trauma;

  // Max shake offset (e.g., 20 pixels)
  float maxOffset = 20.0f;

  // Simple random noise using GetTime
  float time = (float)GetTime();
  float offsetX =
      maxOffset * shake * (2.0f * NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 1.0f) - 1.0f);
  float offsetY =
      maxOffset * shake * (2.0f * NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 1.0f) - 1.0f);

  return {offsetX, offsetY};
}

void RenderSystem::render(entt::registry &registry,
                          const NoMoreDay::SharedContext &context,
                          const Camera2D &camera) {
  float cameraZoom = (context.settings) ? context.settings->cameraZoom : 1.5f;
  float fontScale = 1.0f / cameraZoom;

  // 0. VFX: Trails (Rendered before sprites)
  static Shader trailShader = {0};
  if (trailShader.id == 0 && context.resources) {
    trailShader =
        context.resources->getShader(entt::hashed_string("sh_sword_trail"));
  }
  if (trailShader.id != 0) {
    NoMoreDay::systems::TrailSystem::Render(registry, trailShader);
  }

  // 0.5. Sword Intent Aura (Before Sprites)
  NoMoreDay::systems::SwordIntentVisualSystem::Render(registry);

  // 1. 绘制精灵 (具有 Position 和 SpriteComponent 的实体)
  // Updated to iterate entities for ShadowVisualComponent check
  // Exclude HoloBlade as it's handled by HoloBladeRenderSystem
  auto spriteView = registry.view<const Position, const SpriteComponent>(
      entt::exclude<NoMoreDay::ItemComponent, GoldComponent, NoMoreDay::components::HoloBlade>);

  for (auto entity : spriteView) {
    const auto & [pos, sprite] = spriteView.get(entity);

    float width = (float)sprite.texture.width * sprite.scale; // 宽度
    float height = (float)sprite.texture.height * sprite.scale;

    // Center the sprite on the position
    Vector2 origin = {width / 2.0f, height / 2.0f};
    Rectangle source = {0.0f, 0.0f, (float)sprite.texture.width,
                        (float)sprite.texture.height};
    Rectangle dest = {pos.x, pos.y, width, height};

    Color tint = WHITE;
    if (auto *svc =
            registry.try_get<NoMoreDay::ShadowVisualComponent>(entity)) {
      tint = svc->color_tint;
    }

    // --- Monster Rarity Glow (Underlay) ---
    if (auto* rarityComp = registry.try_get<EnemyRarityComponent>(entity)) {
        if (rarityComp->rarity > EnemyRarityComponent::NORMAL) {
            Color glowColor = WHITE;
            float glowScale = 1.2f;
            switch (rarityComp->rarity) {
                case EnemyRarityComponent::CHAMPION: 
                    glowColor = SKYBLUE; glowScale = 1.3f; break;
                case EnemyRarityComponent::ELITE: 
                    glowColor = { 255, 215, 0, 255 }; // Gold
                    glowScale = 1.5f; break;
                case EnemyRarityComponent::BOSS: 
                    glowColor = { 255, 120, 0, 255 }; // Orange
                    glowScale = 2.0f; break;
                case EnemyRarityComponent::NEMESIS: 
                    glowColor = { 220, 20, 60, 255 }; // Crimson
                    glowScale = 2.5f; break;
                default: break;
            }
            
            float pulse = 0.85f + 0.15f * sinf((float)GetTime() * 3.0f);
            float radius = (width > height ? width : height) * 0.6f * glowScale * pulse;
            
            // Draw multi-layered glow for premium feel
            DrawCircleGradient((int)pos.x, (int)pos.y, radius, Fade(glowColor, 0.4f), Fade(glowColor, 0.0f));
            DrawCircleGradient((int)pos.x, (int)pos.y, radius * 0.6f, Fade(glowColor, 0.6f), Fade(glowColor, 0.0f));
        }
    }

    DrawTexturePro(sprite.texture, source, dest, origin, 0.0f, tint);
  }

  // GPU 粒子渲染
  NoMoreDay::systems::GPUParticleSystem::Get().Render(camera);
  NoMoreDay::systems::GPUEntitySystem::Get().Render();

  // 2. 绘制基础颜色形状 (具有 Position 和 ColorComponent)
  // 注意：如果是投射物 (Projectile)，即使它有 GPUIndex，我们也允许 CPU
  // 绘制特殊的形状（如剑气弧）
  auto pixelView = registry.view<const Position, const ColorComponent>(
      entt::exclude<SpriteComponent>);
  for (auto entity : pixelView) {
    // 如果不是投射物且具有 GPUIndex，则跳过（由 GPU 渲染精灵）
    if (registry.any_of<GPUIndex>(entity) &&
        !registry.any_of<NoMoreDay::Projectile>(entity)) {
      continue;
    }

    auto pos = pixelView.get<Position>(entity);
    const auto &col = pixelView.get<ColorComponent>(entity);

    // Spec 2.2: Visual De-stacking Offset
    // Prevent z-fighting and perfect overlap for mass units
    uint32_t id = (uint32_t)entity;
    float offsetX = (float)((id % 11) - 5) * 1.5f;
    float offsetY = (float)((id % 7) - 3) * 1.5f;
    pos.x += offsetX;
    pos.y += offsetY;

    // Dedicated Loop for Projectiles is below (Step 6).
    // Here we only draw simple shapes for non-projectiles (debug/fallback).
    if (registry.any_of<NoMoreDay::Projectile>(entity)) {
      continue;
    }

    DrawCircle((int)pos.x, (int)pos.y, 8.0f, col.color);
  }

  // 2.3. 渲染熔火区域 (Molten Trail Zones)
  auto moltenView = registry.view<const Position, const NoMoreDay::MoltenTrailTag, const Radius, const ColorComponent, const DelayedDestroyComponent>();
  for (auto entity : moltenView) {
    const auto& pos = moltenView.get<Position>(entity);
    const auto& radius = moltenView.get<Radius>(entity);
    const auto& color = moltenView.get<ColorComponent>(entity);
    const auto& delayed = moltenView.get<DelayedDestroyComponent>(entity);
    
    // Calculate fade based on remaining time
    float lifeRatio = delayed.timer / 3.0f; // Assuming 3s duration
    lifeRatio = std::clamp(lifeRatio, 0.0f, 1.0f);
    float alpha = lifeRatio; // Fade out as time runs out
    
    // Core glow (orange-red)
    Color coreColor = color.color;
    coreColor.a = (unsigned char)(180 * alpha);
    DrawCircleGradient((int)pos.x, (int)pos.y, radius.value, coreColor, Fade(coreColor, 0.0f));
    
    // Outer ring
    Color ringColor = {255, 50, 0, (unsigned char)(100 * alpha)};
    DrawRing({pos.x, pos.y}, radius.value * 0.8f, radius.value, 0, 360, 16, ringColor);
    
    // Center bright spot
    Color brightSpot = {255, 200, 100, (unsigned char)(200 * alpha)};
    DrawCircle((int)pos.x, (int)pos.y, radius.value * 0.3f, brightSpot);
  }

  // 2.5. 绘制浮游灵剑实体 (Spirit Sword Entities)
  // 2.7. Holo Blade Rendering (Spirit Swords, etc)
  NoMoreDay::systems::HoloBladeRenderSystem::Render(registry, context);

  // 3. 绘制攻击特效 (挥剑轨迹)
  auto effectView = registry.view<const Position, const AttackEffect>();
  effectView.each([](const auto &pos, const auto &effect) {
    // 计算透明度：随时间淡出 // Calculate transparency: fade out over time
    float alpha = 1.0f - (effect.timer / effect.lifeTime);
    Color color = effect.color;
    color.a = (unsigned char)(255 * alpha);

    // 绘制扇形 (模拟挥剑) // Draw sector (simulate sword swing)
    // Raylib 的 DrawCircleSector 需要起始角和结束角 // Raylib's
    // DrawCircleSector requires start and end angles
    float startAngle = effect.rotation - (effect.arcAngle / 2.0f); // 起始角度
    float endAngle = effect.rotation + (effect.arcAngle / 2.0f);   // 结束角度

    // 转换角度适应 Raylib (Raylib 0度在右边，顺时针增加?
    // 需要测试，通常数学上逆时针) // Convert angles to suit Raylib (Raylib 0
    // degrees to the right, clockwise increase? Needs testing, usually
    // counter-clockwise mathematically) DrawCircleSector
    // 绘制实心扇形，我们可能想要一个空心扇形或者半透明实心 // DrawCircleSector
    // draws a solid sector, we might want a hollow or semi-transparent solid
    // sector
    DrawCircleSector({pos.x, pos.y}, effect.range, startAngle, endAngle, 10,
                     Fade(color, 0.5f * alpha));
    DrawCircleSectorLines({pos.x, pos.y}, effect.range, startAngle, endAngle,
                          10, color);
  });

  // 3.5. 绘制通用视觉特效 (Visual Effects)
  auto visualEffectView = registry.view<const Position, const VisualEffect>();
  for (auto entity : visualEffectView) {
    const auto &pos = visualEffectView.get<Position>(entity);
    const auto &effect = visualEffectView.get<VisualEffect>(entity);

    float lifeRatio = effect.timer / effect.lifeTime;

    // 简单的线性插值
    float currentScale =
        effect.startScale + (effect.endScale - effect.startScale) * lifeRatio;

    // 透明度淡出 (最后 30% 时间快速淡出)
    float alpha = 1.0f;
    if (lifeRatio > 0.7f) {
      alpha = 1.0f - ((lifeRatio - 0.7f) / 0.3f);
    }

    Color color = effect.color;
    color.a = (unsigned char)(255 * alpha);

    switch (effect.type) {
    case VisualEffectType::AoeArray: {
      if (auto *array = registry.try_get<ArrayEffect>(entity)) {
        static Shader arrayShader = {0};
        if (arrayShader.id == 0 && context.resources) {
          arrayShader =
              context.resources->getShader(entt::hashed_string("sh_aoe_array"));
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

          SetShaderValue(arrayShader, timeLoc, &shaderTime,
                         SHADER_UNIFORM_FLOAT);
          SetShaderValue(arrayShader, radiusLoc, &radius, SHADER_UNIFORM_FLOAT);
          SetShaderValue(arrayShader, thickLoc, &thickness,
                         SHADER_UNIFORM_FLOAT);
          SetShaderValue(arrayShader, colorLoc, &colVec, SHADER_UNIFORM_VEC4);

          BeginShaderMode(arrayShader);
          // Using DrawTexturePro with a white texture to ensure UVs are passed
          // correctly
          Texture2D whiteTex = {rlGetTextureIdDefault(), 1, 1, 1,
                                PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
          Rectangle src = {0, 0, 1, 1};
          Rectangle dest = {pos.x, pos.y, radius * 2.0f, radius * 2.0f};
          Vector2 origin = {radius, radius}; // Center the quad
          DrawTexturePro(whiteTex, src, dest, origin, 0.0f, WHITE);
          EndShaderMode();
        }
      }
      break;
    }
    case VisualEffectType::Pickup: {
      // 扩散的圆环 (Expanding Ring)
      // DrawRing(center, innerRadius, outerRadius, startAngle, endAngle,
      // segments, color)
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
      DrawRectangleGradientV((int)(pos.x - width / 2), (int)(pos.y - height),
                             (int)width, (int)height, Fade(WHITE, 0.0f), color);
      // 底部光晕
      DrawCircleGradient((int)pos.x, (int)pos.y, width, color,
                         Fade(color, 0.0f));
      break;
    }
    case VisualEffectType::GoldSparkle: {
      // 闪烁星星 (Sparkle)
      // 旋转效果
      float rotation = lifeRatio * 180.0f;
      DrawPoly({pos.x, pos.y}, 4, 15.0f * currentScale, rotation, color);
      DrawPoly({pos.x, pos.y}, 4, 8.0f * currentScale, -rotation,
               WHITE); // 内部亮白
      break;
    }
    case VisualEffectType::LevelUp: {
      // 升级特效 (围绕角色的光旋)
      // TODO: 实现更复杂的升级特效
      DrawRing({pos.x, pos.y}, 40.0f, 45.0f, lifeRatio * 360.0f,
               lifeRatio * 360.0f + 180.0f, 16, color);
      break;
    }
    case VisualEffectType::SwordIntentBurst: {
        // Sword Intent Burst: Ink Splatter + Ring
        static Texture2D inkTex = {0};
        if (inkTex.id == 0 && FileExists("assets/textures/vfx/vfx_ink_splatter.png")) {
             inkTex = LoadTexture("assets/textures/vfx/vfx_ink_splatter.png");
             SetTextureFilter(inkTex, TEXTURE_FILTER_BILINEAR);
        }

        // 1. Shockwave Ring
        float radius = currentScale * 50.0f;
        float thickness = 4.0f * (1.0f - lifeRatio);
        DrawRing({pos.x, pos.y}, radius, radius + thickness, 0, 360, 32, color);

        // 2. Ink Splatter
        if (inkTex.id != 0) {
            float spin = lifeRatio * 45.0f; // Slow spin
            float inkScale = currentScale * 1.5f; 
            
            Rectangle src = {0, 0, (float)inkTex.width, (float)inkTex.height};
            Rectangle dest = {pos.x, pos.y, inkTex.width * inkScale, inkTex.height * inkScale};
            Vector2 origin = {dest.width/2, dest.height/2};
            
            // Cyan tint
            DrawTexturePro(inkTex, src, dest, origin, spin, Fade(color, 0.8f));
        }
        break;
    }
    default:
      break;
    }
  }

  // 4. 绘制伤害飘字
  Font font = UISystem::GetFont(); // Move font retrieval up

  auto popupView = registry.view<const Position, const DamagePopup>();
  popupView.each([&font, fontScale](const auto &pos, const auto &popup) {
    float alpha = 1.0f;
    // 后半段生命周期淡出 // Fade out during the second half of its lifetime
    if (popup.timer > popup.lifeTime * 0.5f) {
      alpha = 1.0f -
              ((popup.timer - popup.lifeTime * 0.5f) / (popup.lifeTime * 0.5f));
    }

    Color color = popup.color;
    color.a = (unsigned char)(255 * alpha);
    // 绘制文字 - 使用 TextFormat 避免 std::string 分配 // Draw text - Use
    // TextFormat to avoid std::string allocation
    const char *text;
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
    if (popup.isCrit)
      baseSize = 36.0f; // Increased from 24
    if (popup.isStatus)
      baseSize = 22.0f; // Status text slightly smaller

    float fontSize = baseSize * popup.currentScale * fontScale;
    if (fontSize < 12.0f)
      fontSize = 12.0f;

    // 增加阴影或边框效果
    if (IsFontValid(font)) {
      DrawTextEx(font, text, {pos.x + 2, pos.y + 2}, fontSize, 1.0f,
                 Fade(BLACK, alpha * 0.8f));
      DrawTextEx(font, text, {pos.x, pos.y}, fontSize, 1.0f, color);
    } else {
      DrawText(text, (int)pos.x + 2, (int)pos.y + 2, (int)fontSize,
               Fade(BLACK, alpha * 0.8f));
      DrawText(text, (int)pos.x, (int)pos.y, (int)fontSize, color);
    }
  });

  // --- 5. 绘制物品和金币的世界标签 (Optimization: High-Speed Linear Culling + Multi-Pass Batching) ---
  
  struct RenderItem {
      entt::entity entity;
      Position pos;
      Color rarityColor;
      NoMoreDay::Rarity rarity;
      float scale;
      bool emphasized;
      const char* name;
      Vector2 textSize;
  };
  static std::vector<RenderItem> s_visibleItems;
  s_visibleItems.clear();

  struct RenderGold {
      Position pos;
      const char* text; 
      Vector2 textSize;
  };
  static std::vector<RenderGold> s_visibleGold;
  s_visibleGold.clear();

  // Calculate visible world area (Frustum)
  Vector2 viewTopLeft = GetScreenToWorld2D({0, 0}, camera);
  Vector2 viewBottomRight = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);
  Rectangle viewRect = { viewTopLeft.x - 100, viewTopLeft.y - 100, (viewBottomRight.x - viewTopLeft.x) + 200, (viewBottomRight.y - viewTopLeft.y) + 200 };

  // 1. Item Collection Pass (Linear culling is ~O(N) but predictable and cache-friendly)
  auto itemView = registry.view<NoMoreDay::ItemComponent, Position>();
  itemView.each([&](entt::entity entity, const auto& item, const auto& pos) {
      // Frustum Culling
      if (!CheckCollisionPointRec({pos.x, pos.y}, viewRect)) return;

      Color rarityColor = UISystem::GetRarityColor(item.rarity);
      float scale = 1.0f;
      bool emphasized = false;

      const auto* filterResult = registry.try_get<NoMoreDay::LootFilterResultComponent>(entity);
      if (filterResult) {
          if (!filterResult->visible) return;
          if (filterResult->scale > 1.0f) {
              scale = filterResult->scale;
              emphasized = true;
              rarityColor = filterResult->color;
          }
      }

      int fontSize = (int)(18.0f * scale * fontScale);
      if (fontSize < 12) fontSize = 12;

      auto& labelCache = registry.get_or_emplace<LabelCacheComponent>(entity);
      if (!labelCache.isValid || labelCache.lastFontSize != fontSize) {
          labelCache.cachedSize = IsFontValid(font) ? MeasureTextEx(font, item.name.c_str(), (float)fontSize, 1.0f) : Vector2{(float)MeasureText(item.name.c_str(), fontSize), (float)fontSize};
          labelCache.lastFontSize = fontSize;
          labelCache.isValid = true;
      }
      
      s_visibleItems.push_back({entity, pos, rarityColor, item.rarity, scale, emphasized, item.name.c_str(), labelCache.cachedSize});
  });

  // 2. Gold Collection Pass
  auto goldView = registry.view<GoldComponent, Position>();
  goldView.each([&](entt::entity entity, const auto& gold, const auto& pos) {
      if (!CheckCollisionPointRec({pos.x, pos.y}, viewRect)) return;

      int fontSize = (int)(16.0f * fontScale);
      if (fontSize < 10) fontSize = 10;
      
      auto& labelCache = registry.get_or_emplace<LabelCacheComponent>(entity);
      if (!labelCache.isValid || labelCache.lastFontSize != fontSize) {
          // Format and cache string inside LabelCacheComponent
          snprintf(labelCache.cachedText, sizeof(labelCache.cachedText), "%d 金币", gold.amount);
          labelCache.cachedSize = IsFontValid(font) ? MeasureTextEx(font, labelCache.cachedText, (float)fontSize, 1.0f) : Vector2{(float)MeasureText(labelCache.cachedText, fontSize), (float)fontSize};
          labelCache.lastFontSize = fontSize;
          labelCache.isValid = true;
      }
      s_visibleGold.push_back({pos, labelCache.cachedText, labelCache.cachedSize});
  });

  // 3. Rendering Pass: Beams (Pure vector iteration, no registry lookups)
  for (const auto& item : s_visibleItems) {
      if (item.rarity >= NoMoreDay::Rarity::Rare || item.emphasized) {
          float time = (float)GetTime();
          float alpha = 0.45f + 0.15f * std::sin(time * 3.0f);
          float beamHeight = 120.0f * item.scale;
          float beamWidth = 24.0f * item.scale;

          Color colBurst = item.rarityColor;
          colBurst.a = (unsigned char)(255 * alpha);

          DrawRectangleGradientV((int)(item.pos.x - beamWidth * 0.5f), (int)(item.pos.y - beamHeight), (int)beamWidth, (int)beamHeight, Fade(item.rarityColor, 0.0f), colBurst);
          DrawCircleGradient((int)item.pos.x, (int)item.pos.y, beamWidth * 0.8f, colBurst, Fade(colBurst, 0.0f));

          if (item.rarity == NoMoreDay::Rarity::Legendary) {
              Color coreCol = ColorAlpha(WHITE, alpha * 0.7f);
              DrawRectangleGradientV((int)(item.pos.x - 2 * item.scale), (int)(item.pos.y - beamHeight), (int)(4 * item.scale), (int)beamHeight, Fade(WHITE, 0.0f), coreCol);
          }
      }
  }

  // 4. Rendering Pass: Background Boxes
  for (const auto& item : s_visibleItems) {
      Vector2 textPos = {item.pos.x - item.textSize.x / 2.0f, item.pos.y - 30.0f * item.scale};
      DrawRectangleRec({textPos.x - 4, textPos.y - 2, item.textSize.x + 8, item.textSize.y + 4}, Fade(BLACK, 0.7f));
  }
  for (const auto& gold : s_visibleGold) {
      Vector2 textPos = {gold.pos.x - gold.textSize.x / 2.0f, gold.pos.y - 25.0f};
      DrawRectangleRec({textPos.x - 4, textPos.y - 2, gold.textSize.x + 8, gold.textSize.y + 4}, Fade(BLACK, 0.6f));
  }

  // 5. Rendering Pass: Borders
  for (const auto& item : s_visibleItems) {
      Vector2 textPos = {item.pos.x - item.textSize.x / 2.0f, item.pos.y - 30.0f * item.scale};
      bool isHovered = (item.entity == UISystem::State.hoveredItem);
      if (isHovered) {
          DrawRectangleLinesEx({textPos.x - 4, textPos.y - 2, item.textSize.x + 8, item.textSize.y + 4}, 2.0f, WHITE);
      } else {
          DrawRectangleLinesEx({textPos.x - 4, textPos.y - 2, item.textSize.x + 8, item.textSize.y + 4}, 1.0f, ColorAlpha(item.rarityColor, 0.5f));
      }
  }

  // 6. Rendering Pass: Text (Max Batching)
  if (IsFontValid(font)) {
      for (const auto& item : s_visibleItems) {
          Vector2 textPos = {item.pos.x - item.textSize.x / 2.0f, item.pos.y - 30.0f * item.scale};
          float baseItemFontSize = 18.0f;
          int fontSize = (int)(baseItemFontSize * item.scale * fontScale);
          if (fontSize < 12) fontSize = 12;
          DrawTextEx(font, item.name, textPos, (float)fontSize, 1.0f, item.rarityColor);
      }
      for (const auto& gold : s_visibleGold) {
          Vector2 textPos = {gold.pos.x - gold.textSize.x / 2.0f, gold.pos.y - 25.0f};
          float baseGoldFontSize = 16.0f;
          int fontSize = (int)(baseGoldFontSize * fontScale);
          if (fontSize < 10) fontSize = 10;
          DrawTextEx(font, gold.text, textPos, (float)fontSize, 1.0f, GOLD);
      }
  } else {
      for (const auto& item : s_visibleItems) {
          Vector2 textPos = {item.pos.x - item.textSize.x / 2.0f, item.pos.y - 30.0f * item.scale};
          DrawText(item.name, (int)textPos.x, (int)textPos.y, (int)(18 * item.scale), item.rarityColor);
      }
  }

  // 6. Debug: GPU Flow Field Visualization
  auto &flowSystem = NoMoreDay::systems::GPUFlowFieldSystem::Get();
  if (flowSystem.m_debugDraw) {
    flowSystem.SyncToCPU();
    const std::vector<Vector2>& flowField = flowSystem.GetFlowFieldCPU();
    int width = flowSystem.GetWidth();
    int height = flowSystem.GetHeight();
    Vector2 origin = flowSystem.GetGridOrigin();
    // Assuming cell size is 64x64 or similar. Need to know cell size or assume
    // 1 unit = 1 pixel if logic dictates. Spec says "Cell Specification: Each
    // cell represents a 64x64 pixel area". But in GPUFlowFieldSystem::Update,
    // we passed world coords. Let's assume 64.0f for now.
    float cellSize = 64.0f;

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int index = y * width + x;
        Vector2 flow = flowField[index];

        if (Vector2Length(flow) > 0.01f) {
          float posX = origin.x + x * cellSize + cellSize * 0.5f;
          float posY = origin.y + y * cellSize + cellSize * 0.5f;

          Vector2 start = {posX, posY};
          Vector2 end = {posX + flow.x * (cellSize * 0.4f),
                         posY + flow.y * (cellSize * 0.4f)};

          DrawLineV(start, end, GREEN);
          DrawCircleV(end, 2.0f, GREEN); // Arrow head
        }
      }
    }

    // Draw Grid Bounds
    DrawRectangleLines(origin.x, origin.y, width * cellSize, height * cellSize,
                       RED);
  } // End of pixelView loop

  // 6. Projectiles Submission (Dedicated Loop)
  // Iterate ALL projectiles, regardless of GPUIndex or SpriteComponent or
  // ColorComponent We want to render them via GPUSkillEffectSystem if they
  // exist.
  auto projView = registry.view<const Position, NoMoreDay::Projectile>();
  for (auto entity : projView) {
    const auto &pos = projView.get<const Position>(entity);
    auto &proj = projView.get<NoMoreDay::Projectile>(entity);

    proj.hasRendered = true; // Mark as visible

    NoMoreDay::components::GPUSkillEffect effect;

    // Extrapolate
    float ax = pos.x;
    float ay = pos.y;
    if (auto *vel = registry.try_get<Velocity>(entity)) {
      ax += vel->vx * context.renderAccumulator;
      ay += vel->vy * context.renderAccumulator;
      effect.velocity = {vel->vx, vel->vy};
    }
    effect.position = {ax, ay};

    // Radius & Angle
    effect.radius = (proj.radius > 1.0f) ? proj.radius : 5.0f;
    effect.sectorAngle = (proj.arcWidth > 0.0f) ? proj.arcWidth : 45.0f;
    effect.type = (float)proj.visualType;
    effect.softness = 0.5f;

    // Color - Try get ColorComponent, else default
    if (auto *col = registry.try_get<ColorComponent>(entity)) {
      effect.coreColor = ColorNormalize(col->color);
      effect.glowColor = ColorNormalize(col->color);
    } else {
      effect.coreColor = {1.0f, 1.0f, 1.0f, 1.0f}; // White default
      effect.glowColor = {0.8f, 0.8f, 1.0f, 0.5f};
    }

    NoMoreDay::systems::GPUSkillEffectSystem::Get().Submit(effect);
  }

  // GPU Skill Effects
  NoMoreDay::systems::GPUSkillEffectSystem::Get().Render(camera);

  // 7. 高性能伤害飘字
  NoMoreDay::DamagePopupManager::Get().Draw(font);
}
