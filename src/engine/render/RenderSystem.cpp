#include "engine/render/RenderSystem.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "engine/physics/SIMDSpatialGrid.hpp"
#include "engine/physics/SpatialGrid.hpp"
#include "engine/render/ComputeBuffer.hpp" 
#include "engine/render/GPUEntitySystem.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUABIContract.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/trail/GPUTrailRenderer.hpp"
#include "engine/render/dev/ShaderHotReloadManager.hpp"
#include "engine/render/debug/RenderProfiler.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/passes/LightingPass.hpp"
#include "engine/render/passes/PostProcessPass.hpp"
#include "engine/render/passes/VolumetricLightPass.hpp"
#include "engine/render/PopupRenderer.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"
#include "engine/render/resources/TransientResourcePool.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/ScopedGLState.hpp"
#include "engine/render/RenderConstants.hpp" 
#include "engine/render/RenderContext.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/Projectile.hpp"     
#include "game/components/SkillDefs.hpp"      
#include "game/components/StashComponent.hpp" 
#include "game/components/vfx/HoloBladeComponent.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/combat/DamagePopupManager.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp" 
#include "game/systems/item/LootFilter.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/vfx/HoloBladeRenderSystem.hpp"
#include "game/systems/vfx/SwordIntentVisualSystem.hpp"
#include "game/systems/vfx/TrailSystem.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "engine/render/LootTextBatcher.hpp"
#include "game/systems/ui/PlayerHUD.hpp"
#include "rlgl.h"

// Static Buffers
std::vector<NoMoreDay::components::GPULabelInstance>
    RenderSystem::s_labelBuffer;
std::vector<NoMoreDay::components::GPUGlyphInstance>
    RenderSystem::s_glyphBuffer;
NoMoreDay::render::resources::FramebufferHandle RenderSystem::s_hdrSceneBuffer;
std::vector<RenderSystem::VisibleItemCache::ItemData>
    RenderSystem::VisibleItemCache::visibleItems; 

// Static Members Definition
float RenderSystem::s_trauma = 0.0f;
float RenderSystem::s_shakeMultiplier = 1.0f;
Shader RenderSystem::s_labelShader = {0};
int RenderSystem::s_labelMvpLoc = -1;
std::unique_ptr<NoMoreDay::core::ComputeBuffer>
    RenderSystem::s_labelInstanceBuffer = nullptr;

Shader RenderSystem::s_glyphShader = {0};
int RenderSystem::s_glyphMvpLoc = -1;
int RenderSystem::s_glyphTexLoc = -1;
std::unique_ptr<NoMoreDay::core::ComputeBuffer>
    RenderSystem::s_glyphInstanceBuffer = nullptr;

// Screen Shake Implementation
void RenderSystem::AddScreenShake(float intensity) {
    if (s_shakeMultiplier < 1e-4f) return;
    s_trauma = std::clamp(s_trauma + intensity * s_shakeMultiplier, 0.0f, 1.0f);
}

void RenderSystem::UpdateShake(float dt) {
    if (s_trauma > 1e-4f) {
        s_trauma = std::max(0.0f, s_trauma - dt * 1.5f);
    } else {
        s_trauma = 0.0f;
    }
}

Vector2 RenderSystem::GetShakeOffset() {
    if (s_trauma < 1e-4f || s_shakeMultiplier < 1e-4f) return {0, 0};
    float shake = s_trauma * s_trauma;
    float maxOffset = 15.0f * shake * s_shakeMultiplier;
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

namespace {

struct RenderFrameData {
  entt::registry &registry;
  const NoMoreDay::SharedContext &context;
  const Camera2D &camera;
  float cameraZoom = 1.5f;
  float fontScale = 1.0f;
  Font font = {};
  Vector2 playerPos = {0.0f, 0.0f};
  bool hasPlayer = false;
  bool limitEnemyVision = false;
  const FogOfWarSystem *fogSystem = nullptr;
  std::vector<NoMoreDay::components::GPULabelInstance> *labelBuffer = nullptr;
  std::vector<NoMoreDay::components::GPUGlyphInstance> *glyphBuffer = nullptr;
  Shader *labelShader = nullptr;
  Shader *glyphShader = nullptr;
  int labelMvpLoc = -1;
  int glyphMvpLoc = -1;
  int glyphTexLoc = -1;
  NoMoreDay::core::ComputeBuffer *labelInstanceBuffer = nullptr;
  NoMoreDay::core::ComputeBuffer *glyphInstanceBuffer = nullptr;
};

NoMoreDay::render::resources::TransientResourcePool g_transientPool;
std::shared_ptr<NoMoreDay::render::passes::PostProcessPass> g_postProcessPass;
std::shared_ptr<NoMoreDay::render::passes::LightingPass> g_lightingPass;
std::shared_ptr<NoMoreDay::render::passes::VolumetricLightPass> g_volumetricPass;
std::shared_ptr<NoMoreDay::render::passes::DistortionPass> g_distortionPass;
std::unique_ptr<NoMoreDay::render::debug::RenderProfiler> g_renderProfiler;
std::unique_ptr<NoMoreDay::render::dev::ShaderHotReloadManager> g_shaderHotReloadManager;

struct CompositeTargetState {
  uint32_t framebuffer = 0;
  int viewportX = 0;
  int viewportY = 0;
  int viewportWidth = 0;
  int viewportHeight = 0;
};

CompositeTargetState CaptureCompositeTargetState() {
  CompositeTargetState state = {};
  state.viewportWidth = GetScreenWidth();
  state.viewportHeight = GetScreenHeight();

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_43)
  constexpr uint32_t kGLFramebufferBinding = 0x8CA6;
  constexpr uint32_t kGLViewport = 0x0BA2;
  GLint boundFramebuffer = 0;
  GLint viewport[4] = {0, 0, state.viewportWidth, state.viewportHeight};
  glGetIntegerv(kGLFramebufferBinding, &boundFramebuffer);
  glGetIntegerv(kGLViewport, viewport);
  state.framebuffer = static_cast<uint32_t>(boundFramebuffer);
  state.viewportX = viewport[0];
  state.viewportY = viewport[1];
  state.viewportWidth = viewport[2];
  state.viewportHeight = viewport[3];
#endif

  return state;
}

bool IsHdrPostProcessRequested(
    const NoMoreDay::render::core::RenderConfig &config) {
  return config.bloomEnabled || config.fxaaEnabled || config.vignetteEnabled ||
         (config.colorGradingEnabled && config.colorGradingLutSize > 0);
}

bool IsHdrScenePipelineRequested(
    const NoMoreDay::render::core::RenderConfig &config) {
  return config.dynamicLightingEnabled || config.volumetricLightEnabled ||
         IsHdrPostProcessRequested(config);
}

Mesh &GetLabelQuadMesh() {
  static Mesh quadMesh = {0};
  if (quadMesh.vertexCount != 0) {
    return quadMesh;
  }

  Mesh mesh = {0};
  mesh.triangleCount = 2;
  mesh.vertexCount = 6;
  mesh.vertices = static_cast<float *>(MemAlloc(18 * sizeof(float)));
  mesh.texcoords = static_cast<float *>(MemAlloc(12 * sizeof(float)));

  const float vertices[] = {0, 0, 0, 0, 1, 0, 1, 1, 0,
                            0, 0, 0, 1, 1, 0, 1, 0, 0};
  const float texcoords[] = {0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0};
  std::memcpy(mesh.vertices, vertices, sizeof(vertices));
  std::memcpy(mesh.texcoords, texcoords, sizeof(texcoords));
  UploadMesh(&mesh, false);
  quadMesh = mesh;
  return quadMesh;
}

void BuildRenderFrameData(RenderFrameData &frame) {
  frame.cameraZoom =
      (frame.context.settings != nullptr) ? frame.context.settings->cameraZoom
                                          : 1.5f;
  frame.fontScale = (frame.cameraZoom > 1e-4f) ? (1.0f / frame.cameraZoom) : 1.0f;
  frame.font = UISystem::GetFont();

  auto playerView = frame.registry.view<PlayerTag, Position>();
  if (playerView.begin() != playerView.end()) {
    const auto &playerPosComp = playerView.get<Position>(playerView.front());
    frame.playerPos = {playerPosComp.x, playerPosComp.y};
    frame.hasPlayer = true;
  }

  if (frame.context.levelManager != nullptr) {
    const auto &biome = NoMoreDay::BiomeRegistry::Get().GetBiome(
        frame.context.levelManager->getCurrentBiomeID());
    frame.limitEnemyVision =
        biome.hasFeature(NoMoreDay::BiomeFeature::LimitedVision) &&
        biome.visionRadius > 0.0f;
    frame.fogSystem = &frame.context.levelManager->getFogSystem();
  }
}

void ExecuteScenePass(RenderFrameData &frame) {
  static Shader trailShader = {0};
  if (trailShader.id == 0 && frame.context.resources != nullptr) {
    trailShader = frame.context.resources->getShader(
        entt::hashed_string("sh_sword_trail"));
  }
  if (trailShader.id != 0) {
    NoMoreDay::systems::TrailSystem::Render(frame.registry, trailShader);
  }
  NoMoreDay::systems::SwordIntentVisualSystem::Render(frame.registry);

  auto stashView =
      frame.registry
          .view<const Position, const NoMoreDay::StashPlaceholderRender>();
  for (auto entity : stashView) {
    const auto &pos = stashView.get<Position>(entity);
    const auto &render =
        stashView.get<NoMoreDay::StashPlaceholderRender>(entity);
    DrawRectangle(static_cast<int>(pos.x), static_cast<int>(pos.y),
                  static_cast<int>(render.WIDTH), static_cast<int>(render.HEIGHT),
                  render.color);

    const char *label = "Stash";
    if (auto *interact =
            frame.registry.try_get<NoMoreDay::StashInteractableComponent>(
                entity)) {
      if (interact->type == NoMoreDay::StashType::Shared) {
        label = "Shared Stash";
      }
    }

    DrawText(label, static_cast<int>(pos.x), static_cast<int>(pos.y) - 20, 20,
             WHITE);
    if (frame.hasPlayer) {
      const float dx = pos.x - frame.playerPos.x;
      const float dy = pos.y - frame.playerPos.y;
      if (dx * dx + dy * dy < 100.0f * 100.0f) {
        DrawText("Press E", static_cast<int>(pos.x),
                 static_cast<int>(pos.y) - 45, 24, YELLOW);
      }
    }
  }

  auto spriteView = frame.registry.view<const Position, const SpriteComponent>(
      entt::exclude<NoMoreDay::components::HoloBlade, NoMoreDay::ItemComponent,
                    GoldComponent>);

  for (auto entity : spriteView) {
    const auto &[pos, sprite] = spriteView.get(entity);
    const bool isPlayer = frame.registry.any_of<PlayerTag>(entity);

    if (!isPlayer && frame.registry.any_of<GPUIndex>(entity) &&
        sprite.textureLayerIndex >= 0) {
      continue;
    }

    const float width = static_cast<float>(sprite.texture.width) * sprite.scale;
    const float height =
        static_cast<float>(sprite.texture.height) * sprite.scale;
    const Vector2 origin = {width / 2.0f, height / 2.0f};
    const Rectangle source = {0.0f, 0.0f, static_cast<float>(sprite.texture.width),
                              static_cast<float>(sprite.texture.height)};

    float renderX = pos.x;
    float renderY = pos.y;
    if (frame.limitEnemyVision && frame.fogSystem &&
        frame.registry.any_of<EnemyTag>(entity)) {
      using namespace NoMoreDay::Constants::World;
      const int gx = static_cast<int>(renderX / GRID_TILE_SIZE);
      const int gy = static_cast<int>(renderY / GRID_TILE_SIZE);
      if (!frame.fogSystem->isVisible(gx, gy)) {
        continue;
      }
    }

    if (!isPlayer && frame.registry.any_of<GPUIndex>(entity)) {
      if (const auto *prevPos = frame.registry.try_get<PrevPosition>(entity)) {
        renderX = Lerp(prevPos->x, pos.x, frame.context.renderAlpha);
        renderY = Lerp(prevPos->y, pos.y, frame.context.renderAlpha);
      }
    }

    const Rectangle dest = {renderX, renderY, width, height};
    if (frame.registry.any_of<NoMoreDay::ShadowVisualComponent>(entity)) {
      DrawEllipse(static_cast<int>(renderX),
                  static_cast<int>(renderY + height * 0.4f), width * 0.32f,
                  height * 0.12f, Fade(BLACK, 0.3f));
    }
    DrawTexturePro(sprite.texture, source, dest, origin, 0.0f, WHITE);
  }

  NoMoreDay::systems::GPUEntitySystem::Get().Render(frame.context, frame.camera);

  auto pixelView = frame.registry.view<const Position, const ColorComponent>(
      entt::exclude<SpriteComponent>);
  for (auto entity : pixelView) {
    if (frame.registry.any_of<GPUIndex>(entity) &&
        !frame.registry.any_of<NoMoreDay::Projectile>(entity)) {
      continue;
    }
    if (frame.registry.any_of<NoMoreDay::Projectile>(entity)) {
      continue;
    }

    auto pos = pixelView.get<Position>(entity);
    const auto &col = pixelView.get<ColorComponent>(entity);
    const uint32_t id = static_cast<uint32_t>(entity);
    pos.x += static_cast<float>((id % 11) - 5) * 1.5f;
    pos.y += static_cast<float>((id % 7) - 3) * 1.5f;
    DrawCircle(static_cast<int>(pos.x), static_cast<int>(pos.y), 8.0f,
               col.color);
  }

  auto moltenView = frame.registry.view<const Position, const NoMoreDay::MoltenTrailTag,
                                        const Radius, const ColorComponent,
                                        const DelayedDestroyComponent>();
  for (auto entity : moltenView) {
    const auto &pos = moltenView.get<Position>(entity);
    const auto &radius = moltenView.get<Radius>(entity);
    const auto &color = moltenView.get<ColorComponent>(entity);
    const auto &delayed = moltenView.get<DelayedDestroyComponent>(entity);
    const float alpha = std::clamp(delayed.timer / 3.0f, 0.0f, 1.0f);
    Color coreColor = color.color;
    coreColor.a = static_cast<unsigned char>(180 * alpha);
    DrawCircleGradient(static_cast<int>(pos.x), static_cast<int>(pos.y),
                       radius.value, coreColor, Fade(coreColor, 0.0f));
    DrawRing({pos.x, pos.y}, radius.value * 0.8f, radius.value, 0, 360, 16,
             {255, 50, 0, static_cast<unsigned char>(100 * alpha)});
  }

  NoMoreDay::systems::HoloBladeRenderSystem::Render(frame.registry, frame.context);
}

void ExecuteVFXPass(RenderFrameData &frame) {
  Matrix viewProj = NoMoreDay::systems::GPUParticleSystem::Get().BuildMVP(
      frame.camera);
  // Keep VFX order stable: particles -> trails -> effect overlays.
  NoMoreDay::systems::GPUParticleSystem::Get().Render(frame.camera);
  NoMoreDay::render::GPUTrailRenderer::Get().Render(frame.camera);
  NoMoreDay::render::PopupRenderer::Get().Render(viewProj);

  auto effectView = frame.registry.view<const Position, const AttackEffect>();
  effectView.each([](const auto &pos, const auto &effect) {
    const float alpha = 1.0f - (effect.timer / effect.lifeTime);
    const float startAngle = effect.rotation - (effect.arcAngle / 2.0f);
    const float endAngle = effect.rotation + (effect.arcAngle / 2.0f);
    DrawCircleSector({pos.x, pos.y}, effect.range, startAngle, endAngle, 10,
                     Fade(effect.color, 0.5f * alpha));
  });

  auto vfxView = frame.registry.view<const Position, VisualEffect>();
  for (auto entity : vfxView) {
    const auto &pos = vfxView.get<const Position>(entity);
    auto &effect = vfxView.get<VisualEffect>(entity);
    const float lifeRatio = effect.timer / effect.lifeTime;
    const float currentScale = Lerp(effect.startScale, effect.endScale, lifeRatio);
    Color color = effect.color;
    color.a = static_cast<unsigned char>(static_cast<float>(color.a) *
                                         (1.0f - lifeRatio));

    switch (effect.type) {
    case VisualEffectType::Pickup: {
      const float r = currentScale * 30.0f;
      DrawRing({pos.x, pos.y}, r, r + 2.0f + (1.0f - lifeRatio) * 3.0f, 0, 360,
               32, color);
      break;
    }
    case VisualEffectType::DropPillar: {
      const float w = 30.0f * (1.0f - lifeRatio);
      const float h = (effect.param1 > 0) ? effect.param1 : 150.0f;
      DrawRectangleGradientV(static_cast<int>(pos.x - w / 2),
                             static_cast<int>(pos.y - h), static_cast<int>(w),
                             static_cast<int>(h), Fade(WHITE, 0.0f), color);
      DrawCircleGradient(static_cast<int>(pos.x), static_cast<int>(pos.y), w,
                         color, Fade(color, 0.0f));
      break;
    }
    case VisualEffectType::GoldSparkle:
      DrawPoly({pos.x, pos.y}, 4, 15.0f * currentScale, lifeRatio * 180.0f,
               color);
      break;
    default:
      break;
    }
  }

  auto projView = frame.registry.view<const Position, NoMoreDay::Projectile>();
  for (auto entity : projView) {
    const auto &pos = projView.get<const Position>(entity);
    auto &proj = projView.get<NoMoreDay::Projectile>(entity);
    proj.hasRendered = true;

    NoMoreDay::components::GPUSkillEffect eff;
    float ax = pos.x;
    float ay = pos.y;
    if (const auto *vel = frame.registry.try_get<Velocity>(entity)) {
      ax += vel->vx * frame.context.renderAlpha * (1.0f / 60.0f);
      ay += vel->vy * frame.context.renderAlpha * (1.0f / 60.0f);
      eff.velocity = {vel->vx, vel->vy};
    }
    eff.position = {ax, ay};
    eff.radius = (proj.radius > 1.0f) ? proj.radius : 5.0f;
    eff.sectorAngle = (proj.arcWidth > 0.0f) ? proj.arcWidth : 45.0f;
    eff.type = static_cast<float>(proj.visualType);
    eff.softness = 0.5f;

    if (const auto *col = frame.registry.try_get<ColorComponent>(entity)) {
      eff.coreColor = ColorNormalize(col->color);
      eff.glowColor = ColorNormalize(col->color);
    } else {
      eff.coreColor = {1, 1, 1, 1};
      eff.glowColor = {0.8f, 0.8f, 1, 0.5f};
    }
    NoMoreDay::systems::GPUSkillEffectSystem::Get().Submit(eff);
  }
  NoMoreDay::systems::GPUSkillEffectSystem::Get().Render(frame.camera);
}

void ExecuteUIWorldPass(RenderFrameData &frame) {
  auto popupView = frame.registry.view<const Position, const DamagePopup>();
  popupView.each([&frame](const auto &pos, const auto &popup) {
    const float alpha =
        (popup.timer > popup.lifeTime * 0.5f)
            ? 1.0f -
                  ((popup.timer - popup.lifeTime * 0.5f) /
                   (popup.lifeTime * 0.5f))
            : 1.0f;
    Color color = popup.color;
    color.a = static_cast<unsigned char>(255 * alpha);
    const char *text = popup.isStatus
                           ? popup.statusText.c_str()
                           : (popup.isDodge
                                  ? "Dodge"
                                  : (popup.isMiss
                                         ? "Miss"
                                         : (popup.isBlock
                                                ? TextFormat("Block %d",
                                                             static_cast<int>(popup.damage))
                                                : TextFormat("%d",
                                                             static_cast<int>(popup.damage)))));
    const float fontSize = (popup.isCrit ? 36.0f : 28.0f) * popup.currentScale *
                           frame.fontScale;
    if (IsFontValid(frame.font)) {
      DrawTextEx(frame.font, text, {pos.x + 2, pos.y + 2}, fontSize, 1.0f,
                 Fade(BLACK, alpha * 0.8f));
      DrawTextEx(frame.font, text, {pos.x, pos.y}, fontSize, 1.0f, color);
    }
  });

  NoMoreDay::utils::ScopedTimer itemTimer("Loot Label Collection", 100);
  RenderSystem::VisibleItemCache::Clear();
  if (frame.labelBuffer != nullptr) {
    frame.labelBuffer->clear();
  }
  if (frame.glyphBuffer != nullptr) {
    frame.glyphBuffer->clear();
  }
  s_beamBuffer.clear();
  const bool enableLootBeams =
      NoMoreDay::render::core::QualityTierManager::Get()
          .GetConfig()
          .dynamicLightingEnabled;

  const Vector2 vTL = GetScreenToWorld2D({0, 0}, frame.camera);
  const Vector2 vBR =
      GetScreenToWorld2D({static_cast<float>(GetScreenWidth()),
                          static_cast<float>(GetScreenHeight())},
                         frame.camera);
  const Rectangle viewRect = {vTL.x - 100, vTL.y - 100, (vBR.x - vTL.x) + 200,
                              (vBR.y - vTL.y) + 200};
  Mesh &quadMesh = GetLabelQuadMesh();

  int labelCount = 0;
  struct LabelCandidate {
    entt::entity entity;
    Vector2 pos;
    Vector2 size;
    Color color;
    float scale;
    std::string text;
    bool isGold = false;
    Rectangle currentRect;
  };
  static std::vector<LabelCandidate> s_candidates;
  s_candidates.clear();

  if (RenderSystem::s_itemGrid) {
    RenderSystem::s_itemGrid->query(
        {frame.camera.target.x, frame.camera.target.y}, 1000.0f,
        [&](entt::entity entity, const Vector2 &pos) -> bool {
          if (labelCount >= 64) {
            return false;
          }
          if (!CheckCollisionPointRec({pos.x, pos.y}, viewRect)) {
            return true;
          }

          if (const auto *item =
                  frame.registry.try_get<NoMoreDay::ItemComponent>(entity)) {
            const auto *filterResult =
                frame.registry.try_get<NoMoreDay::LootFilterResultComponent>(
                    entity);
            if (labelCount > 32 && item->rarity < NoMoreDay::Rarity::Rare &&
                (!filterResult || filterResult->scale <= 1.0f)) {
              return true;
            }

            Color rarityColor = UISystem::GetRarityColor(item->rarity);
            float scale = 1.0f;
            bool emphasized = false;
            if (filterResult) {
              if (!filterResult->visible) {
                return true;
              }
              if (filterResult->scale > 1.0f) {
                scale = filterResult->scale;
                emphasized = true;
                rarityColor = filterResult->color;
              }
            }

            auto &labelCache =
                frame.registry
                    .get_or_emplace<LabelCacheComponent>(entity);
            ++labelCount;
            int fSize = static_cast<int>(18.0f * scale * frame.fontScale);
            if (fSize < 12) {
              fSize = 12;
            }
            if (!labelCache.isValid || labelCache.lastFontSize != fSize ||
                labelCache.lastRarityHash !=
                    static_cast<uint32_t>(item->rarity)) {
              labelCache.cachedSize =
                  IsFontValid(frame.font)
                      ? ::NoMoreDay::render::LootTextBatcher::MeasureText(
                            frame.font, item->name, static_cast<float>(fSize))
                      : Vector2{static_cast<float>(
                                    MeasureText(item->name.c_str(), fSize)),
                                static_cast<float>(fSize)};
              labelCache.lastFontSize = fSize;
              labelCache.lastRarityHash = static_cast<uint32_t>(item->rarity);
              labelCache.isValid = true;
            }

            const Vector2 tSize = labelCache.cachedSize;
            const Rectangle bg = {pos.x - tSize.x / 2 - 4,
                                  pos.y - 30.0f * scale - 2, tSize.x + 8,
                                  tSize.y + 4};
            s_candidates.push_back(
                {entity, pos, tSize, rarityColor, scale, item->name, false, bg});

            if ((item->rarity >= NoMoreDay::Rarity::Rare || emphasized) &&
                enableLootBeams) {
              GPUBeamInstance bi;
              bi.position = {pos.x, pos.y};
              bi.size = {24.0f * scale, 120.0f * scale};
              bi.color = ColorNormalize(rarityColor);
              bi.time = static_cast<float>(GetTime());
              s_beamBuffer.push_back(bi);
            }
          } else if (const auto *gold = frame.registry.try_get<GoldComponent>(
                         entity)) {
            if (labelCount > 48 && gold->amount < 100) {
              return true;
            }
            auto &labelCache =
                frame.registry
                    .get_or_emplace<LabelCacheComponent>(entity);
            ++labelCount;
            int fSize = static_cast<int>(16.0f * frame.fontScale);
            if (fSize < 10) {
              fSize = 10;
            }
            if (!labelCache.isValid || labelCache.lastFontSize != fSize) {
              if (!labelCache.isValid) {
                std::snprintf(labelCache.cachedText,
                              sizeof(labelCache.cachedText), "%d Gold",
                              gold->amount);
              }
              labelCache.cachedSize =
                  IsFontValid(frame.font)
                      ? ::NoMoreDay::render::LootTextBatcher::MeasureText(
                            frame.font, labelCache.cachedText,
                            static_cast<float>(fSize))
                      : Vector2{static_cast<float>(
                                    MeasureText(labelCache.cachedText, fSize)),
                                static_cast<float>(fSize)};
              labelCache.lastFontSize = fSize;
              labelCache.isValid = true;
            }

            const Vector2 tSize = labelCache.cachedSize;
            const Rectangle bg = {pos.x - tSize.x / 2 - 4, pos.y - 25.0f - 2,
                                  tSize.x + 8, tSize.y + 4};
            s_candidates.push_back({entity, pos, tSize, GOLD, 1.0f,
                                    labelCache.cachedText, true, bg});
          }
          return true;
        });
  }

  if (!s_candidates.empty()) {
    std::sort(s_candidates.begin(), s_candidates.end(),
              [](const LabelCandidate &a, const LabelCandidate &b) {
                return a.pos.y > b.pos.y;
              });

    for (size_t i = 0; i < s_candidates.size(); ++i) {
      auto &cand = s_candidates[i];
      bool overlap = true;
      int safety = 0;
      while (overlap && safety < 8) {
        overlap = false;
        for (size_t j = 0; j < i; ++j) {
          if (CheckCollisionRecs(cand.currentRect, s_candidates[j].currentRect)) {
            cand.currentRect.y =
                s_candidates[j].currentRect.y - cand.currentRect.height - 2;
            overlap = true;
            break;
          }
        }
        ++safety;
      }

      const bool hovered = (cand.entity == UISystem::State.hoveredItem);
      NoMoreDay::components::GPULabelInstance inst;
      inst.position = {cand.currentRect.x, cand.currentRect.y};
      inst.size = {cand.currentRect.width, cand.currentRect.height};
      inst.bgColor = ColorNormalize(Fade(BLACK, 0.7f));
      inst.borderColor =
          ColorNormalize(hovered ? WHITE : ColorAlpha(cand.color, 0.5f));
      inst.borderWidth = hovered ? 2.0f : 1.0f;
      inst.cornerRadius = 4.0f;
      if (frame.labelBuffer != nullptr) {
        frame.labelBuffer->push_back(inst);
      }
      RenderSystem::VisibleItemCache::visibleItems.push_back(
          {cand.entity, cand.currentRect});

      if (IsFontValid(frame.font) && frame.glyphBuffer != nullptr) {
        int fSize = cand.isGold ? static_cast<int>(16.0f * frame.fontScale)
                                : static_cast<int>(18.0f * cand.scale *
                                                   frame.fontScale);
        if (fSize < 10) {
          fSize = 10;
        }
        ::NoMoreDay::render::LootTextBatcher::BatchString(
            frame.font, cand.text, {cand.currentRect.x + 4, cand.currentRect.y + 2},
            static_cast<float>(fSize), cand.color, *frame.glyphBuffer);
      }
    }
  }

  rlDrawRenderBatchActive();
  rlDisableDepthMask();
  rlDisableDepthTest();
  rlDisableBackfaceCulling();
  rlSetBlendMode(RL_BLEND_ALPHA);

  if (!s_beamBuffer.empty() && s_beamShader.id != 0 && s_beamInstanceBuffer) {
    const size_t sz = s_beamBuffer.size() * sizeof(GPUBeamInstance);
    if (sz > s_beamInstanceBuffer->GetSize()) {
      s_beamInstanceBuffer->Create(sz * 2, s_beamBuffer.data(), RL_DYNAMIC_DRAW);
    } else {
      s_beamInstanceBuffer->OrphanAndUpload(s_beamBuffer.data(), sz,
                                            RL_DYNAMIC_DRAW);
    }
    s_beamInstanceBuffer->BindBase(static_cast<uint32_t>(
        NoMoreDay::RenderConstants::Binding::SSBO_BEAM_INSTANCE));
    BeginShaderMode(s_beamShader);
    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    SetShaderValueMatrix(s_beamShader, s_beamMvpLoc, mvp);
    rlEnableVertexArray(quadMesh.vaoId);
    rlDrawVertexArrayInstanced(0, 6, static_cast<int>(s_beamBuffer.size()));
    rlDisableVertexArray();
    EndShaderMode();
  }

  if (frame.labelBuffer != nullptr && !frame.labelBuffer->empty() &&
      frame.labelShader != nullptr && frame.labelShader->id != 0 &&
      frame.labelInstanceBuffer != nullptr) {
    const size_t sz = frame.labelBuffer->size() *
                      sizeof(NoMoreDay::components::GPULabelInstance);
    if (sz > frame.labelInstanceBuffer->GetSize()) {
      frame.labelInstanceBuffer->Create(sz * 2, frame.labelBuffer->data(),
                                        RL_DYNAMIC_DRAW);
    } else {
      frame.labelInstanceBuffer->OrphanAndUpload(frame.labelBuffer->data(), sz,
                                                 RL_DYNAMIC_DRAW);
    }
    frame.labelInstanceBuffer->BindBase(static_cast<uint32_t>(
        NoMoreDay::RenderConstants::Binding::SSBO_LABEL_INSTANCE));
    BeginShaderMode(*frame.labelShader);
    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    SetShaderValueMatrix(*frame.labelShader, frame.labelMvpLoc, mvp);
    rlEnableVertexArray(quadMesh.vaoId);
    rlDrawVertexArrayInstanced(0, 6,
                               static_cast<int>(frame.labelBuffer->size()));
    rlDisableVertexArray();
    EndShaderMode();
  }

  if (frame.glyphBuffer != nullptr && !frame.glyphBuffer->empty() &&
      frame.glyphShader != nullptr && frame.glyphShader->id != 0 &&
      frame.glyphInstanceBuffer != nullptr) {
    const size_t sz = frame.glyphBuffer->size() *
                      sizeof(NoMoreDay::components::GPUGlyphInstance);
    if (sz > frame.glyphInstanceBuffer->GetSize()) {
      frame.glyphInstanceBuffer->Create(sz * 2, frame.glyphBuffer->data(),
                                        RL_DYNAMIC_DRAW);
    } else {
      frame.glyphInstanceBuffer->OrphanAndUpload(frame.glyphBuffer->data(), sz,
                                                 RL_DYNAMIC_DRAW);
    }
    frame.glyphInstanceBuffer->BindBase(static_cast<uint32_t>(
        NoMoreDay::RenderConstants::Binding::SSBO_GLYPH_INSTANCE));

    BeginShaderMode(*frame.glyphShader);
    const Matrix mvp = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    SetShaderValueMatrix(*frame.glyphShader, frame.glyphMvpLoc, mvp);

    rlActiveTextureSlot(3);
    rlEnableTexture(frame.font.texture.id);
    if (frame.glyphTexLoc != -1) {
      int texUnit = 3;
      rlSetUniform(frame.glyphTexLoc, &texUnit, RL_SHADER_UNIFORM_INT, 1);
    }

    rlEnableVertexArray(quadMesh.vaoId);
    rlDrawVertexArrayInstanced(0, 6,
                               static_cast<int>(frame.glyphBuffer->size()));
    rlDisableVertexArray();
    EndShaderMode();
    rlActiveTextureSlot(0);
  }

  rlDrawRenderBatchActive();
  rlSetBlendMode(RL_BLEND_ALPHA);
}

void ExecuteCompositePass() {
  rlDrawRenderBatchActive();
  rlSetBlendMode(RL_BLEND_ALPHA);
}

void ExecuteCompositePass(
    const NoMoreDay::render::resources::FramebufferHandle *hdrBuffer,
    const CompositeTargetState &targetState) {
  if (hdrBuffer == nullptr || !hdrBuffer->IsValid()) {
    ExecuteCompositePass();
    return;
  }

  constexpr uint32_t kGLFramebuffer = 0x8D40;
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer,
                                              targetState.framebuffer);
  NoMoreDay::utils::GPUUtils::Viewport(
      targetState.viewportX, targetState.viewportY, targetState.viewportWidth,
      targetState.viewportHeight);

  Texture2D hdrTexture = {};
  hdrTexture.id = hdrBuffer->colorTexture;
  hdrTexture.width = hdrBuffer->width;
  hdrTexture.height = hdrBuffer->height;
  hdrTexture.mipmaps = 1;
  hdrTexture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;

  const Rectangle source = {0.0f, 0.0f, static_cast<float>(hdrTexture.width),
                            -static_cast<float>(hdrTexture.height)};
  const Rectangle target = {
      static_cast<float>(targetState.viewportX),
      static_cast<float>(targetState.viewportY),
      static_cast<float>(targetState.viewportWidth),
      static_cast<float>(targetState.viewportHeight)};
  DrawTexturePro(hdrTexture, source, target, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
}

} // namespace

void RenderSystem::AddDistortionSource(float worldX, float worldY, float radius,
                                       float strength) {
  if (g_distortionPass == nullptr) {
    return;
  }

  const auto &config =
      NoMoreDay::render::core::QualityTierManager::Get().GetConfig();
  if (!config.distortionEnabled) {
    return;
  }

  g_distortionPass->AddDistortionSource(worldX, worldY, radius, strength);
}

void RenderSystem::Initialize() {
#if defined(NDEBUG)
  constexpr bool kHardFailGpuAbiMismatch = false;
#else
  constexpr bool kHardFailGpuAbiMismatch = true;
#endif
  const bool abiCompatible = NoMoreDay::render::abi::ValidateGeneratedShaderABI(
      "assets/shaders/generated/gpu_abi.glslinc", kHardFailGpuAbiMismatch);
  if (!abiCompatible) {
    LOG_ERROR("RenderSystem: startup aborted due incompatible GPU ABI contract.");
    return;
  }

  NoMoreDay::render::core::QualityTierManager::Get().Initialize("settings.json");
  NoMoreDay::render::MaterialManager::Get().Initialize();
  NoMoreDay::render::MaterialManager::Get().LoadFromJson(
      "assets/data/materials_vfx.json");
  NoMoreDay::render::lighting::LightManager::Get().Initialize();
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    if (screenWidth > 0 && screenHeight > 0) {
      s_hdrSceneBuffer = NoMoreDay::render::resources::FramebufferManager::Create(
          screenWidth, screenHeight, 0x881A, false); // GL_RGBA16F
    }
  }
  g_postProcessPass = std::make_shared<NoMoreDay::render::passes::PostProcessPass>();
  g_postProcessPass->Initialize();
  g_lightingPass = std::make_shared<NoMoreDay::render::passes::LightingPass>();
  g_lightingPass->Initialize();
  g_volumetricPass =
      std::make_shared<NoMoreDay::render::passes::VolumetricLightPass>();
  g_distortionPass = std::make_shared<NoMoreDay::render::passes::DistortionPass>();
  g_distortionPass->Initialize();
  g_renderProfiler = std::make_unique<NoMoreDay::render::debug::RenderProfiler>();
  g_shaderHotReloadManager =
      std::make_unique<NoMoreDay::render::dev::ShaderHotReloadManager>();
  g_shaderHotReloadManager->SetPollIntervalSeconds(0.5);
  g_shaderHotReloadManager->Register(
      {.debugName = "PostProcess.BrightExtract",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/postprocess/bright_extract.frag"},
      []() { return g_postProcessPass && g_postProcessPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "PostProcess.KawaseDown",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/postprocess/kawase_down.frag"},
      []() { return g_postProcessPass && g_postProcessPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "PostProcess.KawaseUp",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/postprocess/kawase_up.frag"},
      []() { return g_postProcessPass && g_postProcessPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "PostProcess.Tonemap",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/postprocess/tonemap.frag"},
      []() { return g_postProcessPass && g_postProcessPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "PostProcess.FXAA",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/postprocess/fxaa.frag"},
      []() { return g_postProcessPass && g_postProcessPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "PostProcess.Vignette",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/postprocess/vignette.frag"},
      []() { return g_postProcessPass && g_postProcessPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "PostProcess.ColorGrading",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/postprocess/color_grading.frag"},
      []() { return g_postProcessPass && g_postProcessPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "Lighting.Accumulation",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/lighting/light_accumulation.frag"},
      []() { return g_lightingPass && g_lightingPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "Lighting.Volumetric",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/lighting/volumetric_light.frag"},
      []() { return g_volumetricPass && g_volumetricPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "Distortion.Apply",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/postprocess/distortion_apply.frag"},
      []() { return g_distortionPass && g_distortionPass->ReloadShaders(); });
  g_shaderHotReloadManager->Register(
      {.debugName = "Distortion.Write",
       .vertexPath = "assets/shaders/postprocess/fullscreen.vert",
       .fragmentPath = "assets/shaders/postprocess/distortion_write.frag"},
      []() { return g_distortionPass && g_distortionPass->ReloadShaders(); });

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

  s_glyphShader = LoadShader("assets/shaders/ui/glyph.vert",
                             "assets/shaders/ui/glyph.frag");
  if (s_glyphShader.id != 0) {
    s_glyphMvpLoc = GetShaderLocation(s_glyphShader, "mvp");
    s_glyphTexLoc = GetShaderLocation(s_glyphShader, "uFontAtlas");
  }

  s_glyphInstanceBuffer = std::make_unique<NoMoreDay::core::ComputeBuffer>();
  s_glyphInstanceBuffer->Create(
      NoMoreDay::RenderConstants::GPU::MAX_GLYPHS *
          sizeof(NoMoreDay::components::GPUGlyphInstance),
      nullptr, RL_DYNAMIC_DRAW);
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

  if (s_glyphShader.id != 0) {
    UnloadShader(s_glyphShader);
    s_glyphShader.id = 0;
  }
  s_glyphInstanceBuffer = nullptr;

  NoMoreDay::render::resources::FramebufferManager::Destroy(s_hdrSceneBuffer);
  NoMoreDay::render::MaterialManager::Get().Shutdown();
  NoMoreDay::render::lighting::LightManager::Get().Shutdown();
  if (g_lightingPass) {
    g_lightingPass->Shutdown();
    g_lightingPass.reset();
  }
  if (g_volumetricPass) {
    g_volumetricPass->Shutdown();
    g_volumetricPass.reset();
  }
  if (g_postProcessPass) {
    g_postProcessPass->Shutdown();
    g_postProcessPass.reset();
  }
  if (g_distortionPass) {
    g_distortionPass->Shutdown();
    g_distortionPass.reset();
  }
  if (g_shaderHotReloadManager) {
    g_shaderHotReloadManager->Clear();
    g_shaderHotReloadManager.reset();
  }
  g_renderProfiler.reset();
  NoMoreDay::render::GPUTrailRenderer::Get().Shutdown();
  NoMoreDay::render::resources::FullscreenQuad::Shutdown();
  s_itemGrid = nullptr;
  g_transientPool.Shutdown();
}

void RenderSystem::render(entt::registry &registry,
                          const NoMoreDay::SharedContext &context,
                          const Camera2D &camera) {
  RenderFrameData frame{registry, context, camera};
  frame.labelBuffer = &s_labelBuffer;
  frame.glyphBuffer = &s_glyphBuffer;
  frame.labelShader = &s_labelShader;
  frame.glyphShader = &s_glyphShader;
  frame.labelMvpLoc = s_labelMvpLoc;
  frame.glyphMvpLoc = s_glyphMvpLoc;
  frame.glyphTexLoc = s_glyphTexLoc;
  frame.labelInstanceBuffer = s_labelInstanceBuffer.get();
  frame.glyphInstanceBuffer = s_glyphInstanceBuffer.get();
  BuildRenderFrameData(frame);
  const CompositeTargetState compositeTarget = CaptureCompositeTargetState();

  g_transientPool.BeginFrame();
  const auto &renderConfig =
      NoMoreDay::render::core::QualityTierManager::Get().GetConfig();
#if defined(NDEBUG)
  constexpr bool kDevHotReloadAllowed = false;
#else
  constexpr bool kDevHotReloadAllowed = true;
#endif
  const bool shaderHotReloadEnabled =
      kDevHotReloadAllowed && renderConfig.shaderHotReloadEnabled;
  if (g_shaderHotReloadManager) {
    g_shaderHotReloadManager->SetEnabled(shaderHotReloadEnabled);
    g_shaderHotReloadManager->PollAndReload();
  }
  if (renderConfig.materialSystemEnabled) {
    NoMoreDay::render::MaterialManager::Get().TryHotReload();
    NoMoreDay::render::MaterialManager::Get().SyncToGPU();
    NoMoreDay::render::MaterialManager::Get().BindSSBO(
        NoMoreDay::RenderConstants::Binding::SSBO_MATERIAL_DATA);
  }
  // Safety gate for BUG-20260212-001:
  // internal HDR/post-process/distortion chain is only valid for the default
  // framebuffer path. Offscreen RT path must use its dedicated pipeline.
  const bool hdrPipelineRequested = IsHdrScenePipelineRequested(renderConfig);
  const bool useHdrSceneBuffer =
      hdrPipelineRequested && (compositeTarget.framebuffer == 0);
  static bool s_prevUseHdrSceneBuffer = false;
  static bool s_prevHdrPipelineRequested = false;
  static uint32_t s_prevCompositeFramebuffer = 0;
  if (s_prevUseHdrSceneBuffer != useHdrSceneBuffer ||
      s_prevHdrPipelineRequested != hdrPipelineRequested ||
      s_prevCompositeFramebuffer != compositeTarget.framebuffer) {
    LOG_INFO("RenderSystem: HDR chain {} (requested={}, bloom={}, postFx={}, "
             "dynamicLighting={}, volumetric={}, compositeFbo={})",
             useHdrSceneBuffer ? "enabled" : "disabled",
             hdrPipelineRequested ? 1 : 0,
             renderConfig.bloomEnabled ? 1 : 0,
             IsHdrPostProcessRequested(renderConfig) ? 1 : 0,
             renderConfig.dynamicLightingEnabled ? 1 : 0,
             renderConfig.volumetricLightEnabled ? 1 : 0,
             compositeTarget.framebuffer);
    s_prevUseHdrSceneBuffer = useHdrSceneBuffer;
    s_prevHdrPipelineRequested = hdrPipelineRequested;
    s_prevCompositeFramebuffer = compositeTarget.framebuffer;
  }
  const bool useDistortionPass =
      useHdrSceneBuffer && renderConfig.distortionEnabled &&
      (g_postProcessPass != nullptr) && (g_distortionPass != nullptr);
  const bool useVolumetricPass =
      useHdrSceneBuffer && renderConfig.volumetricLightEnabled &&
      (g_volumetricPass != nullptr);
  if (!useDistortionPass && g_distortionPass != nullptr) {
    g_distortionPass->ResetSources();
  }
  static bool s_prevUseVolumetricPass = false;
  if (s_prevUseVolumetricPass && !useVolumetricPass && g_volumetricPass != nullptr) {
    g_volumetricPass->Shutdown();
  }
  s_prevUseVolumetricPass = useVolumetricPass;

  if (useHdrSceneBuffer && NoMoreDay::utils::GPUUtils::IsInitialized()) {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    if (!s_hdrSceneBuffer.IsValid()) {
      s_hdrSceneBuffer = NoMoreDay::render::resources::FramebufferManager::Create(
          screenWidth, screenHeight, 0x881A, false); // GL_RGBA16F
      if (s_hdrSceneBuffer.IsValid()) {
        const double approxMb =
            (static_cast<double>(screenWidth) * static_cast<double>(screenHeight) *
             8.0) /
            (1024.0 * 1024.0);
        LOG_INFO("RenderSystem: created HDR scene buffer {}x{} (~{:.2f} MB)",
                 screenWidth, screenHeight, approxMb);
      }
      if (g_lightingPass && s_hdrSceneBuffer.IsValid()) {
        g_lightingPass->OnResize(screenWidth, screenHeight);
      }
      if (g_volumetricPass && s_hdrSceneBuffer.IsValid() && useVolumetricPass) {
        g_volumetricPass->OnResize(screenWidth, screenHeight);
      }
    } else if (s_hdrSceneBuffer.width != screenWidth ||
               s_hdrSceneBuffer.height != screenHeight) {
      NoMoreDay::render::resources::FramebufferManager::Resize(
          s_hdrSceneBuffer, screenWidth, screenHeight);
      const double approxMb =
          (static_cast<double>(screenWidth) * static_cast<double>(screenHeight) *
           8.0) /
          (1024.0 * 1024.0);
      LOG_INFO("RenderSystem: resized HDR scene buffer {}x{} (~{:.2f} MB)",
               screenWidth, screenHeight, approxMb);
      if (g_lightingPass && s_hdrSceneBuffer.IsValid()) {
        g_lightingPass->OnResize(screenWidth, screenHeight);
      }
      if (g_volumetricPass && s_hdrSceneBuffer.IsValid() && useVolumetricPass) {
        g_volumetricPass->OnResize(screenWidth, screenHeight);
      }
    }
  }

  if (useHdrSceneBuffer && renderConfig.dynamicLightingEnabled && g_lightingPass) {
    NoMoreDay::render::lighting::LightManager::Get().Update(
        registry, camera, renderConfig.maxLights, static_cast<float>(GetTime()));

    static double s_lastLightingDiagLogTime = 0.0;
    const double now = GetTime();
    if ((now - s_lastLightingDiagLogTime) >= 10.0) {
      const auto &stats = NoMoreDay::render::lighting::LightManager::Get().GetDebugStats();
      const double hdrApproxMb =
          (s_hdrSceneBuffer.IsValid()
               ? (static_cast<double>(s_hdrSceneBuffer.width) *
                  static_cast<double>(s_hdrSceneBuffer.height) * 8.0)
               : 0.0) /
          (1024.0 * 1024.0); // RGBA16F ~= 8 bytes/pixel
      LOG_INFO(
          "RenderSystem: LightingDiag tier={} allowed={} ecs={} transient={} "
          "candidates={} selected={} dropped={} hdr={}x{} (~{:.2f} MB)",
          static_cast<int>(NoMoreDay::render::core::QualityTierManager::Get().GetTier()),
          stats.allowedLights, stats.ecsLights, stats.transientLights,
          stats.candidatesAfterCull, stats.selectedLights, stats.droppedByBudget,
          s_hdrSceneBuffer.width, s_hdrSceneBuffer.height, hdrApproxMb);
      s_lastLightingDiagLogTime = now;
    }
  }

  using NoMoreDay::render::graph::RenderOwnerTag;
  using NoMoreDay::render::graph::RenderResourceTag;

  RenderOwnerTag sceneHdrOwner = RenderOwnerTag::Unknown;
  RenderOwnerTag ldrOwner = RenderOwnerTag::Unknown;

  NoMoreDay::render::graph::RenderGraph graph;
  graph.AddPass(std::make_shared<NoMoreDay::render::passes::ScenePass>(
      [&frame, useHdrSceneBuffer](NoMoreDay::render::graph::RenderContext &) {
        NoMoreDay::render::core::ScopedGLState scopedState;
        if (useHdrSceneBuffer && s_hdrSceneBuffer.IsValid()) {
          constexpr uint32_t kGLFramebuffer = 0x8D40;
          NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer,
                                                      s_hdrSceneBuffer.fbo);
          NoMoreDay::utils::GPUUtils::Viewport(0, 0, s_hdrSceneBuffer.width,
                                               s_hdrSceneBuffer.height);
          ClearBackground(BLANK);
        }
        ExecuteScenePass(frame);
      }));
  sceneHdrOwner = RenderOwnerTag::Scene;

  if (useHdrSceneBuffer && renderConfig.dynamicLightingEnabled &&
      g_lightingPass != nullptr) {
    graph.AddPass(g_lightingPass);
    sceneHdrOwner = RenderOwnerTag::Lighting;
  }
  if (useVolumetricPass && g_volumetricPass != nullptr) {
    graph.AddPass(g_volumetricPass);
    sceneHdrOwner = RenderOwnerTag::Volumetric;
  }
  graph.AddPass(std::make_shared<NoMoreDay::render::passes::VFXPass>(
      [&frame](NoMoreDay::render::graph::RenderContext &) {
        NoMoreDay::render::core::ScopedGLState scopedState;
        ExecuteVFXPass(frame);
      }));
  sceneHdrOwner = RenderOwnerTag::VFX;

  graph.AddPass(std::make_shared<NoMoreDay::render::passes::UIWorldPass>(
      [&frame](NoMoreDay::render::graph::RenderContext &) {
        NoMoreDay::render::core::ScopedGLState scopedState;
        ExecuteUIWorldPass(frame);
      }));
  sceneHdrOwner = RenderOwnerTag::UIWorld;

  if (useHdrSceneBuffer && g_postProcessPass != nullptr) {
    graph.AddPass(g_postProcessPass);
    ldrOwner = RenderOwnerTag::PostProcess;
  }
  if (useDistortionPass && g_distortionPass != nullptr &&
      g_postProcessPass != nullptr) {
    g_distortionPass->SetInputBuffer(&g_postProcessPass->GetOutputBuffer());
    graph.AddPass(g_distortionPass);
    ldrOwner = RenderOwnerTag::Distortion;
  }

  RenderResourceTag compositeInputResource = RenderResourceTag::SceneHdrColor;
  RenderOwnerTag compositeInputOwner = sceneHdrOwner;
  if (ldrOwner == RenderOwnerTag::PostProcess) {
    compositeInputResource = RenderResourceTag::PostProcessLdrColor;
    compositeInputOwner = RenderOwnerTag::PostProcess;
  } else if (ldrOwner == RenderOwnerTag::Distortion) {
    compositeInputResource = RenderResourceTag::DistortionLdrColor;
    compositeInputOwner = RenderOwnerTag::Distortion;
  }

  static RenderOwnerTag s_prevSceneHdrOwner = RenderOwnerTag::Unknown;
  static RenderOwnerTag s_prevLdrOwner = RenderOwnerTag::Unknown;
  static RenderResourceTag s_prevCompositeInput = RenderResourceTag::Custom;
  static RenderOwnerTag s_prevCompositeInputOwner = RenderOwnerTag::Unknown;
  if (s_prevSceneHdrOwner != sceneHdrOwner || s_prevLdrOwner != ldrOwner ||
      s_prevCompositeInput != compositeInputResource ||
      s_prevCompositeInputOwner != compositeInputOwner) {
    LOG_INFO(
        "RenderSystem: ownership transition sceneHdr={} ldr={} compositeIn={} "
        "compositeOwner={}",
        NoMoreDay::render::graph::ToOwnerName(sceneHdrOwner),
        NoMoreDay::render::graph::ToOwnerName(ldrOwner),
        NoMoreDay::render::graph::ToResourceName(compositeInputResource),
        NoMoreDay::render::graph::ToOwnerName(compositeInputOwner));
    s_prevSceneHdrOwner = sceneHdrOwner;
    s_prevLdrOwner = ldrOwner;
    s_prevCompositeInput = compositeInputResource;
    s_prevCompositeInputOwner = compositeInputOwner;
  }

  graph.AddPass(std::make_shared<NoMoreDay::render::passes::CompositePass>(
      compositeInputResource, compositeInputOwner,
      [useHdrSceneBuffer, useDistortionPass, compositeTarget](
          NoMoreDay::render::graph::RenderContext &) {
        NoMoreDay::render::core::ScopedGLState scopedState;
        if (useDistortionPass && g_distortionPass != nullptr &&
            g_distortionPass->GetOutputBuffer().IsValid()) {
          ExecuteCompositePass(&g_distortionPass->GetOutputBuffer(),
                               compositeTarget);
        } else if (useHdrSceneBuffer && g_postProcessPass != nullptr &&
                   g_postProcessPass->GetOutputBuffer().IsValid()) {
          ExecuteCompositePass(&g_postProcessPass->GetOutputBuffer(),
                               compositeTarget);
        } else if (useHdrSceneBuffer && s_hdrSceneBuffer.IsValid()) {
          ExecuteCompositePass(&s_hdrSceneBuffer, compositeTarget);
        } else {
          ExecuteCompositePass();
        }
      }));

  NoMoreDay::render::graph::RenderContext graphContext = {};
  graphContext.registry = &registry;
  graphContext.shared = &context;
  graphContext.camera = &camera;
  graphContext.transientPool = &g_transientPool;
  graphContext.qualityManager = &NoMoreDay::render::core::QualityTierManager::Get();
  graphContext.renderProfiler =
      (renderConfig.profilerHudEnabled && g_renderProfiler != nullptr)
          ? g_renderProfiler.get()
          : nullptr;
  graphContext.hdrSceneBuffer =
      useHdrSceneBuffer ? s_hdrSceneBuffer
                        : NoMoreDay::render::resources::FramebufferHandle{};

  if (graphContext.renderProfiler != nullptr) {
    graphContext.renderProfiler->BeginFrame();
  }
  graph.Build();
  graph.Execute(graphContext);
  if (graphContext.renderProfiler != nullptr) {
    graphContext.renderProfiler->EndFrame();

    static double s_lastProfilerLog = 0.0;
    const double now = GetTime();
    if ((now - s_lastProfilerLog) >= 5.0) {
      const auto passStats = graphContext.renderProfiler->GetAllStats();
      for (size_t i = 0; i < passStats.size(); ++i) {
        const auto passId = static_cast<NoMoreDay::render::debug::RenderPassId>(i);
        const auto &stats = passStats[i];
        const float overPct = (stats.budgetMs > 0.0f)
                                  ? std::max(0.0f, (stats.gpuMeanMs - stats.budgetMs) /
                                                       stats.budgetMs * 100.0f)
                                  : 0.0f;
        LOG_INFO("RenderProfiler[{}]: CPU(mean={:.3f},p95={:.3f}) GPU(mean={:.3f},p95={:.3f}) budget={:.3f} over={:.1f}%",
                 NoMoreDay::render::debug::RenderProfiler::ToString(passId),
                 stats.cpuMeanMs, stats.cpuP95Ms, stats.gpuMeanMs, stats.gpuP95Ms,
                 stats.budgetMs, overPct);
      }
      s_lastProfilerLog = now;
    }

    NoMoreDay::render::debug::DrawProfilerHud(*graphContext.renderProfiler, 14.0f,
                                               14.0f);
  }

  g_transientPool.EndFrame();
}
