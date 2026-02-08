#include "game/systems/world/EnemySpawnSystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/UUID.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Common.hpp"
#include "game/components/EliteModifierComponents.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/FactionComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/MapFragmentComponent.hpp"
#include "game/components/PlayerState.hpp" // For stats if needed
#include "game/data/BiomeRegistry.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/data/MosaicData.hpp"
#include "game/components/WorldState.hpp"
#include "game/utils/MonsterScaling.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>
#include <unordered_map>

namespace {
int RaceFromName(std::string raceName) {
  std::transform(raceName.begin(), raceName.end(), raceName.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  static const std::unordered_map<std::string, int> kRaceMap = {
      {"undead", EnemyRace::UNDEAD},   {"skeleton", EnemyRace::UNDEAD},
      {"demon", EnemyRace::DEMON},     {"corrupted", EnemyRace::CORRUPTED},
      {"warcraft", EnemyRace::CORRUPTED},
      {"cultist", EnemyRace::CULTIST}, {"elf", EnemyRace::ElVES},
      {"elves", EnemyRace::ElVES},     {"beast", EnemyRace::BEAST},
      {"animal", EnemyRace::BEAST},    {"goblin", EnemyRace::GOBLIN},
      {"machine", EnemyRace::MACHINE}, {"mech", EnemyRace::MACHINE},
      {"elemental", EnemyRace::ELEMENTAL}};
  auto it = kRaceMap.find(raceName);
  if (it != kRaceMap.end()) {
    return it->second;
  }
  return EnemyRace::UNDEAD;
}
} // namespace

EnemySpawnSystem::EnemySpawnSystem()
    : m_mapWidth(0), m_mapHeight(0), m_gen(std::random_device{}()) {
  // 扩大生成和销毁范围
  using namespace NoMoreDay::Constants::Enemy;
  m_activationDistance = DEFAULT_ACTIVATION_DISTANCE;
  m_deactivationDistance = DEFAULT_DEACTIVATION_DISTANCE;
}

EnemySpawnSystem::~EnemySpawnSystem() {
}

void EnemySpawnSystem::initializeLevel(int width, int height, int level,
                                        const MapSystem &mapSystem,
                                        NoMoreDay::BiomeID biome) {
  initData(width, height, level, mapSystem, biome);
}

// Async Loading Support
void EnemySpawnSystem::initData(int width, int height, int level,
                                const MapSystem &mapSystem,
                                NoMoreDay::BiomeID biomeId,
                                const NoMoreDay::ActiveDimensionalState *state) {  m_mapWidth = width;
  m_mapHeight = height;
  m_areaLevel = level;
  m_spawnData.clear();

  // Reset modifiers
  m_resonanceMods = {1.0f, 0, 0.0f, 0, 1.0f, 1.0f, 1.0f};

  if (state) {
    // 1. Base Level Selection (User Choice + Depth Scaling)
    // Depth scaling: +1 Level per depth layer (Depth 1 = Base, Depth 2 = Base+1, etc.)
    m_areaLevel = std::max(1, state->selectedBaseLevel + (state->currentDepth - 1));
    
    // 2. Apply Resonance Modifiers
    const auto& resonance = state->resonance;
    m_resonanceMods.densityMultiplier = resonance.totalEnemyDensity;
    // Keep levelBonus separate from base level (it's from affixes/fragments)
    m_resonanceMods.levelBonus = resonance.totalLevelMod; 
    m_resonanceMods.dropRateBonus = resonance.totalDropRate;
    m_resonanceMods.dominantElement = static_cast<int>(resonance.dominantElement);
    
    // Apply Explicit Affixes (Challenges)
    for (const auto& affix : state->explicitAffixes) {
        switch (affix.type) {
            case NoMoreDay::MapAffixType::MonsterDensity:
                m_resonanceMods.densityMultiplier *= (1.0f + affix.value);
                break;
            case NoMoreDay::MapAffixType::MonsterLevel:
                m_resonanceMods.levelBonus += static_cast<int>(affix.value);
                break;
            case NoMoreDay::MapAffixType::Enemy_ExtraHealth:
                m_resonanceMods.hpMultiplier *= (1.0f + affix.value);
                break;
            case NoMoreDay::MapAffixType::Enemy_ExtraDamage:
                // Global damage multiplier
                m_resonanceMods.damageMultiplier *= (1.0f + affix.value);
                break;
            case NoMoreDay::MapAffixType::Enemy_Fast:
                m_resonanceMods.speedMultiplier *= (1.0f + affix.value);
                break;
            default:
                break;
        }
    }

    // Depth Scaling for Stats (HP/Dmg +10% per depth beyond 1)
    if (state->currentDepth > 1) {
        float depthScale = 1.0f + (float)(state->currentDepth - 1) * 0.10f;
        m_resonanceMods.hpMultiplier *= depthScale;
        m_resonanceMods.damageMultiplier *= depthScale;
        m_resonanceMods.dropRateBonus *= depthScale; // Reward scaling
    }

    LOG_INFO("EnemySpawnSystem: Dimensional Rift Depth {} (Base Lv: {}) - Density: {:.2f}, Bonus Lv: +{}, Drop: {:.2f}, HP: {:.2f}x",
             state->currentDepth, m_areaLevel,
             m_resonanceMods.densityMultiplier, m_resonanceMods.levelBonus,
             m_resonanceMods.dropRateBonus, m_resonanceMods.hpMultiplier);
  }

  LOG_INFO("EnemySpawnSystem: Initializing level data for biome '{}'",
           static_cast<int>(biomeId));

  const auto &biomeConfig = NoMoreDay::BiomeRegistry::Get().GetBiome(biomeId);
  if (biomeConfig.isSafeZone) {
    LOG_INFO("Safe zone detected, no enemies will be spawned.");
    return;
  }

  using namespace NoMoreDay::Constants::Enemy;
  const int clampedMaxEnemies = std::clamp(
      biomeConfig.maxEnemies, BIOME_MAX_ENEMIES_MIN, BIOME_MAX_ENEMIES_MAX);
  if (biomeConfig.maxEnemies != clampedMaxEnemies) {
    LOG_INFO("EnemySpawnSystem: maxEnemies clamped from {} to {} for biome {}",
             biomeConfig.maxEnemies, clampedMaxEnemies, biomeConfig.id);
  }

  // 1. 种族池逻辑
  std::vector<int> availableRaces;
  if (biomeConfig.enemyPool.empty()) {
    // Default pool if none specified
    availableRaces = {EnemyRace::UNDEAD, EnemyRace::DEMON};
  } else {
    static const std::unordered_map<std::string, int> kRaceMap = {
        {"undead", EnemyRace::UNDEAD},
        {"skeleton", EnemyRace::UNDEAD},
        {"demon", EnemyRace::DEMON},
        {"corrupted", EnemyRace::CORRUPTED},
        {"warcraft", EnemyRace::CORRUPTED},
        {"cultist", EnemyRace::CULTIST},
        {"elf", EnemyRace::ElVES},
        {"elves", EnemyRace::ElVES},
        {"beast", EnemyRace::BEAST},
        {"animal", EnemyRace::BEAST},
        {"goblin", EnemyRace::GOBLIN},
        {"machine", EnemyRace::MACHINE},
        {"mech", EnemyRace::MACHINE},
        {"elemental", EnemyRace::ELEMENTAL}};

    for (const auto &raceName : biomeConfig.enemyPool) {
      auto it = kRaceMap.find(raceName);
      if (it != kRaceMap.end()) {
        availableRaces.push_back(it->second);
      }
    }
    if (availableRaces.empty()) {
      availableRaces = {EnemyRace::UNDEAD, EnemyRace::DEMON};
    }
  }

  // 2. 群聚生成 - 大幅增加密度 (5~10倍)
  using namespace NoMoreDay::Constants::Enemy;
  int baseClusterCount =
      (width * height) / CLUSTER_DENSITY_DIVISOR; // 原来是 1000, 现在是 5倍密度
  
  int clusterCount =
      static_cast<int>(baseClusterCount * m_resonanceMods.densityMultiplier);

  // Ensure strict min/max
  if (biomeConfig.maxEnemies > 0) {
    // 允许生成更多，限制在 maxEnemies 范围内
    clusterCount =
        std::min(clusterCount,
                 std::max(1, clampedMaxEnemies / MIN_CLUSTER_ENEMY_COUNT));
  }
  if (clusterCount < 1)
    clusterCount = 1;

  std::uniform_int_distribution<int> xDist(2, width - 3);
  std::uniform_int_distribution<int> yDist(2, height - 3);
  using namespace NoMoreDay::Constants::Enemy;
  std::uniform_int_distribution<int> countDist(
      MIN_CLUSTER_ENEMY_COUNT,
      MAX_CLUSTER_ENEMY_COUNT); // 每群 5-12 只 (原来 3-6)
  for (int i = 0; i < clusterCount; ++i) {
    if (static_cast<int>(m_spawnData.size()) >= clampedMaxEnemies) {
      break;
    }

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

    const int remainingSlots =
        clampedMaxEnemies - static_cast<int>(m_spawnData.size());
    enemyCount = std::min(enemyCount, std::max(0, remainingSlots));

    for (int j = 0; j < enemyCount; ++j) {
      std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
      std::uniform_real_distribution<float> rDist(0.0f, 1.0f);

      float angle = angleDist(m_gen);
      using namespace NoMoreDay::Constants::Enemy;
      float r = SPAWN_RADIUS_MIN + rDist(m_gen) * SPAWN_RADIUS_MAX;

      int ex = cx + static_cast<int>(cos(angle) * r);
      int ey = cy + static_cast<int>(sin(angle) * r);

      if (ex >= 0 && ex < width && ey >= 0 && ey < height &&
          mapSystem.getTileType(ex, ey) == Tile::Type::FLOOR) {

        EnemySpawnData data;
        using namespace NoMoreDay::Constants::World;
        
        // [FIX] Add sub-tile jitter to prevent perfect overlap physics explosions
        std::uniform_real_distribution<float> jitterDist(-4.0f, 4.0f);
        float jx = jitterDist(m_gen);
        float jy = jitterDist(m_gen);

        data.position = {ex * GRID_TILE_SIZE + (GRID_TILE_SIZE / 2.0f) + jx,
                         ey * GRID_TILE_SIZE + (GRID_TILE_SIZE / 2.0f) + jy};
        data.isAlive = false;
        data.entityId = entt::null;
        data.enemyType = race;

        // Random Variant (Archetype Distribution)
        // 0: Warrior (40%), 1: Archer (30%), 2: Tank (15%), 3: Assassin (10%),
        // 4: Mage (5%)
        std::uniform_int_distribution<int> varDist(0, 99);
        int roll = varDist(m_gen);
        if (roll < 40)
          data.enemyVariant = 0;
        else if (roll < 70)
          data.enemyVariant = 1;
        else if (roll < 85)
          data.enemyVariant = 2;
        else if (roll < 95)
          data.enemyVariant = 3;
        else
          data.enemyVariant = 4;

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
}

void EnemySpawnSystem::updateEnemySpawning(const Position &playerPos,
                                           entt::registry &registry, float dt,
                                           MapSystem *mapSystem) {
  int activeCount = 0;
  for (const auto& d : m_spawnData) if (d.isAlive) activeCount++;
  LOG_LIMITED_INFO(5.0f, "[SpawnSystem] Active Entities: {} / Total Spawn Points: {}", activeCount, m_spawnData.size());

  // Optimization: Stagger updates to avoid O(N) distance checks for 8000+ points every frame
  static size_t startIndex = 0;
  size_t totalPoints = m_spawnData.size();
  size_t pointsToProcess = (totalPoints / 4) + 1; // Process 25% per frame
  
  if (startIndex >= totalPoints) startIndex = 0;
  size_t endIndex = std::min(startIndex + pointsToProcess, totalPoints);

  for (size_t i = startIndex; i < endIndex; ++i) {
    auto &data = m_spawnData[i];
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
  
  startIndex = endIndex;

  if (mapSystem) {
    updateSpawnerWalls(registry, *mapSystem, playerPos, dt);
  }
}

void EnemySpawnSystem::spawnEnemy(entt::registry &registry,
                                  EnemySpawnData &data) {
  auto entity = registry.create();

  registry.emplace<Position>(entity, data.position.x, data.position.y);
  registry.emplace<Velocity>(entity, 0.0f, 0.0f);
  using namespace NoMoreDay::Constants::Enemy;
  registry.emplace<Radius>(
      entity, DEFAULT_COLLISION_RADIUS);  // Default collision radius
  registry.emplace<GPUIndex>(entity, -1); // Will be assigned by GPUEntitySystem
  registry.emplace<IDComponent>(entity, NoMoreDay::Utils::UUID::generate());
  registry.emplace<LocalLevelTag>(entity);

  auto &aState = registry.emplace<NoMoreDay::AttackState>(entity);
  aState.baseAttackInterval = DEFAULT_ATTACK_INTERVAL;

  EnemyRace::Type raceType = static_cast<EnemyRace::Type>(data.enemyType);
  EnemyArchetype::Type archType = EnemyArchetype::FODDER;

  // Determine archetype based on variant
  switch (data.enemyVariant) {
  case 0: archType = EnemyArchetype::FODDER; break; 
  case 1: archType = EnemyArchetype::RANGER; break;
  case 2: archType = EnemyArchetype::TANK; break;
  case 3: archType = EnemyArchetype::ASSASSIN; break;
  case 4: archType = EnemyArchetype::SUPPORT; break;
  }

  // Emplace EnemyStateComponent EARLY so StatsSystem can use it
  auto &esc = registry.emplace<EnemyStateComponent>(entity, raceType, archType);
  esc.activationRange = DEFAULT_AGGRO_DISTANCE;

  registry.emplace<EnemyTag>(entity);
  auto &cStats = registry.emplace<NoMoreDay::CombatStats>(entity);
  registry.emplace<NoMoreDay::StatsDirty>(entity); 

  // === Advanced Rarity System Implementation ===
  std::uniform_real_distribution<float> rarityRoll(0.0f, 1.0f);
  EnemyRarityComponent::Rarity rarity = EnemyRarityComponent::NORMAL;
  Color rarityColor = WHITE;
  float rarityScale = 1.0f;

  float roll = rarityRoll(m_gen);
  if (roll < BOSS_CHANCE) {
    rarity = EnemyRarityComponent::BOSS;
    rarityScale = 2.0f;
    rarityColor = ORANGE;
  } else if (roll < BOSS_CHANCE + ELITE_CHANCE) {
    rarity = EnemyRarityComponent::ELITE;
    rarityScale = 1.4f;
    rarityColor = YELLOW;
  } else if (roll < BOSS_CHANCE + ELITE_CHANCE + CHAMPION_CHANCE) {
    rarity = EnemyRarityComponent::CHAMPION;
    rarityScale = 1.15f;
    rarityColor = SKYBLUE;
  }

  registry.emplace<EnemyRarityComponent>(entity, rarity);

  // === Sprite and Texture Array Setup ===
  auto& sprite = registry.emplace<SpriteComponent>(entity);
  sprite.scale = DEFAULT_SPRITE_SCALE * rarityScale;
  // Map race + variant to global texture array index
  // Each race has 5 variants in the array (see Game::init)
  sprite.textureLayerIndex = static_cast<int>(data.enemyType) * 5 + data.enemyVariant;

  // === Monster Scaling Implementation ===
  int playerLevel = 1;
  auto playerView = registry.view<PlayerStats>();
  if (playerView.begin() != playerView.end()) {
      playerLevel = playerView.get<PlayerStats>(*playerView.begin()).level;
  }

  int effectiveAreaLevel = m_areaLevel + m_resonanceMods.levelBonus;
  
  // Apply new Level Calculation Logic
  // Check if we are in Mosaic Mode (m_resonanceMods.levelBonus > 0 is a heuristic, 
  // but better to add a flag. For now assume if density/level mods exist it's likely scaling content)
  bool isScalingContent = (m_resonanceMods.levelBonus > 0 || m_resonanceMods.densityMultiplier > 1.0f);
  
  int monsterLevel = NoMoreDay::MonsterScaling::CalculateMonsterLevel(
      effectiveAreaLevel, 
      playerLevel, 
      rarity, 
      isScalingContent
  );
  esc.level = monsterLevel;

  NoMoreDay::MonsterScalingResult result = NoMoreDay::MonsterScaling::Calculate(raceType, monsterLevel, rarity);

  result.minDamage *= m_resonanceMods.damageMultiplier;
  result.maxDamage *= m_resonanceMods.damageMultiplier;
  result.maxHealth *= m_resonanceMods.hpMultiplier;

  cStats.min_weapon_damage = result.minDamage;
  cStats.max_weapon_damage = result.maxDamage;
  cStats.armor = result.armor;
  cStats.accuracy = 1.0f;
  cStats.attack_speed = 1.0f * m_resonanceMods.speedMultiplier;
  
  float modifiedHP = result.maxHealth;

  // Apply Race specifics (Stats/AI tweaks beyond base component init)
  switch (raceType) {
  case EnemyRace::UNDEAD:
    registry.emplace<ColorComponent>(entity, rarityColor);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.25f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    break;
  case EnemyRace::DEMON:
    registry.emplace<ColorComponent>(entity, rarityColor);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.45f * (1.0f + m_resonanceMods.dropRateBonus), 1, 2);
    aState.baseAttackInterval = 2.0f;
    cStats.attack_speed = 0.8f;
    break;
  case EnemyRace::CORRUPTED:
    registry.emplace<ColorComponent>(entity, rarityColor);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.30f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    aState.baseAttackInterval = 1.0f;
    cStats.attack_speed = 1.2f;
    break;
  case EnemyRace::CULTIST:
    registry.emplace<ColorComponent>(entity, rarityColor);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.35f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    aState.baseAttackInterval = 1.8f;
    break;
  case EnemyRace::ElVES:
    registry.emplace<ColorComponent>(entity, rarityColor);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.30f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    cStats.attack_speed = 1.3f;
    break;
  case EnemyRace::BEAST:
    registry.emplace<ColorComponent>(entity, rarityColor);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.20f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    break;
  case EnemyRace::GOBLIN:
    registry.emplace<ColorComponent>(
        entity, (rarity == EnemyRarityComponent::NORMAL) ? GREEN : rarityColor);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.20f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    aState.baseAttackInterval = 1.2f;
    break;
  case EnemyRace::MACHINE:
    registry.emplace<ColorComponent>(entity, rarityColor);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.40f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    cStats.armor *= 1.5f;
    break;
  case EnemyRace::ELEMENTAL:
    registry.emplace<ColorComponent>(entity, rarityColor);
    registry.emplace<NoMoreDay::DropTableComponent>(
        entity, 0, 0.30f * (1.0f + m_resonanceMods.dropRateBonus), 1, 1);
    break;
  default:
    registry.emplace<ColorComponent>(entity, rarityColor);
    break;
  }

  // Apply Element Color Tint
  if (m_resonanceMods.dominantElement != 0) {
    auto *colorComp = registry.try_get<ColorComponent>(entity);
    if (colorComp) {
      switch (static_cast<NoMoreDay::FragmentElement>(
          m_resonanceMods.dominantElement)) {
      case NoMoreDay::FragmentElement::Fire: colorComp->color = RED; break;
      case NoMoreDay::FragmentElement::Cold: colorComp->color = SKYBLUE; break;
      case NoMoreDay::FragmentElement::Lightning: colorComp->color = YELLOW; break;
      case NoMoreDay::FragmentElement::Shadow: colorComp->color = PURPLE; break;
      case NoMoreDay::FragmentElement::Chaos: colorComp->color = DARKGRAY; break;
      default: break;
      }
    }
  }

  // Setup AI
  registry.emplace<HealthComponent>(entity, modifiedHP, modifiedHP);
  registry.emplace<AIComponent>(entity, AIType::PATROL, esc.detectionRange,
                                esc.attackRange, esc.speed * m_resonanceMods.speedMultiplier);

  if (registry.all_of<AIComponent>(entity)) {
    auto &ai = registry.get<AIComponent>(entity);
    ai.patrolStart = data.position;

    std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
    std::uniform_int_distribution<int> moveDist(300, 600);
    float angle = angleDist(m_gen);
    float dist = (float)moveDist(m_gen);

    ai.patrolEnd = {data.position.x + std::cos(angle) * dist,
                    data.position.y + std::sin(angle) * dist};

    // === Monster Affix Assignment ===
    if (rarity != EnemyRarityComponent::NORMAL) {
      int numAffixes = 0;
      if (rarity == EnemyRarityComponent::CHAMPION) numAffixes = 1;
      else if (rarity == EnemyRarityComponent::ELITE) numAffixes = 2;
      else if (rarity == EnemyRarityComponent::BOSS) numAffixes = 4;

      auto &affixComp = registry.emplace<NoMoreDay::MonsterAffixComponent>(entity);

      static constexpr std::array<NoMoreDay::MonsterAffixType, 8> kAffixPool = {
          NoMoreDay::MonsterAffixType::Fast, NoMoreDay::MonsterAffixType::Tanky,
          NoMoreDay::MonsterAffixType::Powerful, NoMoreDay::MonsterAffixType::Vampiric,
          NoMoreDay::MonsterAffixType::Berserker, NoMoreDay::MonsterAffixType::Molten,
          NoMoreDay::MonsterAffixType::Frozen, NoMoreDay::MonsterAffixType::Teleporter};

      std::uniform_int_distribution<int> affixRoll(0, static_cast<int>(kAffixPool.size()) - 1);

      for (int m = 0; m < numAffixes; ++m) {
        NoMoreDay::MonsterAffixType selectedAffix = kAffixPool[affixRoll(m_gen)];
        if (affixComp.HasAffix(selectedAffix)) continue;
        affixComp.AddAffix(selectedAffix);

        if (selectedAffix == NoMoreDay::MonsterAffixType::Avenger) {
          (void)registry.get_or_emplace<NoMoreDay::AvengerComponent>(entity);
          (void)registry.get_or_emplace<NoMoreDay::AvengerTag>(entity);
        } else if (selectedAffix == NoMoreDay::MonsterAffixType::SoulLink) {
          (void)registry.get_or_emplace<NoMoreDay::SoulLinkComponent>(entity);
          (void)registry.get_or_emplace<NoMoreDay::SoulLinkTag>(entity);
        }
      }

      if (registry.all_of<HealthComponent>(entity)) {
        auto &hp = registry.get<HealthComponent>(entity);
        hp.max = modifiedHP;
        hp.current = modifiedHP;
      }
    }

    registry.emplace<NoMoreDay::ActiveEffectsComponent>(entity);
  }

  data.entityId = entity;
  data.isAlive = true;
}

void EnemySpawnSystem::despawnEnemy(entt::registry &registry,
                                    EnemySpawnData &data) {
  // If entity is Dormant, do NOT destroy it here, let it persist in pool
  if (registry.valid(data.entityId) &&
      registry.any_of<DormantTag>(data.entityId)) {
    return;
  }

  if (registry.valid(data.entityId)) {
    registry.destroy(data.entityId);
  }
  data.entityId = entt::null;
  data.isAlive = false;
}

void EnemySpawnSystem::updateSpawnerWalls(entt::registry &registry,
                                          MapSystem &mapSystem,
                                          const Position &playerPos, float dt) {
  auto &spawners = mapSystem.getSpawnerWalls();
  if (spawners.empty()) {
    return;
  }

  using namespace NoMoreDay::Constants::World;
  for (auto &spawner : spawners) {
    if (!spawner.isActive) {
      continue;
    }
    if (spawner.currentSpawns >= spawner.maxSpawns) {
      spawner.isActive = false;
      continue;
    }

    const Position worldPos = {(spawner.gridX + 0.5f) * GRID_TILE_SIZE,
                               (spawner.gridY + 0.5f) * GRID_TILE_SIZE};
    const float dx = worldPos.x - playerPos.x;
    const float dy = worldPos.y - playerPos.y;
    const float distSq = dx * dx + dy * dy;
    if (distSq > m_deactivationDistance * m_deactivationDistance) {
      continue;
    }

    spawner.spawnTimer += dt;
    if (spawner.spawnTimer < spawner.spawnInterval) {
      continue;
    }
    spawner.spawnTimer = 0.0f;

    EnemySpawnData data;
    std::uniform_real_distribution<float> jitter(-6.0f, 6.0f);
    data.position = {worldPos.x + jitter(m_gen), worldPos.y + jitter(m_gen)};
    data.enemyType = spawner.spawnPool.empty()
                         ? EnemyRace::CORRUPTED
                         : RaceFromName(spawner.spawnPool[m_gen() %
                                                          spawner.spawnPool.size()]);
    data.enemyVariant = static_cast<int>(m_gen() % 5u);
    data.isAlive = false;
    data.entityId = entt::null;
    data.allowRespawn = false;

    spawnEnemy(registry, data);
    spawner.currentSpawns++;
  }
}

void EnemySpawnSystem::updateDormantEntities(entt::registry &registry,
                                             const Position &playerPos,
                                             int gridW, int gridH) {
  static int frameCounter = 0;
  using namespace NoMoreDay::Constants::Enemy;
  if (++frameCounter < DORMANT_CHECK_INTERVAL_FRAMES)
    return; // Spec 2.3: Re-schedule every 60 frames
  frameCounter = 0;

  auto dormantView = registry.view<DormantTag, Position, AIComponent>();
  int awakenedCount = 0;
  int maxAwakenPerCycle = MAX_AWAKEN_PER_CYCLE; // Throttle awakening

  std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
  std::uniform_real_distribution<float> distDist(
      AWAKEN_DISTANCE_MIN,
      AWAKEN_DISTANCE_MAX); // Just inside active boundary (1950)

  std::vector<entt::entity> toAwaken;
  toAwaken.reserve(static_cast<size_t>(maxAwakenPerCycle));

  for (auto entity : dormantView) {
    if (awakenedCount >= maxAwakenPerCycle)
      break;

    // Find a valid spot
    float angle = angleDist(m_gen);
    float dist = distDist(m_gen);
    float tx = playerPos.x + std::cos(angle) * dist;
    float ty = playerPos.y + std::sin(angle) * dist;

    // Check bounds (approximate)
    using namespace NoMoreDay::Constants::World;
    if (tx < 0 || tx > gridW * GRID_TILE_SIZE || ty < 0 ||
        ty > gridH * GRID_TILE_SIZE)
      continue;

    // Wake up
    auto &pos = dormantView.get<Position>(entity);
    pos.x = tx;
    pos.y = ty;

    LOG_DEBUG("[RENDER_SYNC] Awakening Entity {} at ({:.1f}, {:.1f})", (uint32_t)entity, tx, ty);

    // Store for second pass to avoid iterator invalidation
    toAwaken.push_back(entity);

    // Reset AI
    auto &ai = dormantView.get<AIComponent>(entity);
    ai.aiType = AIType::IDLE; // Spec 3.0: Reset to IDLE, wait for WakeUp
    ai.target = entt::null;
    
    // Explicitly update previous position
    if (auto* prevPos = registry.try_get<PrevPosition>(entity)) {
        prevPos->x = tx;
        prevPos->y = ty;
    }

    awakenedCount++;
  }

  // Second pass: Modify structure (add/remove components)
  for (auto entity : toAwaken) {
    registry.remove<DormantTag>(entity);
    registry.emplace_or_replace<Velocity>(entity, 0.0f, 0.0f);
  }

  if (awakenedCount > 0) {
    LOG_INFO("Recycled {} dormant entities", awakenedCount);
  }
}
