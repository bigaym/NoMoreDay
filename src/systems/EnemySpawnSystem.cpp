#include "EnemySpawnSystem.hpp"
#include "../components/EnemyComponent.hpp"
#include "../components/AIComponent.hpp"
#include "../components/PlayerState.hpp" // For stats if needed
#include "../components/ItemComponent.hpp"
#include "../tools/Logger.hpp"
#include "../utils/UUID.hpp"
#include <random>
#include <algorithm>
#include <cmath>

EnemySpawnSystem::EnemySpawnSystem() 
    : m_mapWidth(0), m_mapHeight(0), m_gen(std::random_device{}()) {
    // 扩大生成和销毁范围，以支持怪物的"传送回城"逻辑
    // 只有当怪物距离玩家非常远(2000+)时才真正销毁实体
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
    m_mapWidth = width;
    m_mapHeight = height;
    m_spawnData.clear();
    
    LOG_INFO("EnemySpawnSystem: Initializing level for biome '{}'", biome);

    // 清理旧纹理
    for (auto& [type, texture] : m_raceTextures) {
        UnloadTexture(texture);
    }
    m_raceTextures.clear();
    
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    // 1. 种族池逻辑 (Race Deck)
    // 根据文档：每张地图生成时，随机抽取 2-4 个种族作为本关卡的“生态系”
    std::vector<int> availableRaces;
    if (biome == "demon" || biome == "hell") {
        availableRaces = { EnemyRace::DEMON, EnemyRace::CULTIST };
    } else {
        // 默认/洞穴: 随机从所有种族中选2个
        std::vector<int> allRaces = { EnemyRace::UNDEAD, EnemyRace::DEMON, EnemyRace::CORRUPTED, EnemyRace::CULTIST };
        std::shuffle(allRaces.begin(), allRaces.end(), m_gen);
        availableRaces.push_back(allRaces[0]);
        availableRaces.push_back(allRaces[1]);
    }
    
    // 加载本关卡所需种族的纹理
    for (int raceType : availableRaces) {
        EnemyRace raceDef(static_cast<EnemyRace::Type>(raceType));
        Texture2D tex = LoadTexture(raceDef.texturePath.c_str());
        if (tex.id == 0) {
            LOG_ERROR("EnemySpawnSystem: Failed to load texture for race {} at '{}'", raceType, raceDef.texturePath);
        } else {
            m_raceTextures[raceType] = tex;
        }
    }
    
    // 2. 群聚生成 (Cluster Spawning)
    // 文档建议：不在地图上均匀撒胡椒面，而是基于“据点”生成
    // 假设密度：每 800 格 (原来是400) 一个群聚，减少一半数量
    int clusterCount = (width * height) / 800;
    if (clusterCount < 1) clusterCount = 1;
    
    std::uniform_int_distribution<int> xDist(2, width - 3);
    std::uniform_int_distribution<int> yDist(2, height - 3);
    std::uniform_int_distribution<int> countDist(3, 6); // 每个群聚 3-6 个怪
    
    for (int i = 0; i < clusterCount; ++i) {
        // 尝试寻找有效的群聚中心 (ClusterCenter)
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

        // 选定该群聚的种族
        int race = availableRaces[i % availableRaces.size()];
        int enemyCount = countDist(m_gen);
        
        // 在 ClusterCenter 周围半径 R 内生成敌人
        for (int j = 0; j < enemyCount; ++j) {
            float angle = dist(m_gen) * 6.283185f; // 2*PI
            float r = 2.0f + dist(m_gen) * 4.0f;   // 半径 2-6 格
            
            int ex = cx + static_cast<int>(cos(angle) * r);
            int ey = cy + static_cast<int>(sin(angle) * r);
            
            // 边界与地形检查
            if (ex >= 0 && ex < width && ey >= 0 && ey < height && 
                mapSystem.getTileType(ex, ey) == Tile::Type::FLOOR) {
                
                EnemySpawnData data;
                data.position = { ex * 10.0f + 5.0f, ey * 10.0f + 5.0f }; // 居中
                data.isAlive = false;
                data.entityId = entt::null;
                data.enemyType = race;
                data.allowRespawn = false; // 默认不重生
                
                m_spawnData.push_back(data);
            }
        }
    }
    
    LOG_INFO("Initialized {} enemy spawn points in {} clusters", m_spawnData.size(), clusterCount);
}

void EnemySpawnSystem::updateEnemySpawning(const Position& playerPos, entt::registry& registry) {
    for (auto& data : m_spawnData) {
        float dx = data.position.x - playerPos.x;
        float dy = data.position.y - playerPos.y;
        float distSq = dx*dx + dy*dy;
        
        if (!data.isAlive) {
            // 尝试生成：如果在激活范围内
            // 只有当允许重生 或者 这是一个从未生成过的点（这里有点歧义，
            // 既然 isAlive 初始为 false，我们需要区分 "从未生成" 和 "已死"。
            // 简单起见，我们假设 data 在初始化时就是为了生成的。
            // 为了实现 "死后不复活"，我们需要一个状态标记它"已经死过"。
            // 但 data 结构里目前没有 "hasDied"。
            // 然而，如果 allowRespawn 为 false，我们只希望它生成一次。
            // 可是 m_spawnData 是预生成的点。
            // 让我们加一个 flag "hasSpawned" 或者利用 entityId?
            // 不，entityId 被重置了。
            // 让我们假设：如果不允许重生，那么当它死的时候，我们应该把它从 spawnData 移除，
            // 或者标记一个 permanentDead 标志。
            // 为了最小化改动，我们在它死的时候检查 allowRespawn。
            // 如果 allowRespawn 为 false，我们在这里就不做任何事？
            // 不，这里是生成逻辑。如果它死了 (isAlive=false)，我们怎么知道它是刚初始化还没生，还是已经死过了？
            // 我们需要一个 extra flag "hasBeenKilled"。
            
            // 修正策略：在 Entity 死亡被检测到时 (else 分支)，如果 allowRespawn 为 false，
            // 我们就将这个 spawn point 标记为永久失效。
            // 我们可以重用 enemyType = -1 或者增加一个 bool enabled = true;
            
            // 让我们修改 updateEnemySpawning 的逻辑：
            // 如果 data.enemyType == -1，跳过。
            if (data.enemyType == -1) continue;

            if (distSq < m_activationDistance * m_activationDistance) {
                 spawnEnemy(registry, data);
            }
        } else {
            // Check if entity is still valid (might have been killed by player)
            if (!registry.valid(data.entityId)) {
                data.isAlive = false;
                data.entityId = entt::null;
                
                // 核心修改：如果不允许重生，标记该生成点失效
                if (!data.allowRespawn) {
                    LOG_DEBUG("EnemySpawnSystem: Spawn point at ({:.1f}, {:.1f}) permanently disabled", data.position.x, data.position.y);
                    data.enemyType = -1; // Mark as permanently dead/disabled
                }
                
                continue; 
            }

            // 尝试销毁：如果超出销毁范围
            if (distSq > m_deactivationDistance * m_deactivationDistance) {
                despawnEnemy(registry, data);
            }
        }
    }
}

void EnemySpawnSystem::spawnEnemy(entt::registry& registry, EnemySpawnData& data) {
    // LOG_TRACE("EnemySpawnSystem: Spawning enemy type {} at ({:.1f}, {:.1f})", data.enemyType, data.position.x, data.position.y);

    auto entity = registry.create();
    
    // 基础组件
    registry.emplace<Position>(entity, data.position.x, data.position.y);
    registry.emplace<Velocity>(entity, 0.0f, 0.0f);
    // 核心修复：为怪物添加 UUID 和 TextureID，确保其掉落物和视觉效果在读档后能被正确处理
    registry.emplace<IDComponent>(entity, NoMoreDay::Utils::UUID::generate());
    if (m_raceTextures.count(data.enemyType)) {
        registry.emplace<TextureIDComponent>(entity, m_raceTextures[data.enemyType].id);
    }
    registry.emplace<EnemyTag>(entity);
    
    EnemyRace::Type race = static_cast<EnemyRace::Type>(data.enemyType);
    
    // 根据类型添加特定组件
    switch (race) {
        case EnemyRace::UNDEAD:
            registry.emplace<EnemyStateComponent>(entity, EnemyRace::UNDEAD, EnemyArchetype::FODDER);
            registry.emplace<HealthComponent>(entity, 30.0f, 30.0f);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 150.0f, 40.0f, 50.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.25f, 1, 1); // 25% 几率掉落，使用全局池(ID 0)
            break;
        case EnemyRace::DEMON:
            registry.emplace<EnemyStateComponent>(entity, EnemyRace::DEMON, EnemyArchetype::TANK);
            registry.emplace<HealthComponent>(entity, 60.0f, 60.0f);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 200.0f, 50.0f, 70.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.45f, 1, 2); // 45% 几率掉落，可能掉落2件
            break;
        case EnemyRace::CORRUPTED:
            registry.emplace<EnemyStateComponent>(entity, EnemyRace::CORRUPTED, EnemyArchetype::ASSASSIN);
            registry.emplace<HealthComponent>(entity, 25.0f, 25.0f);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 250.0f, 60.0f, 100.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.30f, 1, 1); // 30% 几率掉落
            break;
        case EnemyRace::CULTIST:
            registry.emplace<EnemyStateComponent>(entity, EnemyRace::CULTIST, EnemyArchetype::RANGER);
            registry.emplace<HealthComponent>(entity, 35.0f, 35.0f);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 180.0f, 30.0f, 60.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            registry.emplace<NoMoreDay::DropTableComponent>(entity, 0, 0.35f, 1, 1); // 35% 几率掉落
            break;
    }
    
    // 视觉组件 (如果有纹理资源，这里加载)
    if (m_raceTextures.count(data.enemyType)) {
        // 假设纹理较大，缩放0.1f以匹配玩家大小 (根据实际素材调整)
        registry.emplace<SpriteComponent>(entity, m_raceTextures[data.enemyType], 0.1f);
    }
    
    // 修正：设置巡逻起始点为生成位置，防止AI默认跑向(0,0)
    if (registry.all_of<AIComponent>(entity)) {
        auto& ai = registry.get<AIComponent>(entity);
        ai.patrolStart = data.position;
        
        // 随机设置初始巡逻终点，确保AI开始移动
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
    LOG_TRACE("EnemySpawnSystem: Despawning enemy entity {} (out of range)", (uint32_t)data.entityId);

    if (registry.valid(data.entityId)) {
        registry.destroy(data.entityId);
    }
    data.entityId = entt::null;
    data.isAlive = false;
}