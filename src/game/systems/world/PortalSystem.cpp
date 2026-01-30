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
    : m_sceneManager(sceneManager) {}

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

  for (auto entity : view) {
    const auto &portal = view.get<PortalComponent>(entity);
    const auto &pos = view.get<Position>(entity);

    if (!portal.isActive)
      continue;

    // Determine color based on portal type
    // Skip procedural rendering for DimensionalGate (it uses a sprite)
    if (portal.type == PortalType::DimensionalGate) {
        continue;
    }

    Color ringColor = PURPLE;
    Color innerColor = DARKPURPLE;

    switch (portal.type) {
    case PortalType::Town:
    case PortalType::Return:
      ringColor = GOLD;
      innerColor = ORANGE;
      break;
    case PortalType::Boss:
      ringColor = RED;
      innerColor = MAROON;
      break;
    default:
      break;
    }

    float pulse = 1.0f + 0.1f * sinf(portal.animationTimer * 4.0f);
    float radius = portal.radius * pulse;

    // 1. Outer glow ring
    DrawRing({pos.x, pos.y}, radius - 5.0f, radius, 0, 360, 32,
             ColorAlpha(ringColor, 0.4f));
    DrawRing({pos.x, pos.y}, radius - 8.0f, radius - 5.0f, 0, 360, 32,
             ColorAlpha(ringColor, 0.6f));

    // 2. Inner swirl effect (rotating lines)
    int numLines = 6;
    for (int i = 0; i < numLines; ++i) {
      float angle = portal.animationTimer * 2.0f + i * (2.0f * PI / numLines);
      float innerRadius = 5.0f;
      float outerRadius = radius - 10.0f;

      Vector2 start = {pos.x + cosf(angle) * innerRadius,
                       pos.y + sinf(angle) * innerRadius};
      Vector2 end = {pos.x + cosf(angle) * outerRadius,
                     pos.y + sinf(angle) * outerRadius};

      DrawLineEx(start, end, 2.0f, ColorAlpha(innerColor, 0.5f));
    }

    // 3. Center glow
    DrawCircle((int)pos.x, (int)pos.y, 10.0f * pulse, ColorAlpha(WHITE, 0.6f));
    DrawCircle((int)pos.x, (int)pos.y, 6.0f * pulse,
               ColorAlpha(ringColor, 0.8f));

    // 4. Emit periodic particles
    if (fmodf(portal.animationTimer, 0.1f) < 0.016f) {
      auto &particleSys = systems::GPUParticleSystem::Get();
      float pAngle = portal.animationTimer * 5.0f;

      for (int i = 0; i < 2; ++i) {
        float a = pAngle + i * PI;
        components::GPUParticle p;
        p.position = {pos.x + cosf(a) * (radius - 10.0f),
                      pos.y + sinf(a) * (radius - 10.0f)};
        p.velocity = {-sinf(a) * 40.0f, cosf(a) * 40.0f - 20.0f};
        p.color = ringColor;
        p.lifetime = 0.6f;
        p.maxLifetime = 0.6f;
        p.scale = 3.0f;
        particleSys.Emit(p);
      }
    }
  }

  // Render casting progress circle
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
