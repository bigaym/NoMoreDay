#include "EnemySpawnSystem.hpp"
#include "../components/EnemyComponent.hpp"
#include "../components/AIComponent.hpp"
#include "../components/PlayerState.hpp" // For stats if needed
#include "../tools/Logger.hpp"
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
        m_raceTextures[raceType] = LoadTexture(raceDef.texturePath.c_str());
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
            if (distSq < m_activationDistance * m_activationDistance) {
                spawnEnemy(registry, data);
            }
        } else {
            // 尝试销毁：如果超出销毁范围
            if (distSq > m_deactivationDistance * m_deactivationDistance) {
                // 检查实体是否还存在（可能已经被打死了）
                if (registry.valid(data.entityId)) {
                    despawnEnemy(registry, data);
                } else {
                    // 已经被打死，标记为不再生成 (或者你可以选择重生逻辑)
                    // 这里我们简单地让它保持"活着"状态但ID无效，或者重置？
                    // 如果是Roguelike，通常死了就死了。
                    // 为了简单，我们从 spawnData 中移除它？或者标记为永久死亡。
                    // 这里暂时不做处理，假设死了就不再管理
                }
            }
        }
    }
}

void EnemySpawnSystem::updateEnemyBehavior(float dt, const Position& playerPos, entt::registry& registry) {
    // 获取 MapSystem (假设通过某种方式可以访问，或者我们需要传入 LevelManager)
    // 由于接口限制，我们这里假设 Game 循环中已经更新了 MapSystem 的流场
    // 并且我们需要在这里访问 MapSystem。
    // 临时方案：通过 registry 的 context 获取，或者修改函数签名传入 MapSystem&
    // 为了不破坏现有结构，我们假设调用者会传入 MapSystem，这里先修改函数签名
    // 但由于头文件限制，我们只能在 Game.cpp 调用时传入。
    // 让我们修改 updateEnemyBehavior 的实现，使其包含移动逻辑。
    
    auto view = registry.view<const Position, const EnemyStateComponent, AIComponent, HealthComponent>();
    auto velView = registry.view<Velocity>(); // 需要写入速度
    
    // 我们需要访问 MapSystem 来获取流场。
    // 由于 EnemySpawnSystem 没有持有 MapSystem 的引用，我们需要在 initializeLevel 时保存它吗？
    // initializeLevel 传入了 const MapSystem&，我们可以保存一个指针。
    // 但为了安全，最好在 update 时传入。
    // 这里我们假设 m_mapSystemPtr 是在 initializeLevel 中保存的。
    
    for (auto entity : view) {
        // 使用引用获取 Position，以便在传送时修改 (需要非 const 引用，但 view 默认给 const)
        // 所以我们需要用 registry.get<Position> 或者 replace
        const auto& pos = view.get<Position>(entity); 
        const auto& state = view.get<EnemyStateComponent>(entity);
        auto& ai = view.get<AIComponent>(entity);
        auto& health = view.get<HealthComponent>(entity);
        
        // 获取速度组件 (如果存在)
        Velocity* vel = registry.try_get<Velocity>(entity);

        float dx = pos.x - playerPos.x;
        float dy = pos.y - playerPos.y;
        float distSq = dx*dx + dy*dy;
        
        // 仇恨范围 (使用 deactivationRange 作为脱战距离)
        float leashRangeSq = state.deactivationRange * state.deactivationRange;
        // 强制重置范围 (2倍脱战距离)
        float hardResetRangeSq = leashRangeSq * 4.0f; // (range * 2)^2 = range^2 * 4
        // 激活范围
        float wakeUpRangeSq = state.activationRange * state.activationRange;

        // 1. 距离过远：强制传送回出生点并休眠
        if (distSq > hardResetRangeSq) {
            // 传送回出生点
            registry.replace<Position>(entity, ai.patrolStart);
            
            // 重置状态为 IDLE
            ai.aiType = AIType::IDLE;
            ai.target = entt::null;
            
            // 停止移动
            if (vel) {
                vel->vx = 0.0f;
                vel->vy = 0.0f;
            }
            
            // 瞬间回满血
            health.current = health.max;
        }
        // 2. 超出脱战距离：放弃追击，开始回血
        else if (distSq > leashRangeSq) {
            // 如果处于攻击或追击状态，强制脱战并返回巡逻
            if (ai.aiType == AIType::CHASE || ai.aiType == AIType::ATTACK) {
                ai.aiType = AIType::PATROL;
                ai.target = entt::null;
            }
            
            // 脱战状态下回血 (直到 95%)
            if (health.current < health.max * 0.95f) {
                health.current += health.max * 0.10f * dt;
                if (health.current > health.max) health.current = health.max;
            }
        }
        // 3. 进入激活范围：唤醒怪物
        else if (distSq < wakeUpRangeSq) {
            if (ai.aiType == AIType::IDLE) {
                ai.aiType = AIType::PATROL;
            }
        }
        
        // --- 移动逻辑 (基于流场和寻路) ---
        // 只有非 IDLE 状态才移动
        if (vel && m_mapSystemPtr && ai.aiType != AIType::IDLE) {
            if (ai.aiType == AIType::CHASE) {
                // 使用流场 (Black Magic)
                Vector2 flow = m_mapSystemPtr->getFlowDirection(pos);
                if (flow.x != 0 || flow.y != 0) {
                    vel->vx = flow.x * ai.speed;
                    vel->vy = flow.y * ai.speed;
                } else {
                    // 如果流场无效（例如在墙里），简单的直线追踪
                    float dist = std::sqrt(distSq);
                    if (dist > 0) {
                        vel->vx = -(dx / dist) * ai.speed;
                        vel->vy = -(dy / dist) * ai.speed;
                    }
                }
            } else if (ai.aiType == AIType::PATROL) {
                // 使用 A* 返回巡逻点
                Position nextStep = m_mapSystemPtr->getPathNextStep(pos, ai.patrolEnd);
                float pdx = nextStep.x - pos.x;
                float pdy = nextStep.y - pos.y;
                float pdist = std::sqrt(pdx*pdx + pdy*pdy);
                
                if (pdist > 1.0f) {
                    vel->vx = (pdx / pdist) * ai.speed * 0.5f; // 巡逻速度较慢
                    vel->vy = (pdy / pdist) * ai.speed * 0.5f;
                } else {
                    // 到达目标，停止或选择新点 (这里简化为停止)
                    vel->vx = 0; vel->vy = 0;
                }
            }
        }
    }
}

void EnemySpawnSystem::spawnEnemy(entt::registry& registry, EnemySpawnData& data) {
    auto entity = registry.create();
    
    // 基础组件
    registry.emplace<Position>(entity, data.position.x, data.position.y);
    registry.emplace<Velocity>(entity, 0.0f, 0.0f);
    registry.emplace<EnemyTag>(entity);
    
    EnemyRace::Type race = static_cast<EnemyRace::Type>(data.enemyType);
    
    // 根据类型添加特定组件
    switch (race) {
        case EnemyRace::UNDEAD:
            registry.emplace<EnemyStateComponent>(entity, EnemyRace::UNDEAD, EnemyArchetype::FODDER);
            registry.emplace<HealthComponent>(entity, 30.0f, 30.0f);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 150.0f, 40.0f, 50.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            break;
        case EnemyRace::DEMON:
            registry.emplace<EnemyStateComponent>(entity, EnemyRace::DEMON, EnemyArchetype::TANK);
            registry.emplace<HealthComponent>(entity, 60.0f, 60.0f);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 200.0f, 50.0f, 70.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            break;
        case EnemyRace::CORRUPTED:
            registry.emplace<EnemyStateComponent>(entity, EnemyRace::CORRUPTED, EnemyArchetype::ASSASSIN);
            registry.emplace<HealthComponent>(entity, 25.0f, 25.0f);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 250.0f, 60.0f, 100.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
            break;
        case EnemyRace::CULTIST:
            registry.emplace<EnemyStateComponent>(entity, EnemyRace::CULTIST, EnemyArchetype::RANGER);
            registry.emplace<HealthComponent>(entity, 35.0f, 35.0f);
            registry.emplace<AIComponent>(entity, AIType::PATROL, 180.0f, 30.0f, 60.0f);
            registry.emplace<ColorComponent>(entity, WHITE);
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
    if (registry.valid(data.entityId)) {
        registry.destroy(data.entityId);
    }
    data.entityId = entt::null;
    data.isAlive = false;
}