#include "engine/scene/SceneManager.hpp"
#include "core/logging/Logger.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "raylib.h"


namespace NoMoreDay {

SceneManager::SceneManager(LevelManager &levelManager, entt::registry &registry)
    : m_levelManager(levelManager), m_registry(registry) {}

SceneManager::~SceneManager() {
  // 确保异步加载任务在销毁前完成，避免悬垂指针
  if (m_loadingFuture.valid()) {
    LOG_INFO("SceneManager: Waiting for async loading to complete before "
             "destruction...");
    m_loadingFuture.wait();
  }
}

void SceneManager::RequestTransition(BiomeID biome, int level,
                                     const std::string &entranceId) {
  if (m_isTransitioning)
    return;

  m_targetBiome = biome;
  m_targetBiomeKey = BiomeRegistry::Get().GetBiome(biome).id;
  m_targetLevel = level;
  m_targetEntranceId = entranceId;

  m_isTransitioning = true;
  m_state = State::FADE_OUT;
  m_fadeAlpha = 0.0f;

  LOG_INFO("Transition requested to Biome: {} ({}), Level: {}",
           m_targetBiomeKey, (int)biome, level);
}

void SceneManager::RequestTransition(const std::string &biome, int level,
                                     const std::string &entranceId) {
  if (m_isTransitioning)
    return;

  m_targetBiomeKey = biome;
  m_targetBiome = BiomeRegistry::Get().GetBiome(biome).numericId;
  m_targetLevel = level;
  m_targetEntranceId = entranceId;

  m_isTransitioning = true;
  m_state = State::FADE_OUT;
  m_fadeAlpha = 0.0f;

  LOG_INFO("Transition requested to Biome: {}, Level: {}", biome, level);
}

void SceneManager::RequestMosaicTransition(
    const NoMoreDay::MosaicGrid &grid,
    const NoMoreDay::ResonanceResult &resonance) {
  if (m_isTransitioning)
    return;

  m_pendingMosaicGrid = grid;
  m_pendingResonance = resonance;
  m_isMosaicTransition = true;

  m_targetBiome = (resonance.primaryBiome == NoMoreDay::BiomeID::None)
                      ? NoMoreDay::BiomeID::Cave
                      : resonance.primaryBiome;
  m_targetBiomeKey = BiomeRegistry::Get().GetBiome(m_targetBiome).id;
  m_targetLevel = 1; // Rifts always start at level 1 contextually

  m_isTransitioning = true;
  m_state = State::FADE_OUT;
  m_fadeAlpha = 0.0f;

  LOG_INFO("Mosaic transition requested to Biome: {}", m_targetBiomeKey);
}

void SceneManager::SetOriginInfo(BiomeID biome, int level, float x, float y) {
  m_originBiome = biome;
  m_originBiomeKey = BiomeRegistry::Get().GetBiome(biome).id;
  m_originLevel = level;
  m_originX = x;
  m_originY = y;
  LOG_DEBUG("Origin set: {} L{} at ({:.1f}, {:.1f})", m_originBiomeKey, level,
            x, y);
}

void SceneManager::SetOriginInfo(const std::string &biome, int level, float x,
                                 float y) {
  m_originBiomeKey = biome;
  m_originBiome = BiomeRegistry::Get().GetBiome(biome).numericId;
  m_originLevel = level;
  m_originX = x;
  m_originY = y;
  LOG_DEBUG("Origin set: {} L{} at ({:.1f}, {:.1f})", biome, level, x, y);
}

void SceneManager::Update(float dt) {
  if (!m_isTransitioning)
    return;

  const float fadeSpeed = 2.0f; // 0.5s fade

  switch (m_state) {
  case State::FADE_OUT:
    m_fadeAlpha += dt * fadeSpeed;
    if (m_fadeAlpha >= 1.0f) {
      m_fadeAlpha = 1.0f;
      m_state = State::LOADING;
    }
    break;

  case State::LOADING:
    // 1. Clear Local Entities
    {
      auto view = m_registry.view<LocalLevelTag>();
      // Note: destroy invalidates iterators, so we can't iterate simply if we
      // want to be safe, but entt::registry::destroy(first, last) handles it
      // efficiently.
      m_registry.destroy(view.begin(), view.end());
      LOG_INFO("Cleared local entities");
    }

    // 2. Start Async Load
    if (m_isMosaicTransition) {
      m_loadingFuture = std::async(std::launch::async, [this]() {
        using namespace NoMoreDay::Constants::World;
        // Use standard world size for mosaic maps to provide enough space
        // (approx 500x500 tiles)
        return m_levelManager.prepareMosaicLevel(
            m_pendingMosaicGrid, m_pendingResonance, &m_registry,
            WORLD_WIDTH / 10, WORLD_HEIGHT / 10);
      });
    } else {
      m_loadingFuture = std::async(std::launch::async, [this]() {
        using namespace NoMoreDay::Constants::World;
        return m_levelManager.prepareLevel(m_targetBiome, WORLD_WIDTH / 10,
                                           WORLD_HEIGHT / 10, m_targetLevel);
      });
    }

    m_state = State::WAIT_FOR_FUTURE;
    break;

  case State::WAIT_FOR_FUTURE:
    if (m_loadingFuture.valid() &&
        m_loadingFuture.wait_for(std::chrono::seconds(0)) ==
            std::future_status::ready) {
      ApplyLoadedLevel();
      m_isMosaicTransition = false; // Reset the mosaic flag after applying
      m_state = State::FADE_IN;
    }
    break;

  case State::FADE_IN:
    m_fadeAlpha -= dt * fadeSpeed;
    if (m_fadeAlpha <= 0.0f) {
      m_fadeAlpha = 0.0f;
      m_isTransitioning = false;
      m_state = State::IDLE;
      LOG_INFO("Transition completed");
    }
    break;

  default:
    break;
  }
}

void SceneManager::ApplyLoadedLevel() {
  auto levelData = m_loadingFuture.get();
  m_levelManager.activateLevel(std::move(levelData));

  // Update current scene info
  m_currentBiome = m_targetBiome;
  m_currentBiomeKey = m_targetBiomeKey;
  m_currentLevel = m_targetLevel;

  // Reset kills if moving to a new combat level (not town)
  if (m_targetBiome != NoMoreDay::BiomeID::Town) {
    if (m_isMosaicTransition || m_targetBiome != m_lastCombatBiome ||
        m_targetLevel != m_lastCombatLevel) {
      // Moving to a NEW dungeon level or into a Rift
      auto playerView = m_registry.view<PlayerTag, PlayerStats>();
      for (auto entity : playerView) {
        m_registry.get<PlayerStats>(entity).current_map_kills = 0;
      }

      // Update last combat info
      m_lastCombatBiome = m_targetBiome;
      m_lastCombatBiomeKey = m_targetBiomeKey;
      m_lastCombatLevel = m_targetLevel;
    }
  }

  // Spawn Portals based on Map
  const auto &mapSystem = m_levelManager.getMapSystem();
  int w = mapSystem.getWidth();
  int h = mapSystem.getHeight();

  using namespace NoMoreDay::Constants::World;
  float spawnX = (float)WORLD_WIDTH / 2.0f;
  float spawnY = (float)WORLD_HEIGHT / 2.0f;
  bool spawnFound = false;

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      Tile::Type type = mapSystem.getTileType(x, y);
      if (type == Tile::Type::STAIRS_DOWN) {
        auto portal = m_registry.create();
        m_registry.emplace<LocalLevelTag>(portal);
        m_registry.emplace<Position>(portal, x * 10.0f + 5.0f,
                                     y * 10.0f + 5.0f);

        PortalComponent pc;
        if (m_targetBiome == NoMoreDay::BiomeID::Town) {
          // Town exit: direct transition to dungeon
          pc.type = PortalType::Dungeon;
          pc.targetBiome = NoMoreDay::BiomeID::Cave;
          pc.targetLevel = 1;
        } else {
          // Dungeon exit: trigger Mosaic Editor for next level
          pc.type = PortalType::NextLevel;
          pc.targetLevel = m_targetLevel + 1;
        }
        pc.radius = 35.0f;
        m_registry.emplace<PortalComponent>(portal, pc);
      } else if (type == Tile::Type::STAIRS_UP && !spawnFound) {
        spawnX = x * 10.0f + 5.0f;
        spawnY = y * 10.0f + 5.0f;
        spawnFound = true;
      }
    }
  }

  // Reposition Persistent Entities (Player)
  if (!spawnFound) {
    // Attempt to find a floor tile if no STAIRS_UP found
    int centerX = (int)(spawnX / 10.0f);
    int centerY = (int)(spawnY / 10.0f);

    bool found = false;
    for (int r = 0; r < 50 && !found; r++) {
      for (int dx = -r; dx <= r; dx++) {
        for (int dy = -r; dy <= r; dy++) {
          if (mapSystem.isWalkable(centerX + dx, centerY + dy)) {
            spawnX = (centerX + dx) * 10.0f + 5.0f;
            spawnY = (centerY + dy) * 10.0f + 5.0f;
            found = true;
            break;
          }
        }
        if (found)
          break;
      }
    }
  }

  auto view = m_registry.view<PersistentTag, Position>();
  for (auto entity : view) {
    auto &pos = view.get<Position>(entity);
    pos.x = spawnX;
    pos.y = spawnY;

    // Stop movement
    if (auto *vel = m_registry.try_get<Velocity>(entity)) {
      vel->vx = 0.0f;
      vel->vy = 0.0f;
    }

    // Notify systems that player teleported (optional, e.g. camera snap)
  }

  // Spawn return portal if coming from a dungeon to town
  if (m_targetBiome == NoMoreDay::BiomeID::Town &&
      m_originBiome != NoMoreDay::BiomeID::None &&
      m_originBiome != NoMoreDay::BiomeID::Town) {
    auto returnPortal = m_registry.create();
    m_registry.emplace<LocalLevelTag>(returnPortal);
    m_registry.emplace<Position>(returnPortal, spawnX + 50.0f, spawnY);

    PortalComponent pc;
    pc.type = PortalType::Return;
    pc.targetBiome = m_originBiome;
    pc.targetLevel = m_originLevel;
    pc.originX = m_originX;
    pc.originY = m_originY;
    pc.isActive = true;
    pc.radius = 35.0f;
    m_registry.emplace<PortalComponent>(returnPortal, pc);

    LOG_INFO("Return portal spawned to {} L{}", m_originBiomeKey,
             m_originLevel);
  }
}

void SceneManager::RenderOverlay() {
  if (m_isTransitioning && m_fadeAlpha > 0.0f) {
    Color c = BLACK;
    c.a = (unsigned char)(m_fadeAlpha * 255.0f);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), c);

    if (m_state == State::WAIT_FOR_FUTURE || m_state == State::LOADING) {
      const char *text = "LOADING...";
      int w = MeasureText(text, 20);
      DrawText(text, GetScreenWidth() / 2 - w / 2, GetScreenHeight() / 2, 20,
               WHITE);
    }
  }
}

} // namespace NoMoreDay
