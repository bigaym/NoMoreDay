#include "engine/render/RenderSystem.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "engine/physics/SIMDSpatialGrid.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "engine/render/ComputeBuffer.hpp" 
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/PopupRenderer.hpp"
#include "engine/render/RenderConstants.hpp" 
#include "engine/render/RenderContext.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/Projectile.hpp"     
#include "game/components/SkillDefs.hpp"      
#include "game/components/StashComponent.hpp" 
#include "game/components/vfx/HoloBladeComponent.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp" 
#include "game/systems/item/LootFilter.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/vfx/HoloBladeRenderSystem.hpp"
#include "game/systems/vfx/SwordIntentVisualSystem.hpp"
#include "game/systems/vfx/TrailSystem.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "game/systems/ui/PlayerHUD.hpp"
#include "rlgl.h"

// Static Buffers
std::vector<NoMoreDay::components::GPULabelInstance>
    RenderSystem::s_labelBuffer;
std::vector<RenderSystem::TextRenderCmd> RenderSystem::s_textQueue;
std::vector<RenderSystem::VisibleItemCache::ItemData>
    RenderSystem::VisibleItemCache::visibleItems; 

// Static Members Definition
float RenderSystem::s_trauma = 0.0f;
Shader RenderSystem::s_labelShader = {0};
int RenderSystem::s_labelMvpLoc = -1;
std::unique_ptr<NoMoreDay::core::ComputeBuffer>
    RenderSystem::s_labelInstanceBuffer = nullptr;

// Screen Shake Implementation
void RenderSystem::AddScreenShake(float intensity) {
    s_trauma = std::clamp(s_trauma + intensity, 0.0f, 1.0f);
}

void RenderSystem::UpdateShake(float dt) {
    if (s_trauma > 0.0f) {
        s_trauma = std::max(0.0f, s_trauma - dt * 1.5f);
    }
}

Vector2 RenderSystem::GetShakeOffset() {
    if (s_trauma <= 0.0f) return {0, 0};
    float shake = s_trauma * s_trauma;
    float maxOffset = 15.0f * shake;
    return {
        (float)NoMoreDay::utils::ThreadSafeRandom::GetFloat(-1.0f, 1.0f) * maxOffset,
        (float)NoMoreDay::utils::ThreadSafeRandom::GetFloat(-1.0f, 1.0f) * maxOffset
    };
}

// Phase 4: Loot Label Spatial Optimization
std::unique_ptr<NoMoreDay::systems::SIMDSpatialGrid> RenderSystem::s_itemGrid = nullptr;
bool RenderSystem::s_itemGridDirty = true;

// Phase 2: Beam Instancing
struct GPUBeamInstance {
  Vector2 position;
  Vector2 size;
  Vector4 color;
  float time;
  float padding[3];
};
static Shader s_beamShader = {0};
static int s_beamMvpLoc = -1;
static std::unique_ptr<NoMoreDay::core::ComputeBuffer> s_beamInstanceBuffer =
    nullptr;
static std::vector<GPUBeamInstance> s_beamBuffer;

void RenderSystem::Initialize() {
  s_labelShader = LoadShader("assets/shaders/ui/label_instanced.vert",
                             "assets/shaders/ui/label_instanced.frag");

  if (s_labelShader.id != 0) {
    s_labelMvpLoc = GetShaderLocation(s_labelShader, "mvp");
  }

  using NoMoreDay::RenderConstants::Binding;
  s_labelInstanceBuffer = std::make_unique<NoMoreDay::core::ComputeBuffer>();
  s_labelInstanceBuffer->Create(
      1000 * sizeof(NoMoreDay::components::GPULabelInstance), nullptr,
      RL_DYNAMIC_DRAW);
  s_labelInstanceBuffer->BindBase(
      static_cast<uint32_t>(Binding::SSBO_LABEL_INSTANCE));

  s_itemGrid = std::make_unique<NoMoreDay::systems::SIMDSpatialGrid>(256, 256, 128.0f);
  s_itemGridDirty = true;

  s_beamShader = LoadShader("assets/shaders/vfx/beam_instanced.vert",
                            "assets/shaders/vfx/beam_instanced.frag");
  if (s_beamShader.id != 0) {
    s_beamMvpLoc = GetShaderLocation(s_beamShader, "mvp");
  }

  s_beamInstanceBuffer = std::make_unique<NoMoreDay::core::ComputeBuffer>();
  s_beamInstanceBuffer->Create(500 * sizeof(GPUBeamInstance), nullptr,
                               RL_DYNAMIC_DRAW);
}

void RenderSystem::Shutdown() {
  if (s_labelShader.id != 0) {
    UnloadShader(s_labelShader);
    s_labelShader.id = 0;
  }
  s_labelInstanceBuffer = nullptr;

  if (s_beamShader.id != 0) {
    UnloadShader(s_beamShader);
    s_beamShader.id = 0;
  }
  s_beamInstanceBuffer = nullptr;
  s_itemGrid = nullptr;
}

void RenderSystem::render(entt::registry &registry,
                          const NoMoreDay::SharedContext &context,
                          const Camera2D &camera) {
  float cameraZoom = (context.settings) ? context.settings->cameraZoom : 1.5f;
  float fontScale = 1.0f / cameraZoom;

  // 0. VFX: Trails
  static Shader trailShader = {0};
  if (trailShader.id == 0 && context.resources) {
    trailShader =
        context.resources->getShader(entt::hashed_string("sh_sword_trail"));
  }
  if (trailShader.id != 0) {
    NoMoreDay::systems::TrailSystem::Render(registry, trailShader);
  }

  NoMoreDay::systems::SwordIntentVisualSystem::Render(registry);

  // Render Stash
  auto playerView = registry.view<PlayerTag, Position>();
  Vector2 playerPos = {0, 0};
  bool hasPlayer = false;
  if (playerView.begin() != playerView.end()) {
    playerPos = {playerView.get<Position>(playerView.front()).x,
                 playerView.get<Position>(playerView.front()).y};
    hasPlayer = true;
  }

  auto stashView =
      registry.view<const Position, const NoMoreDay::StashPlaceholderRender>();
  for (auto entity : stashView) {
    const auto &pos = stashView.get<Position>(entity);
    const auto &render = stashView.get<NoMoreDay::StashPlaceholderRender>(entity);
    DrawRectangle((int)pos.x, (int)pos.y, (int)render.WIDTH, (int)render.HEIGHT, render.color);
    const char *label = "Stash";
    if (auto *interact = registry.try_get<NoMoreDay::StashInteractableComponent>(entity)) {
      if (interact->type == NoMoreDay::StashType::Shared) label = "Shared Stash";
    }
    DrawText(label, (int)pos.x, (int)pos.y - 20, 20, WHITE);
    if (hasPlayer) {
      float dx = pos.x - playerPos.x;
      float dy = pos.y - playerPos.y;
      if (dx * dx + dy * dy < 100.0f * 100.0f) DrawText("Press E", (int)pos.x, (int)pos.y - 45, 24, YELLOW);
    }
  }

  // 1. Sprites (CPU path for non-GPU entities)
  {
    auto spriteView = registry.view<const Position, const SpriteComponent>(
        entt::exclude<NoMoreDay::components::HoloBlade, NoMoreDay::ItemComponent, GoldComponent>);

    for (auto entity : spriteView) {
      const auto &[pos, sprite] = spriteView.get(entity);
      bool isPlayer = registry.any_of<PlayerTag>(entity);
      
      // Skip if handled by GPU MDI
      if (!isPlayer && registry.any_of<GPUIndex>(entity)) {
        if (sprite.textureLayerIndex >= 0) continue;
      }

      float width = (float)sprite.texture.width * sprite.scale;
      float height = (float)sprite.texture.height * sprite.scale;
      Vector2 origin = {width / 2.0f, height / 2.0f};
      Rectangle source = {0.0f, 0.0f, (float)sprite.texture.width, (float)sprite.texture.height};

      float renderX = pos.x;
      float renderY = pos.y;
      if (!isPlayer && registry.any_of<GPUIndex>(entity)) {
        if (auto *prevPos = registry.try_get<PrevPosition>(entity)) {
          renderX = Lerp(prevPos->x, pos.x, context.renderAlpha);
          renderY = Lerp(prevPos->y, pos.y, context.renderAlpha);
        }
      }

      Rectangle dest = {renderX, renderY, width, height};
      if (registry.any_of<NoMoreDay::ShadowVisualComponent>(entity)) {
        DrawEllipse((int)renderX, (int)(renderY + height * 0.4f), width * 0.32f, height * 0.12f, Fade(BLACK, 0.3f));
      }
      DrawTexturePro(sprite.texture, source, dest, origin, 0.0f, WHITE);
    }
  }

  // 1.5 GPU MDI Entity Rendering
  {
      NoMoreDay::systems::GPUEntitySystem::Get().Render(context, camera);
  }

  // GPU 伤害飘字渲染
  {
    Matrix viewProj = NoMoreDay::systems::GPUParticleSystem::Get().BuildMVP(camera);
    NoMoreDay::render::PopupRenderer::Get().Render(viewProj);
  }

  // GPU 粒子渲染
  {
      NoMoreDay::systems::GPUParticleSystem::Get().Render(camera);
  }

  // 2. Shapes
  auto pixelView = registry.view<const Position, const ColorComponent>(entt::exclude<SpriteComponent>);
  for (auto entity : pixelView) {
    if (registry.any_of<GPUIndex>(entity) && !registry.any_of<NoMoreDay::Projectile>(entity)) continue;
    auto pos = pixelView.get<Position>(entity);
    const auto &col = pixelView.get<ColorComponent>(entity);
    uint32_t id = (uint32_t)entity;
    pos.x += (float)((id % 11) - 5) * 1.5f;
    pos.y += (float)((id % 7) - 3) * 1.5f;
    if (registry.any_of<NoMoreDay::Projectile>(entity)) continue;
    DrawCircle((int)pos.x, (int)pos.y, 8.0f, col.color);
  }

  // 2.3. Molten Trails
  auto moltenView = registry.view<const Position, const NoMoreDay::MoltenTrailTag, const Radius, const ColorComponent, const DelayedDestroyComponent>();
  for (auto entity : moltenView) {
    const auto &pos = moltenView.get<Position>(entity);
    const auto &radius = moltenView.get<Radius>(entity);
    const auto &color = moltenView.get<ColorComponent>(entity);
    const auto &delayed = moltenView.get<DelayedDestroyComponent>(entity);
    float alpha = std::clamp(delayed.timer / 3.0f, 0.0f, 1.0f);
    Color coreColor = color.color; coreColor.a = (unsigned char)(180 * alpha);
    DrawCircleGradient((int)pos.x, (int)pos.y, radius.value, coreColor, Fade(coreColor, 0.0f));
    DrawRing({pos.x, pos.y}, radius.value * 0.8f, radius.value, 0, 360, 16, {255, 50, 0, (unsigned char)(100 * alpha)});
  }

  NoMoreDay::systems::HoloBladeRenderSystem::Render(registry, context);

  // 3. Attack Effects
  auto effectView = registry.view<const Position, const AttackEffect>();
  effectView.each([](const auto &pos, const auto &effect) {
    float alpha = 1.0f - (effect.timer / effect.lifeTime);
    float startAngle = effect.rotation - (effect.arcAngle / 2.0f);
    float endAngle = effect.rotation + (effect.arcAngle / 2.0f);
    DrawCircleSector({pos.x, pos.y}, effect.range, startAngle, endAngle, 10, Fade(effect.color, 0.5f * alpha));
  });

  // 3.5. Visual Effects
  auto vfxView = registry.view<const Position, VisualEffect>();
  for (auto entity : vfxView) {
    const auto &pos = vfxView.get<const Position>(entity);
    auto &effect = vfxView.get<VisualEffect>(entity);
    float lifeRatio = effect.timer / effect.lifeTime;
    float currentScale = Lerp(effect.startScale, effect.endScale, lifeRatio);
    Color color = effect.color; color.a = (unsigned char)((float)color.a * (1.0f - lifeRatio));

    switch (effect.type) {
    case VisualEffectType::Pickup: {
      float r = currentScale * 30.0f;
      DrawRing({pos.x, pos.y}, r, r + 2.0f + (1.0f - lifeRatio) * 3.0f, 0, 360, 32, color);
      break;
    }
    case VisualEffectType::DropPillar: {
      float w = 30.0f * (1.0f - lifeRatio);
      float h = effect.param1 > 0 ? effect.param1 : 150.0f;
      DrawRectangleGradientV((int)(pos.x - w / 2), (int)(pos.y - h), (int)w, (int)h, Fade(WHITE, 0.0f), color);
      DrawCircleGradient((int)pos.x, (int)pos.y, w, color, Fade(color, 0.0f));
      break;
    }
    case VisualEffectType::GoldSparkle: {
      DrawPoly({pos.x, pos.y}, 4, 15.0f * currentScale, lifeRatio * 180.0f, color);
      break;
    }
    default: break;
    }
  }

  // 4. Damage Popups
  Font font = UISystem::GetFont();
  auto popupView = registry.view<const Position, const DamagePopup>();
  popupView.each([&font, fontScale](const auto &pos, const auto &popup) {
    float alpha = (popup.timer > popup.lifeTime * 0.5f) ? 1.0f - ((popup.timer - popup.lifeTime * 0.5f) / (popup.lifeTime * 0.5f)) : 1.0f;
    Color color = popup.color; color.a = (unsigned char)(255 * alpha);
    const char *text = popup.isStatus ? popup.statusText.c_str() : (popup.isDodge ? "闪避" : (popup.isMiss ? "未命中" : (popup.isBlock ? TextFormat("格挡 %d", (int)popup.damage) : TextFormat("%d", (int)popup.damage))));
    float fontSize = (popup.isCrit ? 36.0f : 28.0f) * popup.currentScale * fontScale;
    if (IsFontValid(font)) {
      DrawTextEx(font, text, {pos.x + 2, pos.y + 2}, fontSize, 1.0f, Fade(BLACK, alpha * 0.8f));
      DrawTextEx(font, text, {pos.x, pos.y}, fontSize, 1.0f, color);
    }
  });

  // 5. Loot Labels (Optimized)
  {
    NoMoreDay::utils::ScopedTimer itemTimer("Loot Label Collection", 100);
    s_labelBuffer.clear(); s_textQueue.clear(); s_beamBuffer.clear();
    VisibleItemCache::Clear();

    Vector2 vTL = GetScreenToWorld2D({0, 0}, camera);
    Vector2 vBR = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);
    Rectangle viewRect = {vTL.x - 100, vTL.y - 100, (vBR.x - vTL.x) + 200, (vBR.y - vTL.y) + 200};

    static Mesh quadMesh = {0};
    if (quadMesh.vertexCount == 0) {
      Mesh mesh = {0}; mesh.triangleCount = 2; mesh.vertexCount = 6;
      mesh.vertices = (float *)MemAlloc(18 * sizeof(float));
      mesh.texcoords = (float *)MemAlloc(12 * sizeof(float));
      float v[] = {0,0,0, 0,1,0, 1,1,0, 0,0,0, 1,1,0, 1,0,0};
      float t[] = {0,0, 0,1, 1,1, 0,0, 1,1, 1,0};
      memcpy(mesh.vertices, v, 18*sizeof(float)); memcpy(mesh.texcoords, t, 12*sizeof(float));
      UploadMesh(&mesh, false); quadMesh = mesh;
    }

    int labelCount = 0;
    if (s_itemGrid) {
        s_itemGrid->query({camera.target.x, camera.target.y}, 1500.0f, [&](entt::entity entity, const Vector2& pos) {
            if (labelCount >= 64 || !CheckCollisionPointRec({pos.x, pos.y}, viewRect)) return;

            if (const auto *item = registry.try_get<NoMoreDay::ItemComponent>(entity)) {
                const auto *filterResult = registry.try_get<NoMoreDay::LootFilterResultComponent>(entity);
                if (labelCount > 32 && item->rarity < NoMoreDay::Rarity::Rare && (!filterResult || filterResult->scale <= 1.0f)) return;

                Color rarityColor = UISystem::GetRarityColor(item->rarity);
                float scale = 1.0f; bool emphasized = false;
                if (filterResult) {
                    if (!filterResult->visible) return;
                    if (filterResult->scale > 1.0f) { scale = filterResult->scale; emphasized = true; rarityColor = filterResult->color; }
                }

                auto &labelCache = registry.get_or_emplace<LabelCacheComponent>(entity);

                labelCount++;
                int fSize = (int)(18.0f * scale * fontScale); if (fSize < 12) fSize = 12;
                if (!labelCache.isValid || labelCache.lastFontSize != fSize || labelCache.lastRarityHash != (uint32_t)item->rarity) {
                    labelCache.cachedSize = IsFontValid(font) ? MeasureTextEx(font, item->name.c_str(), (float)fSize, 1.0f) : Vector2{(float)MeasureText(item->name.c_str(), fSize), (float)fSize};
                    labelCache.lastFontSize = fSize; labelCache.lastRarityHash = (uint32_t)item->rarity; labelCache.isValid = true;
                }

                Vector2 tSize = labelCache.cachedSize;
                Rectangle bg = {pos.x - tSize.x/2 - 4, pos.y - 30.0f*scale - 2, tSize.x + 8, tSize.y + 4};

                if (item->rarity >= NoMoreDay::Rarity::Rare || emphasized) {
                    GPUBeamInstance bi; bi.position = {pos.x, pos.y}; bi.size = {24.0f*scale, 120.0f*scale};
                    bi.color = ColorNormalize(rarityColor); bi.time = (float)GetTime(); s_beamBuffer.push_back(bi);
                }

                bool hovered = (entity == UISystem::State.hoveredItem);
                NoMoreDay::components::GPULabelInstance inst;
                inst.position = {bg.x, bg.y}; inst.size = {bg.width, bg.height};
                inst.bgColor = ColorNormalize(Fade(BLACK, 0.7f));
                inst.borderColor = ColorNormalize(hovered ? WHITE : ColorAlpha(rarityColor, 0.5f));
                inst.borderWidth = hovered ? 2.0f : 1.0f; inst.cornerRadius = 4.0f;
                s_labelBuffer.push_back(inst);
                VisibleItemCache::visibleItems.push_back({entity, bg});
                s_textQueue.push_back({{pos.x - tSize.x/2, pos.y - 30.0f*scale}, item->name.c_str(), (float)fSize, rarityColor, false});
            } else if (const auto *gold = registry.try_get<GoldComponent>(entity)) {
                if (labelCount > 48 && gold->amount < 100) return;
                auto &labelCache = registry.get_or_emplace<LabelCacheComponent>(entity);
                labelCount++;
                int fSize = (int)(16.0f * fontScale); if (fSize < 10) fSize = 10;
                if (!labelCache.isValid || labelCache.lastFontSize != fSize) {
                    if (!labelCache.isValid) snprintf(labelCache.cachedText, sizeof(labelCache.cachedText), "%d 金币", gold->amount);
                    labelCache.cachedSize = IsFontValid(font) ? MeasureTextEx(font, labelCache.cachedText, (float)fSize, 1.0f) : Vector2{(float)MeasureText(labelCache.cachedText, fSize), (float)fSize};
                    labelCache.lastFontSize = fSize; labelCache.isValid = true;
                }
                Vector2 tSize = labelCache.cachedSize;
                Rectangle bg = {pos.x - tSize.x/2 - 4, pos.y - 25.0f - 2, tSize.x + 8, tSize.y + 4};
                NoMoreDay::components::GPULabelInstance inst;
                inst.position = {bg.x, bg.y}; inst.size = {bg.width, bg.height};
                inst.bgColor = ColorNormalize(Fade(BLACK, 0.6f)); inst.borderColor = ColorNormalize(Fade(GOLD, 0.5f));
                inst.borderWidth = 1.0f; inst.cornerRadius = 4.0f; s_labelBuffer.push_back(inst);
                s_textQueue.push_back({{pos.x - tSize.x/2, pos.y - 25.0f}, labelCache.cachedText, (float)fSize, GOLD, false});
            }
        });
    }

    if (!s_beamBuffer.empty() && s_beamShader.id != 0 && s_beamInstanceBuffer) {
      size_t sz = s_beamBuffer.size() * sizeof(GPUBeamInstance);
      if (sz > s_beamInstanceBuffer->GetSize()) s_beamInstanceBuffer->Create(sz * 2, s_beamBuffer.data(), RL_DYNAMIC_DRAW);
      else s_beamInstanceBuffer->OrphanAndUpload(s_beamBuffer.data(), sz, RL_DYNAMIC_DRAW);
      s_beamInstanceBuffer->BindBase(static_cast<uint32_t>(NoMoreDay::RenderConstants::Binding::SSBO_BEAM_INSTANCE));
      BeginShaderMode(s_beamShader);
      Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
      SetShaderValueMatrix(s_beamShader, s_beamMvpLoc, mvp);
      rlEnableVertexArray(quadMesh.vaoId); rlDrawVertexArrayInstanced(0, 6, (int)s_beamBuffer.size()); rlDisableVertexArray();
      EndShaderMode();
    }

    if (!s_labelBuffer.empty() && s_labelShader.id != 0 && s_labelInstanceBuffer) {
      size_t sz = s_labelBuffer.size() * sizeof(NoMoreDay::components::GPULabelInstance);
      if (sz > s_labelInstanceBuffer->GetSize()) s_labelInstanceBuffer->Create(sz * 2, s_labelBuffer.data(), RL_DYNAMIC_DRAW);
      else s_labelInstanceBuffer->OrphanAndUpload(s_labelBuffer.data(), sz, RL_DYNAMIC_DRAW);
      s_labelInstanceBuffer->BindBase(static_cast<uint32_t>(NoMoreDay::RenderConstants::Binding::SSBO_LABEL_INSTANCE));
      BeginShaderMode(s_labelShader);
      Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
      SetShaderValueMatrix(s_labelShader, s_labelMvpLoc, mvp);
      rlEnableVertexArray(quadMesh.vaoId); rlDrawVertexArrayInstanced(0, 6, (int)s_labelBuffer.size()); rlDisableVertexArray();
      EndShaderMode();
    }

    if (IsFontValid(font)) {
      for (const auto &cmd : s_textQueue) DrawTextEx(font, cmd.text, cmd.position, cmd.fontSize, 1.0f, cmd.color);
    }
  }

  {
    auto projView = registry.view<const Position, NoMoreDay::Projectile>();
    for (auto entity : projView) {
      const auto &pos = projView.get<const Position>(entity);
      auto &proj = projView.get<NoMoreDay::Projectile>(entity);
      proj.hasRendered = true;
      NoMoreDay::components::GPUSkillEffect eff;
      float ax = pos.x, ay = pos.y;
      if (auto *vel = registry.try_get<Velocity>(entity)) {
        ax += vel->vx * context.renderAlpha * (1.0f / 60.0f);
        ay += vel->vy * context.renderAlpha * (1.0f / 60.0f);
        eff.velocity = {vel->vx, vel->vy};
      }
      eff.position = {ax, ay};
      eff.radius = (proj.radius > 1.0f) ? proj.radius : 5.0f;
      eff.sectorAngle = (proj.arcWidth > 0.0f) ? proj.arcWidth : 45.0f;
      eff.type = (float)proj.visualType; eff.softness = 0.5f;
      if (auto *col = registry.try_get<ColorComponent>(entity)) { eff.coreColor = ColorNormalize(col->color); eff.glowColor = ColorNormalize(col->color); }
      else { eff.coreColor = {1,1,1,1}; eff.glowColor = {0.8f,0.8f,1,0.5f}; }
      NoMoreDay::systems::GPUSkillEffectSystem::Get().Submit(eff);
    }
  }
  NoMoreDay::systems::GPUSkillEffectSystem::Get().Render(camera);
}
