#include "game/systems/world/LevelManager.hpp"
#include "core/logging/Logger.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/foundation/components/MapComponent.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/data/BiomeRegistry.hpp"
#include "game/foundation/data/MosaicData.hpp"
#include "game/foundation/data/BiomeTypes.hpp"
#include "game/foundation/components/WorldState.hpp"
#include "game/systems/world/WorldConstants.hpp"
#include <algorithm>


LevelManager::LevelManager() : m_currentLevel(1) {}

LevelManager::~LevelManager() {
  LOG_INFO("Shutting down LevelManager...");
  cleanup();
  LOG_INFO("LevelManager shutdown completed");
}

void LevelManager::initialize(ResourceManager &resources,
                              entt::registry &registry) {
  m_resources = &resources;
  m_registry = &registry;
  LOG_INFO("LevelManager initialized with ResourceManager and Registry");
}

void LevelManager::loadNewLevel(NoMoreDay::BiomeID biome, int width, int height,
                                 int level) {
  LOG_INFO("Loading new level synchronously: {} ({}x{}) Level: {}",
           static_cast<int>(biome), width, height, level);
  auto data = prepareLevel(biome, width, height, level);
  activateLevel(std::move(data));
}

NoMoreDay::BiomeID LevelManager::getCurrentBiomeID() const {
  return NoMoreDay::BiomeRegistry::Get().GetBiome(m_currentBiome).numericId;
}

LevelManager::LevelData LevelManager::prepareLevel(NoMoreDay::BiomeID biome,
                                                   int width, int height,
                                                   int level) {
  LOG_INFO("Preparing level data for biome: {} ({}x{})",
           static_cast<int>(biome), width, height);

  LevelData data;
  data.biome = biome;
  data.width = width;
  data.height = height;
  data.level = level;
  data.isMosaic = false;

  data.map = std::make_unique<MapSystem>();
  data.enemy = std::make_unique<EnemySpawnSystem>();
  data.fog = std::make_unique<FogOfWarSystem>();

  // 生成地图 (这里只是预生成数据结构，不涉及 GPU)
  std::string biomeKey = NoMoreDay::BiomeRegistry::Get().GetBiome(biome).id;
  data.map->generateMap(width, height, biomeKey);

  // 初始化敌人
  data.enemy->initData(width, height, level, *data.map, biome);

  return data;
}

void LevelManager::loadMosaicLevel(const NoMoreDay::MosaicGrid &grid,
                                   const NoMoreDay::ResonanceResult &resonance,
                                   entt::registry *registry, int width,
                                   int height) {
  LOG_INFO("Loading mosaic level ({}x{})...", width, height);
  // Note: Registry passed here is likely for reading components, but we also
  // use m_registry for spawning. Ideally they are the same.
  auto data = prepareMosaicLevel(grid, resonance, registry, width, height);
  activateLevel(std::move(data));
}

LevelManager::LevelData
LevelManager::prepareMosaicLevel(const NoMoreDay::MosaicGrid &grid,
                                 const NoMoreDay::ResonanceResult &resonance,
                                 entt::registry *registry, int width,
                                 int height) {
  const auto effectiveBiome = (resonance.primaryBiome == NoMoreDay::BiomeID::None)
                                  ? NoMoreDay::BiomeID::Cave
                                  : resonance.primaryBiome;

  LOG_INFO("Preparing mosaic level data for biome: {} ({}x{})",
           static_cast<int>(effectiveBiome), width,
           height);

  LevelData data;
  data.biome = effectiveBiome;
  data.width = width;
  data.height = height;
  data.level = m_currentLevel + 1; // Increment level depth
  data.isMosaic = true;
  data.resonance = resonance;

  data.map = std::make_unique<MapSystem>();
  data.enemy = std::make_unique<EnemySpawnSystem>();
  data.fog = std::make_unique<FogOfWarSystem>();

  // 生成地图
  data.map->generateMosaicMap(width, height, grid, resonance, registry);

  // 初始化敌人
  const NoMoreDay::ActiveDimensionalState* statePtr = nullptr;
  if(registry && registry->ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
      statePtr = &registry->ctx().get<NoMoreDay::ActiveDimensionalState>();
  } else {
      LOG_WARN("ActiveDimensionalState missing during Mosaic Load!");
  }
  data.enemy->initData(width, height, data.level, *data.map, effectiveBiome, statePtr);

  return data;
}

void LevelManager::activateLevel(LevelData &&data) {
  LOG_INFO("Activating level: {} ({}x{}) Level: {}", static_cast<int>(data.biome), data.width,
           data.height, data.level);

  cleanup(); // Clean old level (including GPU textures)

  m_mapSystem = std::move(data.map);
  m_enemySystem = std::move(data.enemy);
  m_fogSystem = std::move(data.fog);
  m_currentBiome = NoMoreDay::BiomeRegistry::Get().GetBiome(data.biome).id;
  m_currentLevel = data.level;
  m_currentResonance = data.resonance;
  m_isMosaicLevel = data.isMosaic;

  const auto &biomeConfig = NoMoreDay::BiomeRegistry::Get().GetBiome(data.biome);
  if (biomeConfig.hasFeature(NoMoreDay::BiomeFeature::AirWall) &&
      !biomeConfig.backgroundShader.empty()) {
    m_airWallRenderer = std::make_unique<NoMoreDay::AirWallRenderer>();
    m_airWallRenderer->Initialize(biomeConfig.backgroundShader);
  } else {
    m_airWallRenderer.reset();
  }

  // GPU Initialization (Must be on Main Thread)
  if (m_fogSystem && m_resources) {
    m_fogSystem->initialize(*m_resources, data.width, data.height);
  }
  if (m_enemySystem) {
    m_enemySystem->initTextures();
  }

  // Spawn Level Entities (Portals, etc.)
  spawnLevelEntities();

  LOG_INFO("Level activated successfully");
}

void LevelManager::spawnLevelEntities() {
  if (!m_registry || !m_mapSystem)
    return;

  // Spawn Stashes in Town
  if (getCurrentBiomeID() == NoMoreDay::BiomeID::Town) {
       int cx = m_mapSystem->getWidth() / 2;
       int cy = m_mapSystem->getHeight() / 2;
       float spawnCenterX = cx * NoMoreDay::Constants::World::GRID_TILE_SIZE + (NoMoreDay::Constants::World::GRID_TILE_SIZE * 0.5f);
       float spawnCenterY = cy * NoMoreDay::Constants::World::GRID_TILE_SIZE + (NoMoreDay::Constants::World::GRID_TILE_SIZE * 0.5f);

       auto pStash = m_registry->create();
       m_registry->emplace<Position>(pStash, spawnCenterX - 140.0f, spawnCenterY - 80.0f);
       m_registry->emplace<NoMoreDay::StashInteractableComponent>(pStash, NoMoreDay::StashType::Personal);
       m_registry->emplace<NoMoreDay::StashPlaceholderRender>(pStash);
       m_registry->emplace<LocalLevelTag>(pStash);

       auto sStash = m_registry->create();
       m_registry->emplace<Position>(sStash, spawnCenterX - 70.0f, spawnCenterY - 80.0f);
       m_registry->emplace<NoMoreDay::StashInteractableComponent>(sStash, NoMoreDay::StashType::Shared);
       m_registry->emplace<NoMoreDay::StashPlaceholderRender>(sStash);
       m_registry->emplace<LocalLevelTag>(sStash);
  }

  LOG_INFO("Scanning map for exit portals...");
  int w = m_mapSystem->getWidth();
  int h = m_mapSystem->getHeight();
  using namespace NoMoreDay::Constants::World;

  int count = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (m_mapSystem->getTileType(x, y) == Tile::Type::STAIRS_DOWN) {
        auto entity = m_registry->create();
        float cx = x * GRID_TILE_SIZE + (GRID_TILE_SIZE / 2.0f);
        float cy = y * GRID_TILE_SIZE + (GRID_TILE_SIZE / 2.0f);

        m_registry->emplace<Position>(entity, cx, cy);
        m_registry->emplace<LocalLevelTag>(entity);

        PortalComponent pc;
        pc.isActive = true;
        pc.radius = 25.0f;

        // Determine type based on context
        if (getCurrentBiomeID() == NoMoreDay::BiomeID::Town) {
            // Town Exit -> Dimensional Gate (Triggers Level Select)
            pc.type = PortalType::DimensionalGate;
            pc.radius = 40.0f; // Larger radius for town gate

            // Add Visual Sprite for Dimensional Gate in Town
            if (m_resources) {
                m_resources->loadTexture(assets::ui::textures::Env_Portal_Arcane.id, std::string(assets::ui::textures::Env_Portal_Arcane.path));
                auto& sprite = m_registry->emplace<SpriteComponent>(entity);
                sprite.texture = m_resources->getTexture(assets::ui::textures::Env_Portal_Arcane.id);
                sprite.scale = 0.8f; // Slightly larger
            }
        } else {
            // Dungeon Exit -> Next Level (Triggers Mosaic directly)
            pc.type = PortalType::NextLevel;
        }

        // Visuals are handled by PortalSystem based on type
        m_registry->emplace<PortalComponent>(entity, pc);
        LOG_INFO("Spawned NextLevel Portal at ({}, {})", cx, cy);
        count++;
      }
    }
  }

  if (count == 0) {
    LOG_WARN("No STAIRS_DOWN found in map! Player might be stuck.");
  }
}

void LevelManager::update(float dt, entt::registry &registry,
                          const Position &playerPos) {
  if (m_mapSystem && m_enemySystem && m_fogSystem) {
    // 战争迷雾更新由 GameplayState 根据当前相机屏幕区域调用
    // (视野 = 整个屏幕范围)

    // 更新敌人生成状态
    m_enemySystem->updateEnemySpawning(playerPos, registry, dt,
                                       m_mapSystem.get());
  }
}

void LevelManager::render(const Camera2D &camera) {
  if (m_mapSystem) {
    if (m_airWallRenderer) {
      const auto &mapData = m_mapSystem->getMapData();
      m_airWallRenderer->RenderBackground(camera, mapData.grid, mapData.width,
                                          mapData.height, static_cast<float>(GetTime()));
    }
    m_mapSystem->render(camera);
  }
}

void LevelManager::cleanup() {
  if (m_airWallRenderer) {
    m_airWallRenderer->Shutdown();
    m_airWallRenderer.reset();
  }
  m_mapSystem.reset();
  m_enemySystem.reset();
  m_fogSystem.reset();

  LOG_INFO("LevelManager cleaned up");
}

void LevelManager::generateLevel(NoMoreDay::BiomeID biome, int width, int height) {
  // private section? I didn't remove it from header private section yet. But I
  // can leave it empty or implementing via prepare/activate
  loadNewLevel(biome, width, height);
}

void LevelManager::setLevelParameters(const std::string &biome) {
  // Unused currently
}
