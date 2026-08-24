#pragma once
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUEntitySystem.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/data/BiomeRegistry.hpp"
#include "game/foundation/data/BiomeTypes.hpp"
#include "game/application/render/GPUEntitySync.hpp"
#include "game/systems/world/WorldConstants.hpp"
#include "game/systems/world/LevelManager.hpp"
#include <cstdint>

namespace NoMoreDay {

/**
 * @brief Game 层 GPU 实体渲染适配器。
 *
 * 负责 ECS -> shadow buffer 的投影（槽位分配、物理/视觉同步、雾视野），
 * 渲染资源、上传与绘制全部留在 Engine 的 GPUEntitySystem。
 * 边界 = Engine 侧 DTO shadow buffer，热路径零拷贝（直接写 Engine 的 buffer）。
 */
class GPUEntityAdapter {
public:
  void Init(int maxEntities, entt::registry *registry,
            systems::GPUEntitySystem &engine) {
    m_engine = &engine;
    m_slotManager.Init(maxEntities, registry, [this](int slot) {
      if (slot >= 0 && slot < (int)m_engine->ShadowBuffer().size()) {
        m_engine->ShadowBuffer()[slot].radius = 0.0f;
        m_engine->ShadowBuffer()[slot].position = {0, 0};
        m_engine->VisualStatsBuffer()[slot] = {};
      }
    });

    render::GPUPhysicsSync::Config physicsConfig;
    physicsConfig.maxEntities = maxEntities;
    m_physicsSync.Init(physicsConfig);

    render::GPUVisualSync::Config visualConfig;
    visualConfig.maxEntities = maxEntities;
    m_visualSync.Init(visualConfig);

    engine.BeginShadowWrite();
  }

  void SetLevelManager(LevelManager *levelManager) {
    m_levelManager = levelManager;
  }

  void Update(entt::registry &registry, systems::GPUEntitySystem &engine,
              float dt, float currentTime) {
    m_frameCounter++;
    engine.BeginShadowWrite();

    // Phase 1: Slot Reclamation & Assignment via GPUSlotManager
    m_slotManager.Process(registry);

    // Phase 2: Physics Sync (zero-copy projection into Engine shadow buffer)
    int highWaterMark =
        m_physicsSync.Execute(registry, engine.ShadowBuffer(), m_frameCounter);
    engine.SetHighWaterMark(highWaterMark);

    // Phase 3: Visual Sync
    engine.SetUpdatedStatsIndices(m_visualSync.Execute(
        registry, engine.VisualStatsBuffer(), m_frameCounter, currentTime));

    if (m_levelManager) {
      ApplyFogVision(registry, m_levelManager, engine);
    }
  }

  void ApplyFogVision(entt::registry &registry, LevelManager *levelManager,
                      systems::GPUEntitySystem &engine) {
    if (!levelManager) {
      return;
    }
    const auto biomeId = levelManager->getCurrentBiomeID();
    const auto &biome = NoMoreDay::BiomeRegistry::Get().GetBiome(biomeId);
    if (biome.isSafeZone) {
      return;
    }
    auto enemyView = registry.view<EnemyTag, Position, GPUIndex>();
    auto &shadow = engine.ShadowBuffer();
    for (auto entity : enemyView) {
      const auto &pos = enemyView.get<Position>(entity);
      const auto &gpuIdx = enemyView.get<GPUIndex>(entity);
      const int slot = gpuIdx.index;
      if (slot < 0 || slot >= (int)shadow.size()) {
        continue;
      }
      using namespace NoMoreDay::Constants::World;
      const int gx = static_cast<int>(pos.x / GRID_TILE_SIZE);
      const int gy = static_cast<int>(pos.y / GRID_TILE_SIZE);
      const bool visible = levelManager->getFogSystem().isVisible(gx, gy);
      engine.ApplyShadowFlags(
          slot, visible ? 0u : NoMoreDay::components::GPU_ENTITY_FLAG_NO_RENDER);
    }
  }

private:
  int m_maxEntities = 0;
  uint64_t m_frameCounter = 0;
  render::GPUSlotManager m_slotManager;
  render::GPUPhysicsSync m_physicsSync;
  render::GPUVisualSync m_visualSync;
  systems::GPUEntitySystem *m_engine = nullptr;
  LevelManager *m_levelManager = nullptr;
};

} // namespace NoMoreDay
