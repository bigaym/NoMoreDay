#include "engine/render/GPUEntitySync.hpp"
#include "engine/render/RenderConstants.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/Stats.hpp"
#include "game/systems/stats/AttributePipeline.hpp"

namespace NoMoreDay::render {

using namespace NoMoreDay::components;
using NoMoreDay::BuffType;

// =================================================================================================
// GPUPhysicsSync Implementation
// =================================================================================================

void GPUPhysicsSync::Init(const Config &config) { m_config = config; }

int GPUPhysicsSync::Execute(
    entt::registry &registry,
    std::vector<NoMoreDay::components::GPUEntity> &shadowBuffer,
    uint64_t frameCounter) {
  int highWaterMark = 0;
  int maxShadow = (int)shadowBuffer.size();

  auto view = registry.view<Position, Radius, GPUIndex>();

  for (auto entity : view) {
    auto &gpuIdx = view.get<GPUIndex>(entity);
    int slot = gpuIdx.index;

    if (slot < 0 || slot >= maxShadow)
      continue;

    if (slot > highWaterMark)
      highWaterMark = slot;

    const auto &pos = view.get<Position>(entity);
    const auto &radius = view.get<Radius>(entity);

    auto *velPtr = registry.try_get<Velocity>(entity);
    Vector2 velocity = velPtr ? Vector2{velPtr->vx, velPtr->vy} : Vector2{0, 0};

    auto &gpuEntity = shadowBuffer[slot];

    float dx = pos.x - gpuEntity.position.x;
    float dy = pos.y - gpuEntity.position.y;
    if (dx * dx + dy * dy >
        m_config.teleportThreshold * m_config.teleportThreshold) {
      gpuEntity.position = Vector2{pos.x, pos.y};
      gpuEntity.prevPosition = Vector2{pos.x, pos.y};
    } else {
      gpuEntity.prevPosition = gpuEntity.position;
      gpuEntity.position = Vector2{pos.x, pos.y};
    }

    gpuEntity.velocity = velocity;
    gpuEntity.radius = radius.value;
    gpuEntity.frameId = (uint32_t)frameCounter;

    if (auto *sprite = registry.try_get<SpriteComponent>(entity)) {
      gpuEntity.type = sprite->textureLayerIndex;
    } else {
      gpuEntity.type = NoMoreDay::Constants::GPU::SDF_CIRCLE_TYPE;
    }

    uint32_t flags = 0;
    if (registry.all_of<PlayerTag>(entity)) {
      flags |= GPU_ENTITY_FLAG_KINEMATIC | GPU_ENTITY_FLAG_NO_RENDER;
    }
    
    // Monsters should not rotate based on velocity (User Requirement)
    if (registry.any_of<EnemyTag>(entity)) {
      flags |= GPU_ENTITY_FLAG_NO_ROTATION;
    }

    if (auto *ai = registry.try_get<AIComponent>(entity)) {
      uint8_t stateVal = static_cast<uint8_t>(ai->aiType);
      flags |= GPUFlags::PackAIState(stateVal);
    }
    gpuEntity.flags = flags;
  }

  // Handle removed/dead slots by zeroing them out in shadow buffer (optional
  // but safer) This is actually done by MDIRenderer/Culling usually, but we
  // should ensure highWaterMark is accurate and unused slots don't have
  // artifacts.

  return highWaterMark;
}

// =================================================================================================
// GPUVisualSync Implementation
// =================================================================================================

void GPUVisualSync::Init(const Config &config) { m_config = config; }

void GPUVisualSync::Execute(
    entt::registry &registry,
    std::vector<NoMoreDay::components::GPUVisualStats> &visualBuffer,
    uint64_t frameCounter, float currentTime) {
  auto view = registry.view<GPUIndex, CombatStats>();
  auto &storage = registry.storage<ActiveEffectsComponent>();

  for (auto entity : view) {
    auto &gpuIdx = view.get<GPUIndex>(entity);
    int slot = gpuIdx.index;

    if (slot < 0 || slot >= (int)visualBuffer.size())
      continue;

    bool needsStatsSync = registry.any_of<StatsDirty>(entity) ||
                          (frameCounter % m_config.refreshInterval == 0);

    auto &visualStats = visualBuffer[slot];

    if (needsStatsSync) {
      // CombatStats is guaranteed by view
      auto &stats = view.get<CombatStats>(entity);
      // Corrected: NoMoreDay::AttributePipeline (not systems::)
      NoMoreDay::AttributePipeline::ToGPU(stats, visualStats);

      visualStats.activeStatusMask = 0;
      if (storage.contains(entity)) {
        const auto &effects = storage.get(entity).effects;
        for (const auto &effect : effects) {
          switch (effect.type) {
          case NoMoreDay::BuffType::Freeze:
            visualStats.activeStatusMask |=
                NoMoreDay::Constants::GPU::STATUS_FROZEN;
            break;
          case NoMoreDay::BuffType::Burn:
            visualStats.activeStatusMask |=
                NoMoreDay::Constants::GPU::STATUS_BURNING;
            break;
          case NoMoreDay::BuffType::Poison:
            visualStats.activeStatusMask |=
                NoMoreDay::Constants::GPU::STATUS_POISONED;
            break;
          case NoMoreDay::BuffType::Shock:
            visualStats.activeStatusMask |=
                NoMoreDay::Constants::GPU::STATUS_SHOCKED;
            break;
          default:
            break;
          }
        }
      }

      if (registry.any_of<StatsDirty>(entity)) {
        registry.remove<StatsDirty>(entity);
      }
    }
    visualStats.statusTimer = currentTime;
  }
}

// =================================================================================================
// GPUSlotManager Implementation
// =================================================================================================

void GPUSlotManager::Init(int maxEntities, entt::registry *registry,
                          SlotRecycleCallback onRecycle) {
  m_maxEntities = maxEntities;
  m_onRecycle = onRecycle;

  m_freeSlots.reserve(m_maxEntities);
  m_freeSlots.clear();
  for (int i = m_maxEntities - 1; i >= 0; --i) {
    m_freeSlots.push_back(i);
  }

  m_slotToEntity.assign(m_maxEntities, entt::null);

  if (registry) {
    registry->on_destroy<GPUIndex>()
        .connect<&GPUSlotManager::OnEntityDestroyed>(this);
    registry->on_destroy<Position>()
        .connect<&GPUSlotManager::OnEntityDestroyed>(this);
    registry->on_destroy<Radius>().connect<&GPUSlotManager::OnEntityDestroyed>(
        this);
  }
}

void GPUSlotManager::Process(entt::registry &registry) {
  // 1. Reclaim slots from dead entities
  // [OPTIMIZATION-AUDIT] Switch from O(N) traversal to Multi-component View O(N_dead + N_proj)
  /*
  auto deadView = registry.view<GPUIndex>();
  for (auto entity : deadView) {
    if (registry.any_of<KilledTag, NoMoreDay::Projectile>(entity)) {
      auto &gpuIdx = deadView.get<GPUIndex>(entity);
      if (gpuIdx.index != -1) {
        int slot = gpuIdx.index;
        if (slot >= 0 && slot < m_maxEntities) {
          m_freeSlots.push_back(slot);
          m_slotToEntity[slot] = entt::null;
          if (m_onRecycle)
            m_onRecycle(slot);
        }
        gpuIdx.index = -1;
      }
    }
  }
  */

  auto recycleSlot = [&](GPUIndex &gpuIdx) {
    if (gpuIdx.index != -1) {
      int slot = gpuIdx.index;
      if (slot >= 0 && slot < m_maxEntities) {
        m_freeSlots.push_back(slot);
        m_slotToEntity[slot] = entt::null;
        if (m_onRecycle)
          m_onRecycle(slot);
      }
      gpuIdx.index = -1;
    }
  };

  // Efficiently iterate ONLY dead entities
  auto killedView = registry.view<GPUIndex, KilledTag>();
  for (auto entity : killedView) {
    recycleSlot(killedView.get<GPUIndex>(entity));
  }

  // Efficiently iterate ONLY projectiles (which shouldn't have GPUIndex, but cleanup just in case)
  auto projView = registry.view<GPUIndex, NoMoreDay::Projectile>();
  for (auto entity : projView) {
    recycleSlot(projView.get<GPUIndex>(entity));
  }

  // 2. Assign slots to new entities
  auto activeView = registry.view<Position, Radius, GPUIndex>(
      entt::exclude<KilledTag, NoMoreDay::Projectile>);
  for (auto entity : activeView) {
    auto &gpuIdx = activeView.get<GPUIndex>(entity);

    if (gpuIdx.index == -1) {
      if (!m_freeSlots.empty()) {
        gpuIdx.index = m_freeSlots.back();
        m_freeSlots.pop_back();
        m_slotToEntity[gpuIdx.index] = entity;
      }
    }
  }
}

void GPUSlotManager::OnEntityDestroyed(entt::registry &registry,
                                       entt::entity entity) {
  auto *gpuIdx = registry.try_get<GPUIndex>(entity);
  if (gpuIdx && gpuIdx->index != -1) {
    int slot = gpuIdx->index;
    if (slot >= 0 && slot < m_maxEntities) {
      m_freeSlots.push_back(slot);
      m_slotToEntity[slot] = entt::null;
      if (m_onRecycle)
        m_onRecycle(slot);
    }
    gpuIdx->index = -1;
  }
}

} // namespace NoMoreDay::render
