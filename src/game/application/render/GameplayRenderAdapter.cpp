#include "game/application/render/GameplayRenderAdapter.hpp"
#include "game/application/render/EmissiveStampAdapter.hpp"
#include "game/application/render/GPULootAdapter.hpp"
#include "game/application/render/HeightFieldAdapter.hpp"
#include "game/application/render/LightAdapter.hpp"
#include "game/application/render/LootLabelBudget.hpp"
#include "game/application/render/OccluderProjector.hpp"
#include "game/foundation/data/BiomeTypes.hpp"

#include "core/logging/Logger.hpp"
#include "core/utils/FmtBuffer.hpp"
#include "core/utils/ScopedTimer.hpp"
#include "engine/render/SIMDSpatialGrid.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/LootTextBatcher.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/resource/MSDFAtlasRegistry.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/systems/world/WorldConstants.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/components/vfx/HoloBladeComponent.hpp"
#include "game/foundation/data/BiomeRegistry.hpp"
#include "game/systems/combat/MonsterAffixSystem.hpp"
#include "game/systems/item/LootFilter.hpp"
#include "game/systems/item/LootGridSystem.hpp"
#include "game/foundation/ui_shared/UiShared.hpp"
#include "game/systems/vfx/HoloBladeRenderSystem.hpp"
#include "game/systems/vfx/SwordIntentVisualSystem.hpp"
#include "game/systems/vfx/TrailSystem.hpp"
#include "game/systems/world/FogOfWarSystem.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "game/systems/world/PortalSystem.hpp"

#include <algorithm>
#include <cmath>

// R10 (B-R0-1): Tracy instrumentation, active only when TRACY_PROFILING=ON
// (no-op macros otherwise).
#include <tracy/Tracy.hpp>

namespace NoMoreDay {

namespace {

// R3 (remediation, design §3.5): shared scratch between the read-only
// visible-proxy producer (CollectVisibleItemProxies) and the CPU label builder
// (BuildCpuLootLabels). File-scope reuse buffer keeps the steady-state path
// allocation-free (plan §3 zero-alloc steady state).
struct LabelCandidate {
  entt::entity entity;
  Vector2 pos;
  Vector2 size;
  Color color;
  float scale;
  std::string text;
  bool isGold = false;
  Rectangle currentRect;
  NoMoreDay::Rarity rarity = NoMoreDay::Rarity::Common;
  float distSq = 0.0f;
  int amount = 0;
  bool emphasized = false;
};
std::vector<LabelCandidate> s_candidates;

} // namespace

// Static definitions for the shared visibility cache + loot grid now live in
// NoMoreDayGameUiShared (UiShared.cpp) — design §5.3 ring 2 break.
// U8 收尾：战利品空间网格迁入 item 域 LootGridSystem（生命周期委托本类 Init/Shutdown）。

void GameplayRenderAdapter::Init() { systems::LootGridSystem::Init(); }

void GameplayRenderAdapter::Shutdown() { systems::LootGridSystem::Shutdown(); }

void GameplayRenderAdapter::BuildFrameData(render::GameplayRenderFrame &frame) {
  m_cameraZoom =
      (m_context->settings != nullptr) ? m_context->settings->cameraZoom : 1.5f;
  m_fontScale = (m_cameraZoom > 1e-4f) ? (1.0f / m_cameraZoom) : 1.0f;
  // U8 收尾: m_font 由组合根经 SetFont 注入（原 UiShared::GlobalFont() 读取删除）。

  auto playerView = frame.registry.view<PlayerTag, Position>();
  if (playerView.begin() != playerView.end()) {
    const auto &playerPosComp = playerView.get<Position>(playerView.front());
    m_playerPos = {playerPosComp.x, playerPosComp.y};
    m_hasPlayer = true;
  }

  if (m_context->levelManager != nullptr) {
    const auto &biome = NoMoreDay::BiomeRegistry::Get().GetBiome(
        m_context->levelManager->getCurrentBiomeID());
    m_limitEnemyVision =
        biome.hasFeature(NoMoreDay::BiomeFeature::LimitedVision) &&
        biome.visionRadius > 0.0f;
    m_fogSystem = &m_context->levelManager->getFogSystem();
  }
}

void GameplayRenderAdapter::onFrameData(render::GameplayRenderFrame &frame) {
  BuildFrameData(frame);
  frame.font = m_font;
}

void GameplayRenderAdapter::onScene(render::GameplayRenderFrame &frame) {
  ExecuteScenePass(frame);
}

void GameplayRenderAdapter::onVFX(render::GameplayRenderFrame &frame) {
  ExecuteVFXPass(frame);
}

void GameplayRenderAdapter::onUIWorld(render::GameplayRenderFrame &frame) {
  ExecuteUIWorldPass(frame);
  frame.font = m_font;
}

void GameplayRenderAdapter::onOccluders(render::GameplayRenderFrame &frame) {
  if (frame.occluderBuffer == nullptr) {
    return;
  }
  NoMoreDay::OccluderProjection projection =
      NoMoreDay::OccluderProjector::Project(frame.registry);
  *frame.occluderBuffer = std::move(projection.casters);
  frame.occluderStaticCount = projection.staticCount;
  frame.occluderDynamicCount = projection.dynamicCount;
  frame.occluderStaticSignature = projection.staticSignature;
  frame.occluderDynamicSignature = projection.dynamicSignature;
}

void GameplayRenderAdapter::onLights(render::GameplayRenderFrame &frame) {
  if (frame.lightBuffer == nullptr) {
    return;
  }
  NoMoreDay::LightProjection projection =
      NoMoreDay::LightAdapter::BuildLightCandidates(
          frame.registry, static_cast<float>(GetTime()));
  *frame.lightBuffer = std::move(projection.lights);
  frame.ecsLights = projection.ecsLights;
}

void GameplayRenderAdapter::onHeightField(render::GameplayRenderFrame &frame) {
  if (frame.heightFieldBuffer == nullptr) {
    return;
  }
  NoMoreDay::HeightFieldProjection projection =
      NoMoreDay::HeightFieldAdapter::BuildStamps(frame.registry);
  *frame.heightFieldBuffer = std::move(projection.stamps);
  frame.worldWidth = projection.worldWidth;
  frame.worldHeight = projection.worldHeight;
  frame.tileWorldSize = projection.tileWorldSize;
}

void GameplayRenderAdapter::onLoot(render::GameplayRenderFrame &frame) {
  if (frame.lootBuffer == nullptr) {
    return;
  }
  NoMoreDay::LootProjection projection =
      NoMoreDay::GPULootAdapter::BuildLoot(frame.registry);
  *frame.lootBuffer = std::move(projection.instances);
}

void GameplayRenderAdapter::onEmissive(render::GameplayRenderFrame &frame) {
  if (frame.emissiveStampBuffer == nullptr) {
    return;
  }
  NoMoreDay::EmissiveProjection projection =
      NoMoreDay::EmissiveStampAdapter::BuildEmissiveStamps(frame.registry);
  *frame.emissiveStampBuffer = std::move(projection.stamps);
}

void GameplayRenderAdapter::ExecuteScenePass(render::GameplayRenderFrame &frame) {
  // 1. Render Map/Level Background
  if (m_context != nullptr && m_context->levelManager != nullptr) {
    m_context->levelManager->render(frame.camera);
  }

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
    if (m_hasPlayer) {
      const float dx = pos.x - m_playerPos.x;
      const float dy = pos.y - m_playerPos.y;
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
    if (m_limitEnemyVision && m_fogSystem &&
        frame.registry.any_of<EnemyTag>(entity)) {
      using namespace NoMoreDay::Constants::World;
      const int gx = static_cast<int>(renderX / GRID_TILE_SIZE);
      const int gy = static_cast<int>(renderY / GRID_TILE_SIZE);
      if (!m_fogSystem->isVisible(gx, gy)) {
        continue;
      }
    }

    if (!isPlayer && frame.registry.any_of<GPUIndex>(entity)) {
      if (const auto *prevPos = frame.registry.try_get<PrevPosition>(entity)) {
        renderX = Lerp(prevPos->x, pos.x, m_context->renderAlpha);
        renderY = Lerp(prevPos->y, pos.y, m_context->renderAlpha);
      }
    }

    const Rectangle dest = {renderX, renderY, width, height};
    if (frame.registry.any_of<NoMoreDay::ShadowVisualComponent>(entity)) {
      DrawEllipse(static_cast<int>(renderX),
                  static_cast<int>(renderY + height * 0.4f), width * 0.32f,
                  height * 0.12f, Fade(BLACK, 0.3f));
    }
    if (sprite.texture.id > 0) {
      DrawTexturePro(sprite.texture, source, dest, origin, 0.0f, WHITE);
    }
  }

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
    if (frame.registry.any_of<NoMoreDay::BloodSeaFieldComponent>(entity)) {
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

  auto bloodSeaView =
      frame.registry
          .view<const Position, const NoMoreDay::BloodSeaFieldComponent>();
  for (auto entity : bloodSeaView) {
    const auto &pos = bloodSeaView.get<Position>(entity);
    const auto &field =
        bloodSeaView.get<NoMoreDay::BloodSeaFieldComponent>(entity);
    const float time = static_cast<float>(GetTime());
    const float pulseInterval = std::max(0.01f, field.damage_interval);
    const float pulseProgress = std::clamp(
        1.0f - field.damage_timer / pulseInterval, 0.0f, 1.0f);
    const float pulseWave = 0.5f + 0.5f * std::sin(time * (field.torrent_form ? 5.2f : 4.0f));
    const float pulseIntensity = std::clamp(0.35f + 0.65f * pulseProgress, 0.0f, 1.0f);
    const float fadeOut = std::clamp(field.duration / 0.6f, 0.0f, 1.0f);
    const float visibility = std::max(0.22f, fadeOut);

    Color coreColor = field.has_void_keystone ? Color{98, 34, 68, 0}
                                              : Color{122, 18, 26, 0};
    Color edgeColor = field.has_void_keystone ? Color{184, 76, 136, 0}
                                              : Color{212, 64, 78, 0};
    Color pulseColor = field.has_void_keystone ? Color{234, 148, 212, 0}
                                               : Color{255, 124, 138, 0};

    coreColor.a = static_cast<unsigned char>((field.ring_form ? 54.0f : 82.0f) * visibility);
    edgeColor.a = static_cast<unsigned char>((110.0f + 55.0f * pulseWave) * visibility);
    pulseColor.a = static_cast<unsigned char>((135.0f + 70.0f * pulseIntensity) * visibility);

    const float innerFillRadius = field.ring_form ? field.radius * 0.52f : field.radius * 0.92f;
    const float outerPulseRadius = field.radius + 8.0f + 8.0f * pulseIntensity;
    const float innerRingRadius = field.ring_form ? field.radius * 0.72f : field.radius * 0.86f;

    const Vector2 center{pos.x, pos.y};
    DrawCircleGradient(static_cast<int>(pos.x), static_cast<int>(pos.y),
                       innerFillRadius, coreColor, Fade(coreColor, 0.0f));
    DrawRing(center, innerRingRadius, field.radius, 0.0f, 360.0f, 64,
             edgeColor);
    DrawRing(center, field.radius, outerPulseRadius, 0.0f, 360.0f, 64,
             pulseColor);
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

  // 5. Render Portal Vortex (ON TOP of ground tiles, stashes, and archway sprites)
  if (m_portalSystem != nullptr) {
    m_portalSystem->Render(frame.registry, frame.camera);
  }

  static Shader trailShader = {0};
  if (trailShader.id == 0 && m_context->resources != nullptr) {
    trailShader = m_context->resources->getShader(
        entt::hashed_string("sh_sword_trail"));
  }
  if (trailShader.id != 0) {
    NoMoreDay::systems::TrailSystem::Render(frame.registry, trailShader);
  }
  NoMoreDay::systems::SwordIntentVisualSystem::Render(frame.registry);

  NoMoreDay::systems::HoloBladeRenderSystem::Render(frame.registry, *m_context);
}

void GameplayRenderAdapter::ExecuteVFXPass(render::GameplayRenderFrame &frame) {
  auto &qualityManager = render::core::QualityTierManager::Get();
  const auto &vfxConfig = qualityManager.IsInitialized()
                              ? qualityManager.GetConfig()
                              : render::core::RenderConfig{};
  const uint8_t vfxTier = qualityManager.IsInitialized()
                              ? static_cast<uint8_t>(qualityManager.GetTier())
                              : static_cast<uint8_t>(
                                    render::core::QualityTier::Medium);

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
    case VisualEffectType::SwordIntentBurst: {
      const float radius = 26.0f + currentScale * 36.0f;
      DrawRing({pos.x, pos.y}, radius * 0.65f, radius, 0.0f, 360.0f, 28,
               Fade(color, 0.42f));
      DrawCircleLinesV({pos.x, pos.y}, radius * 0.9f, Fade(WHITE, 0.45f));
      DrawPolyLinesEx({pos.x, pos.y}, 6, radius * 0.72f, lifeRatio * 120.0f, 2.0f,
                      Fade(color, 0.62f));

      const bool distortionAllowed =
          vfxConfig.distortionEnabled &&
          vfxTier >= static_cast<uint8_t>(render::core::QualityTier::High);
      if (distortionAllowed && effect.param1 < 0.5f && lifeRatio < 0.2f) {
        ::RenderSystem::AddDistortionSource(pos.x, pos.y, radius, 0.16f);
        effect.param1 = 1.0f;
      }
      break;
    }
    default:
      break;
    }
  }

  auto projView = frame.registry.view<const Position, NoMoreDay::Projectile>();
  for (auto entity : projView) {
    const auto &pos = projView.get<const Position>(entity);
    auto &proj = projView.get<NoMoreDay::Projectile>(entity);
    components::GPUSkillEffect eff;
    float ax = pos.x;
    float ay = pos.y;
    if (const auto *vel = frame.registry.try_get<Velocity>(entity)) {
      ax += vel->vx * m_context->renderAlpha * (1.0f / 60.0f);
      ay += vel->vy * m_context->renderAlpha * (1.0f / 60.0f);
      eff.velocity = {vel->vx, vel->vy};
    }
    eff.position = {ax, ay};
    uint32_t projectileSkillId = 0u;
    if (const auto *skillComp =
            frame.registry.try_get<NoMoreDay::SkillComponent>(entity)) {
      projectileSkillId = skillComp->skill_id;
    }
    // Skill 7 channel tick uses stationary projectile as logic-only hitbox.
    // Skip projectile mesh rendering to avoid default fan sector artifact.
    if (projectileSkillId == 7u && proj.speed <= 0.01f) {
      continue;
    }
    proj.hasRendered = true;

    float visualRadius = (proj.radius > 1.0f) ? proj.radius : 5.0f;
    float visualArc = (proj.arcWidth > 0.0f) ? proj.arcWidth : 45.0f;
    if (projectileSkillId == 2u) {
      // Skill 2 moon blades remain slightly slimmer than gameplay hit size,
      // but should still scale with specialization (e.g. larger blade nodes).
      visualRadius = std::max(8.0f, visualRadius * 0.62f);
      visualArc = std::max(30.0f, visualArc * 0.72f);
    }
    eff.radius = visualRadius;
    eff.sectorAngle = visualArc;
    eff.type = static_cast<float>(proj.visualType);
    eff.flags = render::skillfx::PackSkillEffectFlags(0u, projectileSkillId);
    if (projectileSkillId == 0u) {
      LOG_LIMITED_WARN(1.0f,
                       "RenderSystem: projectile GPUSkillEffect missing skillId "
                       "for entity {}",
                       static_cast<uint32_t>(entity));
    }

    if (const auto *col = frame.registry.try_get<ColorComponent>(entity)) {
      eff.coreColor = ColorNormalize(col->color);
      eff.glowColor = ColorNormalize(col->color);
    } else {
      eff.coreColor = {1, 1, 1, 1};
      eff.glowColor = {0.8f, 0.8f, 1, 0.5f};
    }
    systems::GPUSkillEffectSystem::Get().Submit(eff);
  }

  static thread_local std::vector<
      systems::GPUSkillEffectSystem::DistortionRequest>
      s_skillDistortionRequests;
  s_skillDistortionRequests.clear();
  systems::GPUSkillEffectSystem::Get().DrainDistortionRequests(
      s_skillDistortionRequests);
  for (const auto &request : s_skillDistortionRequests) {
    ::RenderSystem::AddDistortionSource(request.worldX, request.worldY,
                                              request.radius, request.strength);
  }

  static thread_local std::vector<
      systems::GPUSkillEffectSystem::ResistOverlayRequest>
      s_resistOverlayRequests;
  s_resistOverlayRequests.clear();
  systems::GPUSkillEffectSystem::Get().DrainResistOverlayRequests(
      s_resistOverlayRequests);
  const bool lowTier =
      vfxTier <= static_cast<uint8_t>(render::core::QualityTier::Low);
  const bool highTier =
      vfxTier >= static_cast<uint8_t>(render::core::QualityTier::High);
  for (const auto &request : s_resistOverlayRequests) {
    const auto type =
        static_cast<NoMoreDay::SkillVfxResistDebuffType>(request.resistDebuffType);
    Color overlayColor = WHITE;
    switch (type) {
    case NoMoreDay::SkillVfxResistDebuffType::TypeA:
      overlayColor = Color{255, 180, 60, 220};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeB:
      overlayColor = Color{220, 80, 80, 220};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeC:
      overlayColor = Color{120, 220, 255, 220};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeD:
      overlayColor = Color{180, 140, 255, 220};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeE:
      overlayColor = Color{255, 120, 210, 220};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::None:
    default:
      overlayColor = WHITE;
      break;
    }

    const float radius = std::clamp(15.0f * request.intensity, 9.0f, 34.0f);
    const Vector2 center = request.worldPos;
    DrawCircleLinesV(center, radius, Fade(overlayColor, 0.88f));

    Vector2 axisDir = {1.0f, 0.0f};
    switch (type) {
    case NoMoreDay::SkillVfxResistDebuffType::TypeA:
      axisDir = {1.0f, 0.0f};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeB:
      axisDir = {0.0f, 1.0f};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeC:
      axisDir = {0.7f, 0.7f};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeD:
      axisDir = {-0.7f, 0.7f};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeE:
      axisDir = {-1.0f, 0.0f};
      break;
    case NoMoreDay::SkillVfxResistDebuffType::None:
    default:
      axisDir = {1.0f, 0.0f};
      break;
    }
    axisDir = Vector2Normalize(axisDir);

    if (lowTier) {
      const Vector2 start = Vector2Subtract(center, Vector2Scale(axisDir, radius * 0.58f));
      const Vector2 end = Vector2Add(center, Vector2Scale(axisDir, radius * 0.58f));
      DrawLineEx(start, end, 2.0f, Fade(overlayColor, 0.92f));
      continue;
    }

    DrawRing(center, radius * 0.62f, radius, 0.0f, 360.0f, 24,
             Fade(overlayColor, 0.28f));
    switch (type) {
    case NoMoreDay::SkillVfxResistDebuffType::TypeA: {
      for (int i = 0; i < 3; ++i) {
        const float angle = (i * 120.0f) * DEG2RAD;
        const Vector2 dir = {std::cos(angle), std::sin(angle)};
        DrawLineEx(center, Vector2Add(center, Vector2Scale(dir, radius * 0.72f)), 2.0f,
                   Fade(overlayColor, 0.85f));
      }
      break;
    }
    case NoMoreDay::SkillVfxResistDebuffType::TypeB: {
      const Vector2 diagA = Vector2Scale(Vector2Normalize(Vector2{1.0f, 1.0f}), radius * 0.62f);
      const Vector2 diagB =
          Vector2Scale(Vector2Normalize(Vector2{1.0f, -1.0f}), radius * 0.62f);
      DrawLineEx(Vector2Subtract(center, diagA), Vector2Add(center, diagA), 2.0f,
                 Fade(overlayColor, 0.86f));
      DrawLineEx(Vector2Subtract(center, diagB), Vector2Add(center, diagB), 2.0f,
                 Fade(overlayColor, 0.86f));
      break;
    }
    case NoMoreDay::SkillVfxResistDebuffType::TypeC:
      DrawRing(center, radius * 0.36f, radius * 0.78f, 40.0f, 180.0f, 20,
               Fade(overlayColor, 0.4f));
      DrawRing(center, radius * 0.36f, radius * 0.78f, 220.0f, 340.0f, 20,
               Fade(overlayColor, 0.4f));
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeD:
      DrawPolyLinesEx(center, 4, radius * 0.7f, 45.0f, 2.0f, Fade(overlayColor, 0.88f));
      break;
    case NoMoreDay::SkillVfxResistDebuffType::TypeE:
      DrawPolyLinesEx(center, 5, radius * 0.72f, -18.0f, 2.0f, Fade(overlayColor, 0.88f));
      break;
    case NoMoreDay::SkillVfxResistDebuffType::None:
    default:
      break;
    }

    if (highTier) {
      DrawCircleV(center, 2.0f + radius * 0.05f, Fade(WHITE, 0.65f));
    }
  }
}

void GameplayRenderAdapter::ExecuteUIWorldPass(render::GameplayRenderFrame &frame) {
  ZoneScopedN("GameplayRenderAdapter::ExecuteUIWorldPass");
  // R3 (remediation, design §3.5): the UIWorldPass opens a fresh frame token
  // FIRST, before any branch or quality setting can early-return — so every
  // path (CPU loot, GPU loot, text on/off) clears the previous pass's proxies
  // and hover and rotates the token. No reader can observe a stale
  // previous-pass frame (H-01 root cause). Skipped only when no frame is
  // bound.
  if (m_worldFrame != nullptr) {
    m_worldFrame->BeginFrame(++m_frameCounter);
  }
  if (frame.labelBuffer != nullptr) {
    frame.labelBuffer->clear();
  }
  if (frame.glyphBuffer != nullptr) {
    frame.glyphBuffer->clear();
  }
  if (frame.beamBuffer != nullptr) {
    frame.beamBuffer->clear();
  }

  if (!frame.gpuTextEnabled) {
    auto popupView = frame.registry.view<const Position, const DamagePopup>();
    popupView.each([&frame, this](const auto &pos, const auto &popup) {
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
                                                  ? TextFormat(
                                                        "Block %d",
                                                        static_cast<int>(
                                                            popup.damage))
                                                  : TextFormat(
                                                        "%d",
                                                        static_cast<int>(
                                                            popup.damage)))));
      const float fontSize =
          (popup.isCrit ? 36.0f : 28.0f) * popup.currentScale * m_fontScale;
      if (IsFontValid(frame.font)) {
        DrawTextEx(frame.font, text, {pos.x + 2, pos.y + 2}, fontSize, 1.0f,
                   Fade(BLACK, alpha * 0.8f));
        DrawTextEx(frame.font, text, {pos.x, pos.y}, fontSize, 1.0f, color);
      }
    });
  }

  // R3 (design §3.5.1-2): the read-only query/cull visible-proxy producer runs
  // on BOTH the CPU and GPU loot paths; the GPU path only skips the CPU
  // label/glyph/beam output below, never the proxy fill.
  CollectVisibleItemProxies(frame);

  if (frame.gpuLootEnabled) {
    return;
  }

  NoMoreDay::utils::ScopedTimer itemTimer("Loot Label Collection", 100);
  BuildCpuLootLabels(frame);
}

void GameplayRenderAdapter::CollectVisibleItemProxies(
    render::GameplayRenderFrame &frame) {
  s_candidates.clear();

  const Vector2 vTL = GetScreenToWorld2D({0, 0}, frame.camera);
  const Vector2 vBR =
      GetScreenToWorld2D({static_cast<float>(GetScreenWidth()),
                          static_cast<float>(GetScreenHeight())},
                         frame.camera);
  const Rectangle viewRect = {vTL.x - 100, vTL.y - 100, (vBR.x - vTL.x) + 200,
                              (vBR.y - vTL.y) + 200};

  const Vector2 playerRef = m_hasPlayer ? m_playerPos : frame.camera.target;
  constexpr int kMaxCollectCandidates = 256;
  int collectedCount = 0;

  // U8 收尾: 网格本体与脏标记归 item 域 LootGridSystem（原 UiShared::s_itemGrid 读取）。
  if (systems::LootGridSystem::GetGrid() != nullptr) {
    systems::LootGridSystem::GetGrid()->query(
        {frame.camera.target.x, frame.camera.target.y}, 1000.0f,
        [&](entt::entity entity, const Vector2 &pos) -> bool {
          if (collectedCount >= kMaxCollectCandidates) {
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

            Color rarityColor = UiShared::GetRarityColor(item->rarity);
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

            ++collectedCount;
            int fSize = static_cast<int>(std::round(18.0f * scale * m_fontScale));
            if (fSize < 12) {
              fSize = 12;
            }
            // R3 (design §3.5.2): read-only rect estimate — reuse the label
            // cache when present, otherwise a conservative default. No
            // component write here; the CPU label builder measures and caches
            // below (pre-existing LabelCacheComponent write kept there).
            const auto *labelCache =
                frame.registry.try_get<LabelCacheComponent>(entity);
            Vector2 tSize;
            if (labelCache != nullptr && labelCache->isValid) {
              tSize = labelCache->cachedSize;
            } else {
              tSize = {100.0f, static_cast<float>(fSize)};
            }
            const Rectangle bg = {pos.x - tSize.x / 2 - 5,
                                  pos.y - 30.0f * scale - 2, tSize.x + 10,
                                  tSize.y + 4};
            const float dx = pos.x - playerRef.x;
            const float dy = pos.y - playerRef.y;
            s_candidates.push_back({entity, pos, tSize, rarityColor, scale,
                                    item->name, false, bg, item->rarity,
                                    dx * dx + dy * dy, 0, emphasized});
            // R3 (design §3.5.1): proxy fill runs on every path, including GPU
            // loot. The CPU builder no longer writes proxies.
            if (m_worldFrame != nullptr) {
              m_worldFrame->AddItem(entity, bg, 0.0f);
            }
          } else if (const auto *gold = frame.registry.try_get<GoldComponent>(
                         entity)) {
            ++collectedCount;
            int fSize = static_cast<int>(std::round(16.0f * m_fontScale));
            if (fSize < 10) {
              fSize = 10;
            }
            // Read-only estimate as above; gold label text is formatted by the
            // CPU builder (cachedText write stays there).
            const auto *labelCache =
                frame.registry.try_get<LabelCacheComponent>(entity);
            Vector2 tSize;
            if (labelCache != nullptr && labelCache->isValid) {
              tSize = labelCache->cachedSize;
            } else {
              tSize = {70.0f, static_cast<float>(fSize)};
            }
            const Rectangle bg = {pos.x - tSize.x / 2 - 5, pos.y - 25.0f - 2,
                                  tSize.x + 10, tSize.y + 4};
            const float dx = pos.x - playerRef.x;
            const float dy = pos.y - playerRef.y;
            s_candidates.push_back({entity, pos, tSize, GOLD, 1.0f, "", true,
                                    bg, NoMoreDay::Rarity::Common,
                                    dx * dx + dy * dy,
                                    static_cast<int>(gold->amount), false});
            if (m_worldFrame != nullptr) {
              m_worldFrame->AddItem(entity, bg, 0.0f);
            }
          }
          return true;
        });
  }
}

void GameplayRenderAdapter::BuildCpuLootLabels(
    render::GameplayRenderFrame &frame) {
  // B5: per-frame MSDF glyph out-fields. The Engine applies a single
  // uScreenPxRange uniform to the whole glyph batch, so the representative
  // pixel range is derived from the largest font size used this frame (item
  // labels — 18px tier scaled by emphasis — always exceed gold labels, 16px
  // tier). Known visual deviation: smaller/gold labels render with a slightly
  // different edge thickness, accepted for manual verification.
  // B5/H-01: MSDF glyph templates only when the MSDF atlas registry is
  // available (GPU-text bootstrap registered atlas + metrics) AND the
  // Engine broadcasts its MSDF glyph resources as ready
  // (glyphMsdfEngineReady: glyph_msdf.frag loaded + GPUTextSystem
  // initialized). Otherwise the bitmap path runs, byte-for-byte the
  // pre-B5 behaviour. This flag represents the final per-frame MSDF
  // mode decision and doubles as the cache-template source discriminator.
  // Hoisted before the font-size quantization so the MSDF path can skip it:
  // the MSDF atlas scales to any font size crisply, while the bitmap path
  // must stay on integer texel multiples to avoid resampling blur.
  const bool msdfAvailable =
      ::NoMoreDay::render::MSDFAtlasRegistry::Get().IsAvailable() &&
      frame.glyphMsdfEngineReady;

  bool glyphMsdfUsedThisFrame = false;
  int maxGlyphFSize = 0;

  const bool enableLootBeams =
      render::core::QualityTierManager::Get()
          .GetConfig()
          .dynamicLightingEnabled;
  // Beams are CPU-path output (like labels/glyphs): the GPU loot path skipped
  // them before R3 and still does. Preserved for every collected rare /
  // emphasized item (budget selection below only filters label instances).
  if (enableLootBeams && frame.beamBuffer != nullptr) {
    for (const auto &cand : s_candidates) {
      if (cand.rarity >= NoMoreDay::Rarity::Rare || cand.emphasized) {
        render::GPUBeamInstance bi;
        bi.position = {cand.pos.x, cand.pos.y};
        bi.size = {24.0f * cand.scale, 120.0f * cand.scale};
        bi.color = ColorNormalize(cand.color);
        bi.time = static_cast<float>(GetTime());
        frame.beamBuffer->push_back(bi);
      }
    }
  }

  // Budget selection by priority (design §4.1): collect-then-prioritize so the
  // 64/32/48 rules act on the importance-ordered stream, not grid traversal
  // order (root cause: aborting the whole query at labelCount>=64 starved the
  // bottom-right region).
  if (!s_candidates.empty()) {
    std::vector<NoMoreDay::render::LootLabelCandidate> budgetCandidates;
    budgetCandidates.reserve(s_candidates.size());
    for (size_t i = 0; i < s_candidates.size(); ++i) {
      const LabelCandidate &cand = s_candidates[i];
      NoMoreDay::render::LootLabelCandidate bc;
      bc.isGold = cand.isGold;
      bc.emphasized = cand.emphasized;
      bc.rarityOrdinal = static_cast<int>(cand.rarity);
      bc.distSq = cand.distSq;
      bc.goldAmount = cand.amount;
      bc.scale = cand.scale;
      bc.visible = true;
      bc.stableKey = static_cast<uint64_t>(i);
      budgetCandidates.push_back(bc);
    }
    const auto selected =
        NoMoreDay::render::LootLabelBudget::SelectLootLabels(budgetCandidates);
    std::vector<LabelCandidate> kept;
    kept.reserve(selected.size());
    for (const auto &sel : selected) {
      if (sel.stableKey < s_candidates.size()) {
        kept.push_back(s_candidates[static_cast<size_t>(sel.stableKey)]);
      }
    }
    s_candidates.swap(kept);
  }

  if (!s_candidates.empty()) {
    std::sort(s_candidates.begin(), s_candidates.end(),
              [](const LabelCandidate &a, const LabelCandidate &b) {
                return a.pos.y > b.pos.y;
              });

    for (size_t i = 0; i < s_candidates.size(); ++i) {
      auto &cand = s_candidates[i];

      // R3 (design §3.5.2): label measurement + LabelCacheComponent caching
      // moved here from the collection (the proxy producer is read-only).
      auto &labelCache =
          frame.registry.get_or_emplace<LabelCacheComponent>(cand.entity);
      int fSize = cand.isGold
                      ? static_cast<int>(std::round(16.0f * m_fontScale))
                      : static_cast<int>(
                            std::round(18.0f * cand.scale * m_fontScale));
      if (fSize < 10) {
        fSize = 10;
      }
      // Phase A: quantize scaleFactor = fSize / baseSize to an integer
      // multiple (>= 1) so glyph quads scale by whole atlas texels; a
      // fractional scale would resample the 24px bitmap atlas bilinearly and
      // blur the label. The MSDF path skips quantization: the distance field
      // scales to any font size crisply.
      if (!msdfAvailable && IsFontValid(frame.font) &&
          frame.font.baseSize > 0) {
        const int integerScale =
            std::max(1, static_cast<int>(std::round(
                            static_cast<float>(fSize) /
                            static_cast<float>(frame.font.baseSize))));
        fSize = frame.font.baseSize * integerScale;
      }
      if (!labelCache.isValid || labelCache.lastFontSize != fSize ||
          (!cand.isGold &&
           labelCache.lastRarityHash != static_cast<uint32_t>(cand.rarity))) {
        if (cand.isGold && !labelCache.isValid) {
          NoMoreDay::utils::FormatToBuffer(labelCache.cachedText, "{} Gold",
                                           cand.amount);
          cand.text = labelCache.cachedText;
        }
        const char *measureText =
            cand.isGold ? labelCache.cachedText : cand.text.c_str();
        labelCache.cachedSize =
            IsFontValid(frame.font)
                ? ::NoMoreDay::render::LootTextBatcher::MeasureText(
                      frame.font, measureText, static_cast<float>(fSize))
                : Vector2{static_cast<float>(MeasureText(measureText, fSize)),
                          static_cast<float>(fSize)};
        labelCache.lastFontSize = fSize;
        if (!cand.isGold) {
          labelCache.lastRarityHash = static_cast<uint32_t>(cand.rarity);
        }
        labelCache.isValid = true;
        // Layout may have changed; invalidate cached glyph templates.
        labelCache.glyphTemplates.clear();
        labelCache.cachedGlyphs.clear();
      }
      // Gold label text lives in the label cache; make the candidate carry it
      // for the glyph builder (the producer cannot format it read-only).
      if (cand.isGold && cand.text.empty()) {
        cand.text = labelCache.cachedText;
      }

      const Vector2 tSize = labelCache.cachedSize;
      const Rectangle bg =
          cand.isGold
              ? Rectangle{cand.pos.x - tSize.x / 2 - 5,
                          cand.pos.y - 25.0f - 2, tSize.x + 10, tSize.y + 4}
              : Rectangle{cand.pos.x - tSize.x / 2 - 5,
                          cand.pos.y - 30.0f * cand.scale - 2, tSize.x + 10,
                          tSize.y + 4};
      cand.currentRect = bg;

      // Greedy vertical stacking: each label checks every previously placed
      // label and pushes itself upward until clear (mainstream ARPG
      // non-overlapping label layout). The push budget caps pathological
      // cases; with the budget exhausted the label stays at its last
      // candidate slot (all higher slots are occupied).
      bool overlap = true;
      int safety = 0;
      while (overlap && safety < 64) {
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

      // Snap currentRect origin to camera screen pixels to avoid subpixel bilinear blurring
      const float zoom = frame.camera.zoom;
      if (zoom > 1e-4f) {
        const float screenX =
            (cand.currentRect.x - frame.camera.target.x) * zoom +
            frame.camera.offset.x;
        const float screenY =
            (cand.currentRect.y - frame.camera.target.y) * zoom +
            frame.camera.offset.y;
        const float snappedScreenX = std::round(screenX);
        const float snappedScreenY = std::round(screenY);
        cand.currentRect.x =
            frame.camera.target.x + (snappedScreenX - frame.camera.offset.x) / zoom;
        cand.currentRect.y =
            frame.camera.target.y + (snappedScreenY - frame.camera.offset.y) / zoom;
      }

      // U8 (plan §11): hover read moves from UiShared::HoveredItem() to the
      // frame-scoped WorldUiFrame. Unbound frame => no hover highlight.
      const bool hovered =
          (m_worldFrame != nullptr) &&
          (cand.entity == m_worldFrame->HoveredItem());
      components::GPULabelInstance inst;
      inst.position = {cand.currentRect.x, cand.currentRect.y};
      inst.size = {cand.currentRect.width, cand.currentRect.height};
      inst.bgColor = ColorNormalize(Color{14, 14, 18, 255});
      inst.borderColor =
          ColorNormalize(hovered ? WHITE : ColorAlpha(cand.color, 0.95f));
      inst.borderWidth = hovered ? 2.0f : 1.0f;
      inst.cornerRadius = 3.0f;
      if (frame.labelBuffer != nullptr) {
        frame.labelBuffer->push_back(inst);
      }
      // R3: the visible-item proxy write now lives in
      // CollectVisibleItemProxies (runs on every path); nothing to write here.

      if (IsFontValid(frame.font) && frame.glyphBuffer != nullptr) {
        if (fSize < 10) {
          fSize = 10;
        }
        // Cached-template path: rebuild glyph layout templates only when the
        // cached size is stale (font size, text, or template source changed),
        // then translate the origin-relative cached instances to the final
        // rect each frame. The template source (bitmap vs MSDF) participates
        // in the invalidation: UVs and size math differ between the two
        // atlases and must never be reused across a mode switch.
        auto *labelCachePtr =
            frame.registry.try_get<LabelCacheComponent>(cand.entity);
        if (labelCachePtr != nullptr &&
            (labelCachePtr->glyphTemplates.size() == 0 ||
             labelCachePtr->lastUsedMsdf != msdfAvailable)) {
          labelCachePtr->glyphTemplates.clear();
          labelCachePtr->cachedGlyphs.clear();
          if (msdfAvailable) {
            ::NoMoreDay::render::LootTextBatcher::BuildTemplatesMsdf(
                cand.text, static_cast<float>(fSize),
                labelCachePtr->glyphTemplates);
          } else {
            ::NoMoreDay::render::LootTextBatcher::BuildTemplates(
                frame.font, cand.text, static_cast<float>(fSize),
                labelCachePtr->glyphTemplates);
          }
          ::NoMoreDay::render::LootTextBatcher::BatchString(
              frame.font, cand.text, {0.0f, 0.0f},
              static_cast<float>(fSize), RAYWHITE, labelCachePtr->cachedGlyphs);
          labelCachePtr->lastFontSize = fSize;
          labelCachePtr->lastUsedMsdf = msdfAvailable;
        }
        if (labelCachePtr != nullptr &&
            labelCachePtr->glyphTemplates.size() > 0) {
          ::NoMoreDay::render::LootTextBatcher::WriteInstances(
              labelCachePtr->glyphTemplates, labelCachePtr->cachedGlyphs,
              {cand.currentRect.x + 5, cand.currentRect.y + 2},
              ColorToInt(cand.color), frame.camera.zoom, *frame.glyphBuffer);
          // B5: record that this frame actually emitted glyphs from MSDF
          // templates (WriteInstances returns early unless the cached layout
          // matches the templates), and track the largest font size seen.
          if (msdfAvailable &&
              labelCachePtr->cachedGlyphs.size() ==
                  labelCachePtr->glyphTemplates.size()) {
            glyphMsdfUsedThisFrame = true;
            maxGlyphFSize = std::max(maxGlyphFSize, fSize);
          }
        } else if (msdfAvailable) {
          // H-02: MSDF mode is decided this frame but this label produced no
          // MSDF templates (every codepoint missed the atlas — e.g. GBK
          // extension characters) or has no cache entry. Emitting bitmap UVs
          // here would be sampled by the MSDF shader from the MSDF atlas
          // (wrong texels), so skip this label's glyph instances instead; the
          // background label still draws. One-time log keeps the skipped
          // label visible.
          if (!cand.text.empty()) {
            static bool s_loggedMsdfEmptyTemplates = false;
            if (!s_loggedMsdfEmptyTemplates) {
              LOG_WARN("GameplayRenderAdapter: label has no MSDF glyph coverage "
                       "in MSDF mode; label glyphs skipped.");
              s_loggedMsdfEmptyTemplates = true;
            }
          }
        } else {
          // Bitmap path: bitmap UVs sampled by the bitmap glyph shader.
          ::NoMoreDay::render::LootTextBatcher::BatchString(
              frame.font, cand.text,
              {cand.currentRect.x + 5, cand.currentRect.y + 2},
              static_cast<float>(fSize), cand.color, *frame.glyphBuffer);
        }
      }
    }
  }

  // B5: publish the MSDF glyph decision to the Engine glyph draw. When the
  // MSDF atlas is unavailable the mode stays disabled and the bitmap path
  // runs exactly as before B5. The pixel range scales the font's em size to
  // screen pixels at the current zoom: pxRange = distanceRange *
  // (maxFontSizePx * zoom / emSize).
  frame.glyphMsdfEnabled = glyphMsdfUsedThisFrame;
  if (glyphMsdfUsedThisFrame) {
    const auto &msdfRegistry = ::NoMoreDay::render::MSDFAtlasRegistry::Get();
    frame.glyphMsdfPxRange =
        msdfRegistry.GetDistanceRange() *
        (static_cast<float>(maxGlyphFSize) * frame.camera.zoom /
         msdfRegistry.GetEmSize());
  } else {
    frame.glyphMsdfPxRange = 1.0f;
  }
}

} // namespace NoMoreDay
