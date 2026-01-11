#include "game/systems/world/EnemySpawnSystem.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Combat.hpp"
#include "game/components/PlayerState.hpp" // For stats if needed
#include "game/components/ItemComponent.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/UUID.hpp"
#include <random>
#include <algorithm>
#include <cmath>

EnemySpawnSystem::EnemySpawnSystem() 
    : m_mapWidth(0), m_mapHeight(0), m_gen(std::random_device{}()) {
    // 扩大生成和销毁范围
    m_activationDistance = 1200.0f;
    m_deactivationDistance = 2000.0f;
}

EnemySpawnSystem::~EnemySpawnSystem() {
    for (auto& [type, texture] : m_raceTextures) {
        UnloadTexture(texture);
    }
    m_raceTextures.clear();
}

void EnemySpawnSystem::initializeLevel(int width, int height, const MapSystem& mapSystem, const std::string& biome) {
    initData(width, height, mapSystem, biome);
    initTextures();
}

void EnemySpawnSystem::initData(int width, int height, const MapSystem& mapSystem, const std::string& biomeId) {
    m_mapWidth = width;
    m_mapHeight = height;
    m_spawnData.clear();
    m_pendingRaces.clear();
    
    LOG_INFO("EnemySpawnSystem: Initializing level data for biome '{}'", biomeId);

    const auto& biomeConfig = NoMoreDay::BiomeRegistry::Get().GetBiome(biomeId);
    if (biomeConfig.isSafeZone) {
        LOG_INFO("Safe zone detected, no enemies will be spawned.");
        return;
    }

    // 1. 种族池逻辑
    std::vector<int> availableRaces;
    if (biomeConfig.enemyPool.empty()) {
        // Default pool if none specified
        availableRaces = { EnemyRace::UNDEAD, EnemyRace::DEMON };
    } else {
        for (const auto& raceName : biomeConfig.enemyPool) {
            if (raceName == "undead" || raceName == "skeleton") availableRaces.push_back(EnemyRace::UNDEAD);
            else if (raceName == "demon") availableRaces.push_back(EnemyRace::DEMON);
            else if (raceName == "corrupted") availableRaces.push_back(EnemyRace::CORRUPTED);
            else if (raceName == "cultist") availableRaces.push_back(EnemyRace::CULTIST);
            else if (raceName == "goblin") availableRaces.push_back(EnemyRace::GOBLIN);
            else if (raceName == "slime") availableRaces.push_back(EnemyRace::SLIME);
        }
    }
    
    m_pendingRaces = availableRaces; // Store for texture loading

    // 2. 群聚生成 - 大幅增加密度 (5~10倍)
    int clusterCount = (width * height) / 200; // 原来是 1000, 现在是 5倍密度
    if (biomeConfig.maxEnemies > 0) {
        // 允许生成更多，限制在 maxEnemies 范围内
        clusterCount = std::min(clusterCount, biomeConfig.maxEnemies / 5);
    }
    if (clusterCount < 1 && biomeConfig.maxEnemies > 0) clusterCount = 1;
    
    std::uniform_int_distribution<int> xDist(2, width - 3);
    std::uniform_int_distribution<int> yDist(2, height - 3);
    std::uniform_int_distribution<int> countDist(5, 12); // 每群 5-12 只 (原来 3-6)
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
        
        if (!foundCenter) continue;

        int race = availableRaces[i % availableRaces.size()];
        int enemyCount = countDist(m_gen);
        
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
                data.position = { ex * 10.0f + 5.0f, ey * 10.0f + 5.0f };
                data.isAlive = false;
                data.entityId = entt::null;
                data.enemyType = race;
                data.allowRespawn = false;
                
                m_spawnData.push_back(data);
            }
        }
    }
    
    LOG_INFO("Initialized {} enemy spawn points in {} clusters", m_spawnData.size(), clusterCount);
}

void EnemySpawnSystem::initTextures() {
    // Cleanup old
    for (auto& [type, texture] : m_raceTextures) {
        UnloadTexture(texture);
    }
    m_raceTextures.clear();

    // Load new
    for (int raceType : m_pendingRaces) {
        EnemyRace raceDef(static_cast<EnemyRace::Type>(raceType));
        Texture2D tex = LoadTexture(raceDef.texturePath.c_str());
        if (tex.id == 0) {
            LOG_ERROR("EnemySpawnSystem: Failed to load texture for race {} at '{}'", raceType, raceDef.texturePath);
        } else {
            m_raceTextures[raceType] = tex;
        }
    }
    m_pendingRaces.clear();
}

void EnemySpawnSystem::updateEnemySpawning(const Position& playerPos, entt::registry& registry) {
    for (auto& data : m_spawnData) {
        float dx = data.position.x - playerPos.x;
        float dy = data.position.y - playerPos.y;
        float distSq = dx*dx + dy*dy;
        
        if (!data.isAlive) {
            if (data.enemyType == -1) continue;

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

void EnemySpawnSystem::spawnEnemy(entt::registry& registry, EnemySpawnData& data) {
    auto entity = registry.create();
    
    registry.emplace<Position>(entity, data.position.x, data.position.y);
    registry.emplace<Velocity>(entity, 0.0f, 0.0f);
    registry.emplace<Radius>(entity, 5.0f); // Default collision radius
    registry.emplace<GPUIndex>(entity, -1); // Will be assigned by GPUEntitySystem
    registry.emplace<IDComponent>(entity, NoMoreDay::Utils::UUID::generate());
    registry.emplace<LocalLevelTag>(entity);
    
    if (m_raceTextures.count(data.enemyType)) {
        registry.emplace<TextureIDComponent>(entity, m_raceTextures[data.enemyType].id);
    }
    
    EnemyRace::Type raceType = static_cast<EnemyRace::Type>(data.enemyType);
    EnemyArchetype::Type archType = EnemyArchetype::FODDER;
    
    // Determine archetype based on race/logic (simplified for now)
    if (raceType == EnemyRace::DEMON) archType = EnemyArchetype::TANK;
    else if (raceType == EnemyRace::CORRUPTED) archType = EnemyArchetype::ASSASSIN;
    else if (raceType == EnemyRace::CULTIST) archType = EnemyArchetype::RANGER;

    // Emplace EnemyStateComponent EARLY so StatsSystem can use it
    registry.emplace<EnemyStateComponent>(entity, raceType, archType);
    registry.emplace<EnemyTag>(entity);
    auto& cStats = registry.emplace<NoMoreDay::CombatStats>(entity); 
    registry.emplace<NoMoreDay::StatsDirty>(entity); // Trigger initial calculation
    auto& aState = registry.emplace<NoMoreDay::AttackState>(entity);
    
    EnemyRace raceDef(raceType);

    // Initialize monster stats from race
    cStats.min_weapon_damage = raceDef.baseDamage * 0.8f;
    cStats.max_weapon_damage = raceDef.baseDamage * 1.2f;
    cStats.armor = raceDef.baseArmor;
    cStats.accuracy = 1.0f; // Standard accuracy
    cStats.attack_speed = 1.0f; // Standard attack speed
    aState.baseAttackInterval = 1.5f; // Default attack interval
    
    switch (raceType) {
        case EnemyRace::UNDEAD:
            registry.emplace<HealthComponent>(entity, raceDef.baseHP, raceDef.baseHP);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 150.0f, 40.0f, 50.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.25f, 1, 1);
            break;
        case EnemyRace::DEMON:
            registry.emplace<HealthComponent>(entity, raceDef.baseHP, raceDef.baseHP);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 200.0f, 50.0f, 70.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.45f, 1, 2);
            aState.baseAttackInterval = 2.0f; // Slow but heavy
            cStats.attack_speed = 0.8f;
            break;
        case EnemyRace::CORRUPTED:
            registry.emplace<HealthComponent>(entity, raceDef.baseHP, raceDef.baseHP);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 250.0f, 60.0f, 100.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.30f, 1, 1);
            aState.baseAttackInterval = 1.0f; // Fast
            cStats.attack_speed = 1.2f;
            break;
        case EnemyRace::CULTIST:
            registry.emplace<HealthComponent>(entity, raceDef.baseHP, raceDef.baseHP);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 180.0f, 30.0f, 60.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.35f, 1, 1);
            aState.baseAttackInterval = 1.8f;
            break;
        case EnemyRace::GOBLIN:
            registry.emplace<HealthComponent>(entity, raceDef.baseHP, raceDef.baseHP);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 120.0f, 40.0f, 60.0f);
            registry.emplace<ColorComponent>(entity, GREEN);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.20f, 1, 1);
            aState.baseAttackInterval = 1.2f;
            break;
        case EnemyRace::SLIME:
            registry.emplace<HealthComponent>(entity, raceDef.baseHP, raceDef.baseHP);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 80.0f, 20.0f, 30.0f);
            registry.emplace<ColorComponent>(entity, LIME);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.15f, 1, 1);
            aState.baseAttackInterval = 2.5f;
            break;
        default:
            registry.emplace<HealthComponent>(entity, raceDef.baseHP, raceDef.baseHP);
            registry.emplace<ColorComponent>(entity, WHITE);
            break;
    }
    
    if (m_raceTextures.count(data.enemyType)) {
        registry.emplace<SpriteComponent>(entity, m_raceTextures[data.enemyType], 0.2f);
    }
    
    if (registry.all_of<AIComponent>(entity)) {
        auto& ai = registry.get<AIComponent>(entity);
        ai.patrolStart = data.position;
        
        std::uniform_real_distribution<float> angleDist(0.0f, 6.283185f);
        std::uniform_real_distribution<float> distDist(30.0f, 60.0f);
        float angle = angleDist(m_gen);
        float dist = distDist(m_gen);
        
        ai.patrolEnd = {
            data.position.x + std::cos(angle) * dist,
            data.position.y + std::sin(angle) * dist
        };
    }
    
    data.entityId = entity;
    data.isAlive = true;
}

void EnemySpawnSystem::despawnEnemy(entt::registry& registry, EnemySpawnData& data) {
    if (registry.valid(data.entityId)) {
        registry.destroy(data.entityId);
    }
    data.entityId = entt::null;
    data.isAlive = false;
}
