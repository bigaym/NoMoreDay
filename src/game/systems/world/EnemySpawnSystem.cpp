#include "game/systems/world/EnemySpawnSystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/UUID.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Combat.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/MapFragmentComponent.hpp"
#include "game/components/PlayerState.hpp" // For stats if needed
#include "game/data/BiomeRegistry.hpp"
#include "game/data/MosaicData.hpp"
#include <algorithm>
#include <cmath>
#include <random>


EnemySpawnSystem::EnemySpawnSystem()
    : m_mapWidth(0), m_mapHeight(0), m_gen(std::random_device{}()) {
  // 扩大生成和销毁范围
  m_activationDistance = 1200.0f;
  m_deactivationDistance = 2000.0f;
}

EnemySpawnSystem::~EnemySpawnSystem() {
  for (auto &[type, texture] : m_raceTextures) {
    UnloadTexture(texture);
  }
  m_raceTextures.clear();
}

void EnemySpawnSystem::initializeLevel(int width, int height,
                                       const MapSystem &mapSystem,
                                       const std::string &biome) {
  initData(width, height, mapSystem, biome);
  initTextures();
}

// Async Loading Support
void EnemySpawnSystem::initData(int width, int height,
                                const MapSystem &mapSystem,
                                const std::string &biomeId,
                                const NoMoreDay::ResonanceResult *resonance) {
  m_mapWidth = width;
  m_mapHeight = height;
  m_spawnData.clear();
  m_pendingRaces.clear();

  // Reset resonance mods
  m_resonanceMods = {1.0f, 0, 0.0f, 0};

  if (resonance) {
    m_resonanceMods.densityMultiplier = resonance->totalEnemyDensity;
    m_resonanceMods.levelBonus = resonance->totalLevelMod;
    m_resonanceMods.dropRateBonus = resonance->totalDropRate;
    m_resonanceMods.dominantElement =
        static_cast<int>(resonance->dominantElement);

    LOG_INFO("EnemySpawnSystem: Applying Resonance - Density: {:.2f}, Level: "
             "{}, Drop: {:.2f}, Element: {}",
             m_resonanceMods.densityMultiplier, m_resonanceMods.levelBonus,
             m_resonanceMods.dropRateBonus, m_resonanceMods.dominantElement);
  }

  LOG_INFO("EnemySpawnSystem: Initializing level data for biome '{}'", biomeId);

  const auto &biomeConfig = NoMoreDay::BiomeRegistry::Get().GetBiome(biomeId);
  if (biomeConfig.isSafeZone) {
    LOG_INFO("Safe zone detected, no enemies will be spawned.");
    return;
  }

  // 1. 种族池逻辑
  std::vector<int> availableRaces;
  if (biomeConfig.enemyPool.empty()) {
    // Default pool if none specified
    availableRaces = {EnemyRace::UNDEAD, EnemyRace::DEMON};
  } else {
    for (const auto &raceName : biomeConfig.enemyPool) {
      if (raceName == "undead" || raceName == "skeleton")
        availableRaces.push_back(EnemyRace::UNDEAD);
      else if (raceName == "demon")
        availableRaces.push_back(EnemyRace::DEMON);
      else if (raceName == "corrupted")
        availableRaces.push_back(EnemyRace::CORRUPTED);
      else if (raceName == "cultist")
        availableRaces.push_back(EnemyRace::CULTIST);
      else if (raceName == "goblin")
        availableRaces.push_back(EnemyRace::GOBLIN);
      else if (raceName == "slime")
        availableRaces.push_back(EnemyRace::SLIME);
    }
  }

  m_pendingRaces = availableRaces; // Store for texture loading

  // 2. 群聚生成 - 大幅增加密度 (5~10倍)
  int baseClusterCount = (width * height) / 200; // 原来是 1000, 现在是 5倍密度
  int clusterCount =
      static_cast<int>(baseClusterCount * m_resonanceMods.densityMultiplier);

  // Ensure strict min/max
  if (biomeConfig.maxEnemies > 0) {
    // 允许生成更多，限制在 maxEnemies 范围内
    clusterCount = std::min(clusterCount, biomeConfig.maxEnemies / 5);
  }
  if (clusterCount < 1)
    clusterCount = 1;

  std::uniform_int_distribution<int> xDist(2, width - 3);
  std::uniform_int_distribution<int> yDist(2, height - 3);
  std::uniform_int_distribution<int> countDist(5,
                                               12); // 每群 5-12 只 (原来 3-6)
  for (int i = 0; i < clusterCount; ++i) {
    int cx, cy;
    bool foundCenter = false;
    for (int attempt = 0; attempt < 20; ++attempt) {
      cx = xDist(m_gen);
      cy = yDist(m_gen);
      if (mapSystem.getTileType(cx, cy) == Tile::Type::FLOOR) {
        foundCenter = true;
        break;
      }
    }

    if (!foundCenter)
      continue;

    int race = availableRaces[i % availableRaces.size()];
    int enemyCount = static_cast<int>(
        countDist(m_gen) *
        std::min(1.5f,
                 m_resonanceMods.densityMultiplier)); // 也会稍微增加单群数量

    for (int j = 0; j < enemyCount; ++j) {
      std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
      std::uniform_real_distribution<float> rDist(0.0f, 1.0f);

      float angle = angleDist(m_gen);
      float r = 2.0f + rDist(m_gen) * 4.0f;

      int ex = cx + static_cast<int>(cos(angle) * r);
      int ey = cy + static_cast<int>(sin(angle) * r);

      if (ex >= 0 && ex < width && ey >= 0 && ey < height &&
          mapSystem.getTileType(ex, ey) == Tile::Type::FLOOR) {

        EnemySpawnData data;
        data.position = {ex * 10.0f + 5.0f, ey * 10.0f + 5.0f};
        data.isAlive = false;
        data.entityId = entt::null;
        data.enemyType = race;
        data.allowRespawn = false;

        m_spawnData.push_back(data);
      }
    }
  }

  LOG_INFO(
      "Initialized {} enemy spawn points in {} clusters (Density Mod: {:.2f})",
      m_spawnData.size(), clusterCount, m_resonanceMods.densityMultiplier);
}

void EnemySpawnSystem::initTextures() {
  // Cleanup old
  for (auto &[type, texture] : m_raceTextures) {
    UnloadTexture(texture);
  }
  m_raceTextures.clear();

  // Load new
  for (int raceType : m_pendingRaces) {
    EnemyRace raceDef(static_cast<EnemyRace::Type>(raceType));
    Texture2D tex = LoadTexture(raceDef.texturePath.c_str());
    if (tex.id == 0) {
      LOG_ERROR("EnemySpawnSystem: Failed to load texture for race {} at '{}'",
                raceType, raceDef.texturePath);
    } else {
      m_raceTextures[raceType] = tex;
    }
  }
  m_pendingRaces.clear();
}

void EnemySpawnSystem::updateEnemySpawning(const Position &playerPos,
                                           entt::registry &registry) {
  for (auto &data : m_spawnData) {
    float dx = data.position.x - playerPos.x;
    float dy = data.position.y - playerPos.y;
    float distSq = dx * dx + dy * dy;

    if (!data.isAlive) {
      if (data.enemyType == -1)
        continue;

      if (distSq < m_activationDistance * m_activationDistance) {
        spawnEnemy(registry, data);
      }
    } else {
      if (!registry.valid(data.entityId)) {
        data.isAlive = false;
        data.entityId = entt::null;

        if (!data.allowRespawn) {
          data.enemyType = -1;
        }
        continue;
      }

      if (distSq > m_deactivationDistance * m_deactivationDistance) {
        despawnEnemy(registry, data);
      }
    }
  }
}

void EnemySpawnSystem::spawnEnemy(entt::registry &registry,
                                  EnemySpawnData &data) {
  auto entity = registry.create();

  registry.emplace<Position>(entity, data.position.x, data.position.y);
  registry.emplace<Velocity>(entity, 0.0f, 0.0f);
  registry.emplace<Radius>(entity, 5.0f); // Default collision radius
  registry.emplace<GPUIndex>(entity, -1); // Will be assigned by GPUEntitySystem
  registry.emplace<IDComponent>(entity, NoMoreDay::Utils::UUID::generate());
  registry.emplace<LocalLevelTag>(entity);

  if (m_raceTextures.count(data.enemyType)) {
    registry.emplace<TextureIDComponent>(entity,
                                         m_raceTextures[data.enemyType].id);
  }

  EnemyRace::Type raceType = static_cast<EnemyRace::Type>(data.enemyType);
  EnemyArchetype::Type archType = EnemyArchetype::FODDER;

  // Determine archetype based on race/logic (simplified for now)
  if (raceType == EnemyRace::DEMON)
    archType = EnemyArchetype::TANK;
  else if (raceType == EnemyRace::CORRUPTED)
    archType = EnemyArchetype::ASSASSIN;
  else if (raceType == EnemyRace::CULTIST)
    archType = EnemyArchetype::RANGER;
  else if (raceType == EnemyRace::ELEMENTAL)
    archType = EnemyArchetype::SUPPORT;

  // Emplace EnemyStateComponent EARLY so StatsSystem can use it
  registry.emplace<EnemyStateComponent>(entity, raceType, archType);
  registry.emplace<EnemyTag>(entity);
  auto &cStats = registry.emplace<NoMoreDay::CombatStats>(entity);
  registry.emplace<NoMoreDay::StatsDirty>(
      entity); // Trigger initial calculation
  auto &aState = registry.emplace<NoMoreDay::AttackState>(entity);

  EnemyRace raceDef(raceType);

  // Apply Level Mod to Base Stats
  float levelMultiplier =
      1.0f + (m_resonanceMods.levelBonus * 0.1f); // 每个等级增加 10% 基础属性

  cStats.min_weapon_damage = raceDef.baseDamage * 0.8f * levelMultiplier;
  cStats.max_weapon_damage = raceDef.baseDamage * 1.2f * levelMultiplier;
  cStats.armor = raceDef.baseArmor * levelMultiplier;
  cStats.accuracy = 1.0f;           // Standard accuracy
  cStats.attack_speed = 1.0f;       // Standard attack speed
  aState.baseAttackInterval = 1.5f; // Default attack interval

  float modifiedHP = raceDef.baseHP * levelMultiplier;

  switch (raceType) {
  case EnemyRace::UNDEAD:
    registry.emplace<HealthComponent>(entity, modifiedHP, modifiedHP);
    registry.emplace<AIComponent>(entity, AIType::IDLE, 150.0f, 40.0f, 50.0f);
    registry.emplace<ColorComponent>(entity, WHITE);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.25f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    break;
  case EnemyRace::DEMON:
    registry.emplace<HealthComponent>(entity, modifiedHP, modifiedHP);
    registry.emplace<AIComponent>(entity, AIType::IDLE, 200.0f, 50.0f, 70.0f);
    registry.emplace<ColorComponent>(entity, WHITE);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.45f * (1.0f + m_resonanceMods.dropRateBonus), 1, 2);
    aState.baseAttackInterval = 2.0f; // Slow but heavy
    cStats.attack_speed = 0.8f;
    break;
  case EnemyRace::CORRUPTED:
    registry.emplace<HealthComponent>(entity, modifiedHP, modifiedHP);
    registry.emplace<AIComponent>(entity, AIType::IDLE, 250.0f, 60.0f,
                                  100.0f);
    registry.emplace<ColorComponent>(entity, WHITE);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.30f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    aState.baseAttackInterval = 1.0f; // Fast
    cStats.attack_speed = 1.2f;
    break;
  case EnemyRace::CULTIST:
    registry.emplace<HealthComponent>(entity, modifiedHP, modifiedHP);
    registry.emplace<AIComponent>(entity, AIType::IDLE, 180.0f, 30.0f, 60.0f);
    registry.emplace<ColorComponent>(entity, WHITE);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.35f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    aState.baseAttackInterval = 1.8f;
    break;
  case EnemyRace::GOBLIN:
    registry.emplace<HealthComponent>(entity, modifiedHP, modifiedHP);
    registry.emplace<AIComponent>(entity, AIType::IDLE, 120.0f, 40.0f, 60.0f);
    registry.emplace<ColorComponent>(entity, GREEN);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.20f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    aState.baseAttackInterval = 1.2f;
    break;
  case EnemyRace::SLIME:
    registry.emplace<HealthComponent>(entity, modifiedHP, modifiedHP);
    registry.emplace<AIComponent>(entity, AIType::IDLE, 80.0f, 20.0f, 30.0f);
    registry.emplace<ColorComponent>(entity, LIME);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.15f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    aState.baseAttackInterval = 2.5f;
    break;
  default:
    registry.emplace<HealthComponent>(entity, modifiedHP, modifiedHP);
    registry.emplace<ColorComponent>(entity, WHITE);
    break;
  }

  // Apply Element Color Tint
  if (m_resonanceMods.dominantElement != 0) {
    auto *colorComp = registry.try_get<ColorComponent>(entity);
    if (colorComp) {
      switch (static_cast<NoMoreDay::FragmentElement>(
          m_resonanceMods.dominantElement)) {
      case NoMoreDay::FragmentElement::Fire:
        colorComp->color = RED;
        break;
      case NoMoreDay::FragmentElement::Cold:
        colorComp->color = SKYBLUE;
        break;
      case NoMoreDay::FragmentElement::Lightning:
        colorComp->color = YELLOW;
        break;
      case NoMoreDay::FragmentElement::Shadow:
        colorComp->color = PURPLE;
        break;
      case NoMoreDay::FragmentElement::Chaos:
        colorComp->color = DARKGRAY;
        break;
      default:
        break; // No tint for other elements or if 0
      }
    }
  }

  if (m_raceTextures.count(data.enemyType)) {
    registry.emplace<SpriteComponent>(entity, m_raceTextures[data.enemyType],
                                      0.2f);
  }

  if (registry.all_of<AIComponent>(entity)) {
    auto &ai = registry.get<AIComponent>(entity);
    ai.patrolStart = data.position;

    std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
    std::uniform_real_distribution<float> distDist(30.0f, 60.0f);
    float angle = angleDist(m_gen);
    float dist = distDist(m_gen);

    ai.patrolEnd = {data.position.x + std::cos(angle) * dist,
                    data.position.y + std::sin(angle) * dist};

    // === Set AIType based on archetype ===
    switch (archType) {
    case EnemyArchetype::TANK:
      ai.aiType = AIType::TANK_BLOCK;
      break;
    case EnemyArchetype::ASSASSIN:
      ai.aiType = AIType::ASSASSIN_STEALTH;
      break;
    case EnemyArchetype::SUPPORT:
      ai.aiType = AIType::SUPPORT_FLEE_BUFF;
      break;
    default:
      // FODDER and RANGER use default PATROL
      break;
    }

    // === Random elite modifier assignment (10% chance) ===
    std::uniform_real_distribution<float> eliteChance(0.0f, 1.0f);
    if (eliteChance(m_gen) < 0.10f) {
      // Assign either Link or Avenger modifier
      std::uniform_int_distribution<int> modifierType(0, 1);
      if (modifierType(m_gen) == 0) {
        registry.emplace<NoMoreDay::SoulLinkComponent>(entity);
        registry.emplace<NoMoreDay::SoulLinkTag>(entity);
        LOG_DEBUG("Elite enemy {} spawned with SoulLink modifier",
                  static_cast<uint32_t>(entity));
      } else {
        registry.emplace<NoMoreDay::AvengerComponent>(entity);
        registry.emplace<NoMoreDay::AvengerTag>(entity);
        LOG_DEBUG("Elite enemy {} spawned with Avenger modifier",
                  static_cast<uint32_t>(entity));
      }

      // Mark as elite
      auto &stateComp = registry.get<EnemyStateComponent>(entity);
      // Increase stats for elites
      auto *health = registry.try_get<HealthComponent>(entity);
      if (health) {
        health->max *= 1.5f;
        health->current = health->max;
      }
    }

    // Add ActiveEffectsComponent for buff support
    registry.emplace<NoMoreDay::ActiveEffectsComponent>(entity);
  }

  data.entityId = entity;
  data.isAlive = true;
}

void EnemySpawnSystem::despawnEnemy(entt::registry &registry,
                                    EnemySpawnData &data) {
  // If entity is Dormant, do NOT destroy it here, let it persist in pool
  if (registry.valid(data.entityId) && registry.any_of<DormantTag>(data.entityId)) {
      return; 
  }
  
  if (registry.valid(data.entityId)) {
    registry.destroy(data.entityId);
  }
  data.entityId = entt::null;
  data.isAlive = false;
}

void EnemySpawnSystem::updateDormantEntities(entt::registry& registry, const Position& playerPos, int gridW, int gridH) {
    static int frameCounter = 0;
    if (++frameCounter < 60) return; // Spec 2.3: Re-schedule every 60 frames
    frameCounter = 0;

    auto dormantView = registry.view<DormantTag, Position, AIComponent>();
    int awakenedCount = 0;
    int maxAwakenPerCycle = 50; // Throttle awakening

    std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
    std::uniform_real_distribution<float> distDist(1650.0f, 1800.0f); // Just inside active boundary (1950)

    for (auto entity : dormantView) {
        if (awakenedCount >= maxAwakenPerCycle) break;

        // Find a valid spot
        float angle = angleDist(m_gen);
        float dist = distDist(m_gen);
        float tx = playerPos.x + std::cos(angle) * dist;
        float ty = playerPos.y + std::sin(angle) * dist;

        // Check bounds (approximate)
        if (tx < 0 || tx > gridW * 10.0f || ty < 0 || ty > gridH * 10.0f) continue;
        
        // Wake up
        auto& pos = dormantView.get<Position>(entity);
        pos.x = tx;
        pos.y = ty;

        registry.remove<DormantTag>(entity);
        registry.emplace_or_replace<Velocity>(entity, 0.0f, 0.0f); // Re-add Velocity
        
        // Reset AI
        auto& ai = dormantView.get<AIComponent>(entity);
        ai.aiType = AIType::IDLE; // Spec 3.0: Reset to IDLE, wait for WakeUp
        ai.target = entt::null;

        awakenedCount++;
    }
    
    if (awakenedCount > 0) {
        LOG_INFO("Recycled {} dormant entities", awakenedCount);
    }
}
