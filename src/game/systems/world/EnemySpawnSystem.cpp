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
#include <algorithm>
#include <cmath>
#include <random>

EnemySpawnSystem::EnemySpawnSystem()
    : m_mapWidth(0), m_mapHeight(0), m_gen(std::random_device{}()) {
  // 扩大生成和销毁范围
  using namespace NoMoreDay::Constants::Enemy;
  m_activationDistance = DEFAULT_ACTIVATION_DISTANCE;
  m_deactivationDistance = DEFAULT_DEACTIVATION_DISTANCE;
}

EnemySpawnSystem::~EnemySpawnSystem() {
  for (auto &[type, textures] : m_raceTextures) {
    for (auto &tex : textures) {
      UnloadTexture(tex);
    }
  }
  m_raceTextures.clear();
}

void EnemySpawnSystem::initializeLevel(int width, int height,
                                        const MapSystem &mapSystem,
                                        NoMoreDay::BiomeID biome) {
  initData(width, height, mapSystem, biome);
  initTextures();
}

// Async Loading Support
void EnemySpawnSystem::initData(int width, int height,
                                const MapSystem &mapSystem,
                                NoMoreDay::BiomeID biomeId,
                                const NoMoreDay::ResonanceResult *resonance) {  m_mapWidth = width;
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

  LOG_INFO("EnemySpawnSystem: Initializing level data for biome '{}'",
           static_cast<int>(biomeId));

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
  }

  m_pendingRaces = availableRaces; // Store for texture loading

  // 2. 群聚生成 - 大幅增加密度 (5~10倍)
  using namespace NoMoreDay::Constants::Enemy;
  int baseClusterCount =
      (width * height) / CLUSTER_DENSITY_DIVISOR; // 原来是 1000, 现在是 5倍密度
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
  using namespace NoMoreDay::Constants::Enemy;
  std::uniform_int_distribution<int> countDist(
      MIN_CLUSTER_ENEMY_COUNT,
      MAX_CLUSTER_ENEMY_COUNT); // 每群 5-12 只 (原来 3-6)
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
      using namespace NoMoreDay::Constants::Enemy;
      float r = SPAWN_RADIUS_MIN + rDist(m_gen) * SPAWN_RADIUS_MAX;

      int ex = cx + static_cast<int>(cos(angle) * r);
      int ey = cy + static_cast<int>(sin(angle) * r);

      if (ex >= 0 && ex < width && ey >= 0 && ey < height &&
          mapSystem.getTileType(ex, ey) == Tile::Type::FLOOR) {

        EnemySpawnData data;
        using namespace NoMoreDay::Constants::World;
        data.position = {ex * GRID_TILE_SIZE + (GRID_TILE_SIZE / 2.0f),
                         ey * GRID_TILE_SIZE + (GRID_TILE_SIZE / 2.0f)};
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
  // Cleanup old
  for (auto &[type, textures] : m_raceTextures) {
    for (auto &tex : textures) {
      UnloadTexture(tex);
    }
  }
  m_raceTextures.clear();

  // Load new
  for (int raceType : m_pendingRaces) {
    const auto &raceDef = kRaceData[static_cast<size_t>(raceType)];

    std::array<Texture2D, 5> textures;
    for (int i = 0; i < 5; ++i) {
      std::string path =
          std::string(raceDef.texturePath) + "_" + std::to_string(i) + ".png";
      Texture2D tex = LoadTexture(path.c_str());
      if (tex.id == 0) {
        LOG_ERROR("EnemySpawnSystem: Failed to load texture for race {} "
                  "variant {} at '{}'",
                  raceType, i, path);
      }
      textures[i] = tex;
    }
    m_raceTextures[raceType] = textures;
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
  using namespace NoMoreDay::Constants::Enemy;
  registry.emplace<Radius>(
      entity, DEFAULT_COLLISION_RADIUS);  // Default collision radius
  registry.emplace<GPUIndex>(entity, -1); // Will be assigned by GPUEntitySystem
  registry.emplace<IDComponent>(entity, NoMoreDay::Utils::UUID::generate());
  registry.emplace<LocalLevelTag>(entity);

  if (m_raceTextures.count(data.enemyType)) {
    registry.emplace<TextureIDComponent>(
        entity, m_raceTextures[data.enemyType][data.enemyVariant].id);
  }

  EnemyRace::Type raceType = static_cast<EnemyRace::Type>(data.enemyType);
  EnemyArchetype::Type archType = EnemyArchetype::FODDER;

  // Determine archetype based on variant
  switch (data.enemyVariant) {
  case 0:
    archType = EnemyArchetype::FODDER;
    break; // Warrior
  case 1:
    archType = EnemyArchetype::RANGER;
    break;
  case 2:
    archType = EnemyArchetype::TANK;
    break;
  case 3:
    archType = EnemyArchetype::ASSASSIN;
    break;
  case 4:
    archType = EnemyArchetype::SUPPORT;
    break; // Mage/Support
  }

  // Emplace EnemyStateComponent EARLY so StatsSystem can use it
  registry.emplace<EnemyStateComponent>(entity, raceType, archType);
  registry.emplace<EnemyTag>(entity);
  auto &cStats = registry.emplace<NoMoreDay::CombatStats>(entity);
  registry.emplace<NoMoreDay::StatsDirty>(
      entity); // Trigger initial calculation
  auto &aState = registry.emplace<NoMoreDay::AttackState>(entity);

  const auto &raceDef = kRaceData[static_cast<size_t>(raceType)];

  // Apply Level Mod to Base Stats
  using namespace NoMoreDay::Constants::Enemy;
  float levelMultiplier =
      1.0f + (m_resonanceMods.levelBonus * LEVEL_HP_MULTIPLIER);

  cStats.min_weapon_damage =
      raceDef.baseDamage * DAMAGE_VARIANCE_MIN * levelMultiplier;
  cStats.max_weapon_damage =
      raceDef.baseDamage * DAMAGE_VARIANCE_MAX * levelMultiplier;
  cStats.armor = raceDef.baseArmor * levelMultiplier;
  cStats.accuracy = 1.0f;
  cStats.attack_speed = 1.0f;
  aState.baseAttackInterval = DEFAULT_ATTACK_INTERVAL;

  float modifiedHP = raceDef.baseHP * levelMultiplier;

  // === Advanced Rarity System Implementation ===
  std::uniform_real_distribution<float> rarityRoll(0.0f, 1.0f);
  EnemyRarityComponent::Rarity rarity = EnemyRarityComponent::NORMAL;
  float rarityScale = 1.0f;
  Color rarityColor = WHITE;
  float hpMult = 1.0f;
  float dmgMult = 1.0f;

  // Check for Nemesis eligibility based on Faction Aggro
  NoMoreDay::FactionType faction = NoMoreDay::FactionType::Undead;
  if (raceType == EnemyRace::DEMON)
    faction = NoMoreDay::FactionType::Void;
  else if (raceType == EnemyRace::CORRUPTED)
    faction = NoMoreDay::FactionType::Corrupted;

  float roll = rarityRoll(m_gen);
  if (roll < BOSS_CHANCE) {
    rarity = EnemyRarityComponent::BOSS;
    hpMult = BOSS_HP_MULTIPLIER;
    dmgMult = 2.5f;
    rarityScale = 2.0f;
    rarityColor = ORANGE;
  } else if (roll < BOSS_CHANCE + ELITE_CHANCE) {
    rarity = EnemyRarityComponent::ELITE;
    hpMult = ELITE_HP_MULTIPLIER;
    dmgMult = 1.6f;
    rarityScale = 1.4f;
    rarityColor = YELLOW;
  } else if (roll < BOSS_CHANCE + ELITE_CHANCE + CHAMPION_CHANCE) {
    rarity = EnemyRarityComponent::CHAMPION;
    hpMult = CHAMPION_HP_MULTIPLIER;
    dmgMult = 1.25f;
    rarityScale = 1.15f;
    rarityColor = SKYBLUE;
  }

  registry.emplace<EnemyRarityComponent>(entity, rarity);
  modifiedHP *= hpMult;

  // Setup Race/Archetype specifics
  // Add Core Components (Health, AI) using stats from EnemyStateComponent
  auto &esc = registry.get<EnemyStateComponent>(entity);
  registry.emplace<HealthComponent>(entity, modifiedHP, modifiedHP);
  registry.emplace<AIComponent>(entity, AIType::IDLE, esc.detectionRange,
                                esc.attackRange, esc.speed);

  // Setup Race specifics (Stats/AI tweaks beyond base component init)
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

  // Finalize Stats with Rarity Multipliers
  cStats.min_weapon_damage *= dmgMult;
  cStats.max_weapon_damage *= dmgMult;

  // Apply Element Color Tint (Higher priority than rarity color if exists)
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
        break;
      }
    }
  }

  if (m_raceTextures.count(data.enemyType)) {
    using namespace NoMoreDay::Constants::Enemy;
    registry.emplace<SpriteComponent>(
        entity, m_raceTextures[data.enemyType][data.enemyVariant],
        DEFAULT_SPRITE_SCALE * rarityScale);
  }

  if (registry.all_of<AIComponent>(entity)) {
    auto &ai = registry.get<AIComponent>(entity);
    ai.patrolStart = data.position;

    std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
    std::uniform_int_distribution<int> moveDist(80, 150);
    float angle = angleDist(m_gen);
    float dist = (float)moveDist(m_gen);

    ai.patrolEnd = {data.position.x + std::cos(angle) * dist,
                    data.position.y + std::sin(angle) * dist};

    // === Set AIType based on archetype ===
    // === Set AIType based on archetype ===
    // REMOVED: Do not force state here. Let AISystem transition to specialized
    // states when Aggro occurs.
    /*
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
      break;
    }
    */

    // === Monster Affix Assignment (New System) ===
    if (rarity != EnemyRarityComponent::NORMAL) {
      int numAffixes = 0;
      if (rarity == EnemyRarityComponent::CHAMPION)
        numAffixes = 1;
      else if (rarity == EnemyRarityComponent::ELITE)
        numAffixes = 2;
      else if (rarity == EnemyRarityComponent::BOSS)
        numAffixes = 4;

      auto &affixComp =
          registry.emplace<NoMoreDay::MonsterAffixComponent>(entity);

      // Affix pool for random selection (数值型和基础机制型)
      static constexpr std::array<NoMoreDay::MonsterAffixType, 8> kAffixPool = {
          NoMoreDay::MonsterAffixType::Fast,
          NoMoreDay::MonsterAffixType::Tanky,
          NoMoreDay::MonsterAffixType::Powerful,
          NoMoreDay::MonsterAffixType::Vampiric,
          NoMoreDay::MonsterAffixType::Berserker,
          NoMoreDay::MonsterAffixType::Molten,
          NoMoreDay::MonsterAffixType::Frozen,
          NoMoreDay::MonsterAffixType::Teleporter};

      std::uniform_int_distribution<int> affixRoll(
          0, static_cast<int>(kAffixPool.size()) - 1);

      for (int m = 0; m < numAffixes; ++m) {
        NoMoreDay::MonsterAffixType selectedAffix =
            kAffixPool[affixRoll(m_gen)];

        // Avoid duplicate affixes
        if (affixComp.HasAffix(selectedAffix)) {
          // Try once more
          selectedAffix = kAffixPool[affixRoll(m_gen)];
          if (affixComp.HasAffix(selectedAffix))
            continue;
        }

        affixComp.AddAffix(selectedAffix);

        // Stat modifiers are now handled by StatsSystem during Recalculate.
        // We just need to ensure StatsDirty is set (already handled by default
        // in spawnEnemy).

        // Backward compatibility: Add legacy tags for existing systems
        if (selectedAffix == NoMoreDay::MonsterAffixType::Avenger) {
          (void)registry.get_or_emplace<NoMoreDay::AvengerComponent>(entity);
          (void)registry.get_or_emplace<NoMoreDay::AvengerTag>(entity);
        } else if (selectedAffix == NoMoreDay::MonsterAffixType::SoulLink) {
          (void)registry.get_or_emplace<NoMoreDay::SoulLinkComponent>(entity);
          (void)registry.get_or_emplace<NoMoreDay::SoulLinkTag>(entity);
        }
      }

      // Update HealthComponent with modified HP
      if (registry.all_of<HealthComponent>(entity)) {
        auto &hp = registry.get<HealthComponent>(entity);
        hp.max = modifiedHP;
        hp.current = modifiedHP;
      }

      LOG_TRACE("Entity {} spawned with {} affixes", (uint32_t)entity,
                affixComp.affixes.size());
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

    registry.remove<DormantTag>(entity);
    registry.emplace_or_replace<Velocity>(entity, 0.0f,
                                          0.0f); // Re-add Velocity

    // Reset AI
    auto &ai = dormantView.get<AIComponent>(entity);
    ai.aiType = AIType::IDLE; // Spec 3.0: Reset to IDLE, wait for WakeUp
    ai.target = entt::null;

    awakenedCount++;
  }

  if (awakenedCount > 0) {
    LOG_INFO("Recycled {} dormant entities", awakenedCount);
  }
}
