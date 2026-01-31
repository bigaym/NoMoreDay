#include "game/systems/world/PortalSystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/persistence/SaveManager.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/MapComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/WorldState.hpp"
#include "game/systems/world/MapAffixCalculator.hpp"
#include "game/systems/world/MapAffixRegistry.hpp"
#include "raymath.h"
#include <cmath>
#include <algorithm>

namespace NoMoreDay {

PortalSystem::PortalSystem(SceneManager &sceneManager)
    : m_sceneManager(sceneManager) {
    // Load VFX Resources
    m_vortexShader = LoadShader("assets/shaders/vfx/portal_vortex.vs", "assets/shaders/vfx/portal_vortex.fs");
    m_noiseTexture = LoadTexture("assets/textures/vfx/vfx_energy_noise.png");
    
    // Get Uniform Locations
    m_locTime = GetShaderLocation(m_vortexShader, "uTime");
    m_locColor = GetShaderLocation(m_vortexShader, "uColor");
    m_locSwirl = GetShaderLocation(m_vortexShader, "uSwirlStrength");
    m_locCore = GetShaderLocation(m_vortexShader, "uCoreSize");
    
    // Set Default Uniforms
    float defaultSwirl = 3.0f;
    float defaultCore = 0.15f;
    SetShaderValue(m_vortexShader, m_locSwirl, &defaultSwirl, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_vortexShader, m_locCore, &defaultCore, SHADER_UNIFORM_FLOAT);
}

PortalSystem::~PortalSystem() {
    if (m_vortexShader.id != 0) UnloadShader(m_vortexShader);
    if (m_noiseTexture.id != 0) UnloadTexture(m_noiseTexture);
}

void PortalSystem::Update(entt::registry &registry, float dt) {
  // 1. Update casting state
  UpdateTownPortalCasting(registry, dt);

  // 2. Update portal animations
  UpdatePortalAnimations(registry, dt);

  // 3. Check portal collisions (only if not transitioning)
  if (!m_sceneManager.IsTransitioning()) {
    UpdatePortalCollision(registry);
  }
}

void PortalSystem::UpdatePortalCollision(entt::registry &registry) {
  auto portals = registry.view<PortalComponent, Position>();
  auto players = registry.view<PlayerTag, Position>();

  for (auto player : players) {
    const auto &pPos = players.get<Position>(player);
    bool inAnyPortal = false;

    for (auto portal : portals) {
      const auto &portalComp = portals.get<PortalComponent>(portal);
      if (!portalComp.isActive)
        continue;

      const auto &portPos = portals.get<Position>(portal);

      float dx = pPos.x - portPos.x;
      float dy = pPos.y - portPos.y;
      float distSq = dx * dx + dy * dy;

      // Interaction range (20 units)
      if (distSq < 20.0f * 20.0f) {
        inAnyPortal = true;

        // If we are still in the same portal we just triggered, do nothing
        if (portal == m_lastTriggeredPortal) {
          continue;
        }

        // New activation
        m_lastTriggeredPortal = portal;

        // Handle NextLevel portals
        if (portalComp.type == PortalType::NextLevel) {
          if (registry.ctx().contains<ActiveDimensionalState>()) {
              auto& state = registry.ctx().get<ActiveDimensionalState>();
              if (state.isActive) {
                  LOG_INFO("Advancing Dimensional Rift layer...");
                  AdvanceRiftLayer(registry, player);
                  return;
              }
          }
          
          LOG_INFO("Player triggered NextLevel portal - opening Mosaic Editor");
          registry.emplace_or_replace<PendingMosaicEditorTag>(player);
          return;
        }

        // [New] Handle Dimensional Gate (Town Hub)
        if (portalComp.type == PortalType::DimensionalGate) {
            LOG_INFO("Player triggered Dimensional Gate - opening Rift Window");
            registry.emplace_or_replace<PendingDimensionalGateTag>(player);
            return;
        }

        // Handle other portal types - direct transition
        LOG_INFO("Player triggered portal to {} (Level {})",
                 (int)portalComp.targetBiome, portalComp.targetLevel);

        // Trigger auto-save when entering town
        if (portalComp.targetBiome == NoMoreDay::BiomeID::Town) {
          LOG_INFO("Entering Town - triggering auto-save");
          if (NoMoreDay::SaveManager::Get().IsInitialized()) {
            NoMoreDay::SaveManager::Get().saveCharacterAsync(registry, 0);
          }
        }

        // Store origin info for return portal
        m_sceneManager.SetOriginInfo(m_sceneManager.GetCurrentBiome(),
                                     m_sceneManager.GetCurrentLevel(), pPos.x,
                                     pPos.y);

        m_sceneManager.RequestTransition(portalComp.targetBiome,
                                         portalComp.targetLevel,
                                         portalComp.targetEntranceId);
        return;
      }
    }

    // If player is not in ANY portal, reset the tracker
    if (!inAnyPortal) {
      m_lastTriggeredPortal = entt::null;
    }
  }
}

void PortalSystem::AdvanceRiftLayer(entt::registry &registry, entt::entity player) {
    if (!registry.ctx().contains<ActiveDimensionalState>()) return;
    auto& state = registry.ctx().get<ActiveDimensionalState>();

    // 1. Increment depth
    state.currentDepth++;
    
    // 2. Fragment Decay & Grid Cleanup
        for (int i = 0; i < MosaicGrid::TOTAL_CELLS; ++i) {
            auto& snap = state.gridSnapshots[i];
            if (snap.hasFragment) {
                snap.remainingLayers--;
                 if (snap.remainingLayers <= 0) {
                    snap.hasFragment = false;
    
                    // Also destroy runtime entity if it still exists
                    entt::entity fragEntity = state.sourceGrid.cells[i];
                    if (registry.valid(fragEntity)) {
                        registry.destroy(fragEntity);
                        state.sourceGrid.cells[i] = entt::null;
                    }
                }
            }
        }
    
        // 3. Recalculate State from residual snapshots
        state.explicitAffixes = MapAffixCalculator::GenerateAffixesFromSnapshots(state.gridSnapshots);
        
        // Apply Depth Scaling to affix values
        float depthMult = 1.0f + (state.currentDepth - 1) * 0.1f;
        for (auto& aff : state.explicitAffixes) {
            aff.value *= depthMult;
        }
    
        state.aggregatedAffixes = MapAffixCalculator::AggregateAffixes(state.explicitAffixes);
        state.difficultyScore = MapAffixCalculator::CalculateDifficultyScore(state.explicitAffixes);
    
        auto rewards = MapAffixCalculator::CalculateRewards(state.difficultyScore, state.currentDepth);    state.calculatedRarity = rewards.rarityBonus;
    state.calculatedQuantity = rewards.quantityBonus;

    // 4. Check for Rift Completion (No fragments left or reached max depth)
    bool hasFragments = false;
    for (const auto& snap : state.gridSnapshots) if (snap.hasFragment) { hasFragments = true; break; }

    if (!hasFragments || state.currentDepth > state.maxDepth) {
        LOG_INFO("Rift Completed at depth {}!", state.currentDepth - 1);
        state.isActive = false;
        state.isCompleted = true;
        m_sceneManager.RequestTransition(BiomeID::Town, 1, "");
        return;
    }

    // 5. Normal Progression - New Seed for New Level
    state.seed = (uint32_t)GetTime() + state.currentDepth;
    LOG_INFO("Transitioning to Depth {}", state.currentDepth);
    m_sceneManager.RequestTransition(state.biome, state.currentDepth, "");
}

void PortalSystem::UpdateTownPortalCasting(entt::registry &registry, float dt) {
  auto view = registry.view<TownPortalCastingComponent, Position>();

  for (auto entity : view) {
    auto &casting = view.get<TownPortalCastingComponent>(entity);
    auto &pos = view.get<Position>(entity);

    if (!casting.isCasting)
      continue;

    // Check if player moved (interrupt casting)
    float dx = pos.x - casting.castX;
    float dy = pos.y - casting.castY;
    if (dx * dx + dy * dy > 5.0f * 5.0f) {
      LOG_INFO("Town Portal casting interrupted - player moved");
      casting.isCasting = false;
      casting.elapsedTime = 0.0f;
      continue;
    }

    casting.elapsedTime += dt;

    // Emit casting particles
    auto &particleSys = systems::GPUParticleSystem::Get();
    float progress = casting.elapsedTime / casting.castTime;
    int particleCount = 1 + (int)(progress * 3.0f);

    for (int i = 0; i < particleCount; ++i) {
      float angle =
          casting.elapsedTime * 8.0f + i * (2.0f * PI / particleCount);
      float radius = 15.0f + progress * 15.0f;

      components::GPUParticle p;
      p.position = {casting.castX + cosf(angle) * radius,
                    casting.castY + sinf(angle) * radius};
      p.velocity = {-sinf(angle) * 20.0f, cosf(angle) * 20.0f - 30.0f};
      p.color = GOLD;
      p.lifetime = 0.5f;
      p.maxLifetime = 0.5f;
      p.scale = 2.0f + progress * 2.0f;
      particleSys.Emit(p);
    }

    // Cast complete
    if (casting.elapsedTime >= casting.castTime) {
      LOG_INFO("Town Portal cast complete!");
      SpawnTownPortal(registry, entity);
      casting.isCasting = false;
      casting.elapsedTime = 0.0f;
    }
  }
}

void PortalSystem::SpawnTownPortal(entt::registry &registry,
                                   entt::entity caster) {
  auto *pos = registry.try_get<Position>(caster);
  if (!pos)
    return;

  auto portal = registry.create();
  registry.emplace<LocalLevelTag>(portal);
  registry.emplace<Position>(portal, pos->x,
                             pos->y + 40.0f); // Slightly in front of player

  PortalComponent pc;
  pc.type = PortalType::Town;
  pc.targetBiome = NoMoreDay::BiomeID::Town;
  pc.targetLevel = 1;
  pc.isActive = true;
  pc.radius = 35.0f;

  // Store origin for return portal
  pc.originBiome = m_sceneManager.GetCurrentBiome();
  pc.originLevel = m_sceneManager.GetCurrentLevel();
  pc.originX = pos->x;
  pc.originY = pos->y;

  registry.emplace<PortalComponent>(portal, pc);

  // Spawn visual effect
  auto &particleSys = systems::GPUParticleSystem::Get();
  auto splash = systems::InkEffectHelper::CreateInkSplash(
      {pos->x, pos->y + 40.0f}, 20, 15.0f, 120.0f);
  for (auto &p : splash) {
    p.color = GOLD;
    particleSys.Emit(p);
  }

  LOG_INFO("Town Portal spawned at ({:.1f}, {:.1f})", pos->x, pos->y + 40.0f);
}

void PortalSystem::UpdatePortalAnimations(entt::registry &registry, float dt) {
  auto view = registry.view<PortalComponent>();
  for (auto entity : view) {
    auto &portal = view.get<PortalComponent>(entity);
    portal.animationTimer += dt;
  }
}

void PortalSystem::StartTownPortalCast(entt::registry &registry,
                                       entt::entity caster) {
  auto &casting = registry.get_or_emplace<TownPortalCastingComponent>(caster);

  if (casting.isCasting) {
    LOG_DEBUG("Already casting Town Portal");
    return;
  }

  auto *pos = registry.try_get<Position>(caster);
  if (!pos)
    return;

  casting.isCasting = true;
  casting.elapsedTime = 0.0f;
  casting.castX = pos->x;
  casting.castY = pos->y;

  LOG_INFO("Started Town Portal cast at ({:.1f}, {:.1f})", pos->x, pos->y);
}

void PortalSystem::CancelTownPortalCast(entt::registry &registry,
                                        entt::entity caster) {
  if (auto *casting = registry.try_get<TownPortalCastingComponent>(caster)) {
    if (casting->isCasting) {
      casting->isCasting = false;
      casting->elapsedTime = 0.0f;
      LOG_INFO("Town Portal cast cancelled");
    }
  }
}

void PortalSystem::Render(entt::registry &registry, const Camera2D &camera) {
  auto view = registry.view<PortalComponent, Position>();

  BeginShaderMode(m_vortexShader);

  // Update Time Uniform
  float time = (float)GetTime();
  SetShaderValue(m_vortexShader, m_locTime, &time, SHADER_UNIFORM_FLOAT);

  for (auto entity : view) {
    const auto &portal = view.get<PortalComponent>(entity);
    const auto &pos = view.get<Position>(entity);

    if (!portal.isActive)
      continue;

    // Skip DimensionalGate ONLY if it has a specific sprite component handling it elsewhere?
    // User requested "Vortex effect for ALL portals".
    // So we render for all.
    // If DimensionalGate has a SpriteComponent, we might want to suppress it here OR overwrite it.
    // But SpriteComponent is rendered by RenderSystem. 
    // Ideally we should REMOVE SpriteComponent from DimensionalGate entities if we use this.
    // Assuming we did that (or will do).

    Color baseColor = PURPLE;
    float yOffsetMult = -0.1f; // Default: Slightly up for ground portals

    switch (portal.type) {
    case PortalType::Town:
    case PortalType::Return:
      baseColor = GOLD; break;
    case PortalType::NextLevel:
      baseColor = SKYBLUE; break;
    case PortalType::DimensionalGate:
      baseColor = {172, 226, 232, 255}; // Light Blue/Cyan matching requested RGB
      yOffsetMult = 0.12f;             // Move down to align with archway center
      break;
    case PortalType::Boss:
      baseColor = RED; break;
    }

    // Set Color Uniform (Normalized float vec4)
    float color[4] = {
        (float)baseColor.r / 255.0f,
        (float)baseColor.g / 255.0f,
        (float)baseColor.b / 255.0f,
        (float)baseColor.a / 255.0f
    };
    SetShaderValue(m_vortexShader, m_locColor, color, SHADER_UNIFORM_VEC4);

    // Calculate Aspect Ratio Size (3:5)
    // Width = Radius * 1.5 (Reduced from 3.0 to fit better)
    float visualWidth = portal.radius * 1.5f;
    float visualHeight = visualWidth * (5.0f / 3.0f); // 3:5 Ratio

    Rectangle destRect = {
        pos.x, 
        pos.y + visualHeight * yOffsetMult, 
        visualWidth, 
        visualHeight
    };
    
    Vector2 origin = {visualWidth / 2.0f, visualHeight / 2.0f};

    // Draw Texture with Shader
    // Using White part of noise texture or full texture? 
    // Shader uses texture0.
    DrawTexturePro(m_noiseTexture, 
                   {0, 0, (float)m_noiseTexture.width, (float)m_noiseTexture.height}, 
                   destRect, origin, 0.0f, WHITE);
  }
  
  EndShaderMode();

  // Render casting progress circle (Standard rendering)
  auto castingView = registry.view<TownPortalCastingComponent, Position>();
  for (auto entity : castingView) {
    const auto &casting = castingView.get<TownPortalCastingComponent>(entity);
    if (!casting.isCasting)
      continue;

    float progress = casting.elapsedTime / casting.castTime;
    int degrees = (int)(progress * 360.0f);

    DrawRing({casting.castX, casting.castY}, 18.0f, 22.0f, 0, degrees, 32,
             ColorAlpha(GOLD, 0.7f));
    DrawRing({casting.castX, casting.castY}, 22.0f, 25.0f, 0, 360, 32,
             ColorAlpha(WHITE, 0.3f));
  }
}

} // namespace NoMoreDay
