#include "MapSystem.hpp"
#include "../core/BiomeRegistry.hpp"
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>

MapSystem::MapSystem() : m_gen(42) {
}

MapSystem::~MapSystem() {
    if (m_fogTextureValid) {
        UnloadTexture(m_fogTexture);
    }
}

// --- CaveMapGenerator Implementation ---

MapGenerator::MapData CaveMapGenerator::Generate(int width, int height, uint32_t seed, float wallProb, int iterations) {
    MapData map{width, height};
    map.grid.resize(width * height);

    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // 1. 初始化
    for (auto& tile : map.grid) {
        tile.type = (dist(gen) < wallProb) ? Tile::Type::WALL : Tile::Type::FLOOR;
    }

    // 辅助 buffer
    std::vector<Tile> buffer = map.grid;

    // 2. 平滑迭代 (双缓冲)
    for (int i = 0; i < iterations; ++i) {
        auto& src = (i % 2 == 0) ? map.grid : buffer;
        auto& dst = (i % 2 == 0) ? buffer : map.grid;
        SmoothIteration(src, dst, width, height);
    }

    if (iterations % 2 != 0) {
        map.grid = std::move(buffer);
    }

    // 3. 生成障碍物 (使用 Perlin Noise 生成纹路/矿脉结构)
    GenerateObstacles(map.grid, width, height, seed);

    // 4. 边界处理
    ApplyBoundaries(map.grid, width, height);
    
    // 5. Place Exits
    PlaceExits(map.grid, width, height, seed);

    return map;
}

void CaveMapGenerator::PlaceExits(std::vector<Tile>& grid, int w, int h, uint32_t seed) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> xDist(1, w - 2);
    std::uniform_int_distribution<int> yDist(1, h - 2);
    
    // Place Stairs Down (Exit)
    // Find a floor tile that is not too close to the center (0,0 is top left, but spawn is usually middle? SceneManager uses center)
    // Actually let's just pick a random floor tile.
    
    for (int attempt = 0; attempt < 1000; ++attempt) {
        int x = xDist(gen);
        int y = yDist(gen);
        if (grid[y * w + x].type == Tile::Type::FLOOR) {
            grid[y * w + x].type = Tile::Type::STAIRS_DOWN;
            break;
        }
    }
}

void CaveMapGenerator::SmoothIteration(const std::vector<Tile>& src, std::vector<Tile>& dst, int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                dst[y * w + x].type = Tile::Type::WALL;
                continue;
            }
            int wallCount = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                const int offset = (y + dy) * w + x;
                if (src[offset - 1].type == Tile::Type::WALL) wallCount++;
                if (src[offset].type     == Tile::Type::WALL) wallCount++;
                if (src[offset + 1].type == Tile::Type::WALL) wallCount++;
            }
            dst[y * w + x].type = (wallCount > 4) ? Tile::Type::WALL : Tile::Type::FLOOR;
        }
    }
}

void CaveMapGenerator::ApplyBoundaries(std::vector<Tile>& grid, int w, int h) {
    for (int x = 0; x < w; ++x) {
        grid[0 * w + x].type = Tile::Type::WALL;
        grid[(h - 1) * w + x].type = Tile::Type::WALL;
    }
    for (int y = 0; y < h; ++y) {
        grid[y * w + 0].type = Tile::Type::WALL;
        grid[y * w + (w - 1)].type = Tile::Type::WALL;
    }
}

void CaveMapGenerator::GenerateObstacles(std::vector<Tile>& grid, int w, int h, uint32_t seed) {
    // 使用 Perlin Noise 生成连续的障碍物结构 (纹路)
    // scale 4.0f 适合生成较大的连通块
    Image noiseImg = GenImagePerlinNoise(w, h, seed % 1000, seed / 1000, 4.0f);
    Color* pixels = LoadImageColors(noiseImg);
    
    for (int i = 0; i < w * h; ++i) {
        // 只在地板上生成障碍物，且只在噪声值较高(>180)的区域生成，形成斑块或纹路
        if (grid[i].type == Tile::Type::FLOOR && pixels[i].r > 180) {
            grid[i].type = Tile::Type::WALL;
        }
    }
    
    UnloadImageColors(pixels);
    UnloadImage(noiseImg);
}

// --- MapSystem Implementation ---

void MapSystem::generateCaveMap(int width, int height) {
    // 使用具体的生成器实例
    CaveMapGenerator generator;
    const auto& biome = NoMoreDay::BiomeRegistry::Get().GetBiome(m_currentBiomeId);
    auto mapData = generator.Generate(width, height, 42, biome.wallProbability, biome.smoothIterations); // 使用固定种子或随机种子
    
    m_mapData.width = mapData.width;
    m_mapData.height = mapData.height;
    m_mapData.grid = std::move(mapData.grid);
    
    // 初始化流场大小
    m_flowField.resize(width * height, {0.0f, 0.0f});
    m_distanceField.resize(width * height, -1);
}

void MapSystem::generateMap(int width, int height, const std::string& biome) {
    m_currentBiomeId = biome;
    generateCaveMap(width, height);
}

bool MapSystem::isWalkable(int x, int y) const {
    return m_mapData.isWalkable(x, y);
}

Tile::Type MapSystem::getTileType(int x, int y) const {
    return m_mapData.getTile(x, y);
}

void MapSystem::render(const Camera2D& camera) const {
    // 简单的视锥剔除
    int startX = static_cast<int>((camera.target.x - camera.offset.x) / 10.0f);
    int startY = static_cast<int>((camera.target.y - camera.offset.y) / 10.0f);
    int endX = startX + static_cast<int>(GetScreenWidth() / 10.0f) + 2;
    int endY = startY + static_cast<int>(GetScreenHeight() / 10.0f) + 2;

    startX = std::max(0, startX);
    startY = std::max(0, startY);
    endX = std::min(m_mapData.width, endX);
    endY = std::min(m_mapData.height, endY);

    const auto& biome = NoMoreDay::BiomeRegistry::Get().GetBiome(m_currentBiomeId);

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            // 只渲染可见或已探索的区域
            if (isExplored(x, y)) {
                Tile::Type type = getTileType(x, y);
                Color color = BLACK;
                if (type == Tile::Type::FLOOR) color = biome.floorColor;
                else if (type == Tile::Type::WALL) color = biome.wallColor;
                else if (type == Tile::Type::STAIRS_DOWN) color = RED; // Temporary
                
                // 如果只是已探索但当前不可见，变暗
                if (!isVisible(x, y)) {
                    color = ColorTint(color, GRAY);
                }
                
                DrawRectangle(x * 10, y * 10, 10, 10, color);
            }
        }
    }
}

// --- 寻路算法实现 (The "Black Magic") ---

void MapSystem::updateFlowField(const Position& targetPos) {
    int targetX = static_cast<int>(targetPos.x / 10.0f);
    int targetY = static_cast<int>(targetPos.y / 10.0f);

    // 优化：如果目标瓦片没有变化，不重新计算
    if (targetX == static_cast<int>(m_lastFlowTarget.x) && 
        targetY == static_cast<int>(m_lastFlowTarget.y)) {
        return;
    }
    m_lastFlowTarget = { (float)targetX, (float)targetY };

    // 1. 生成积分场 (Integration Field) - Dijkstra / BFS
    std::fill(m_distanceField.begin(), m_distanceField.end(), std::numeric_limits<int>::max());
    std::queue<int> queue;

    if (isWalkable(targetX, targetY)) {
        int targetIdx = targetY * m_mapData.width + targetX;
        m_distanceField[targetIdx] = 0;
        queue.push(targetIdx);
    }

    // 限制搜索深度以优化性能 (例如只计算玩家周围 50 格)
    // 对于全图追踪，可以移除此限制或分帧计算
    const int MAX_DEPTH = 100; 

    while (!queue.empty()) {
        int currIdx = queue.front();
        queue.pop();

        int cx = currIdx % m_mapData.width;
        int cy = currIdx / m_mapData.width;
        int currentDist = m_distanceField[currIdx];

        if (currentDist >= MAX_DEPTH) continue;

        // 检查 4 个邻居
        const int dirs[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
        for (auto& dir : dirs) {
            int nx = cx + dir[0];
            int ny = cy + dir[1];

            if (nx >= 0 && nx < m_mapData.width && ny >= 0 && ny < m_mapData.height) {
                int nIdx = ny * m_mapData.width + nx;
                if (m_distanceField[nIdx] == std::numeric_limits<int>::max() && isWalkable(nx, ny)) {
                    m_distanceField[nIdx] = currentDist + 1;
                    queue.push(nIdx);
                }
            }
        }
    }

    // 2. 生成流场 (Flow Field)
    // 对每个格子，指向距离最小的邻居
    for (int y = 0; y < m_mapData.height; ++y) {
        for (int x = 0; x < m_mapData.width; ++x) {
            int idx = y * m_mapData.width + x;
            if (m_distanceField[idx] == std::numeric_limits<int>::max()) {
                m_flowField[idx] = {0, 0};
                continue;
            }

            int minDist = m_distanceField[idx];
            Vector2 flow = {0, 0};

            const int dirs[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
            for (auto& dir : dirs) {
                int nx = x + dir[0];
                int ny = y + dir[1];
                if (nx >= 0 && nx < m_mapData.width && ny >= 0 && ny < m_mapData.height) {
                    int nIdx = ny * m_mapData.width + nx;
                    if (m_distanceField[nIdx] < minDist) {
                        minDist = m_distanceField[nIdx];
                        flow = { (float)dir[0], (float)dir[1] };
                    }
                }
            }
            m_flowField[idx] = flow;
        }
    }
}

Vector2 MapSystem::getFlowDirection(const Position& pos) const {
    int x = static_cast<int>(pos.x / 10.0f);
    int y = static_cast<int>(pos.y / 10.0f);
    
    if (x >= 0 && x < m_mapData.width && y >= 0 && y < m_mapData.height) {
        return m_flowField[y * m_mapData.width + x];
    }
    return {0, 0};
}

Position MapSystem::getPathNextStep(const Position& start, const Position& end) const {
    // 简单的 A* 实现，只返回下一步的位置
    // 为了性能，这里使用简化的贪心搜索或小范围 A*
    
    int startX = static_cast<int>(start.x / 10.0f);
    int startY = static_cast<int>(start.y / 10.0f);
    int endX = static_cast<int>(end.x / 10.0f);
    int endY = static_cast<int>(end.y / 10.0f);

    if (startX == endX && startY == endY) return end;

    // 简单的贪心：检查哪个邻居离终点更近且可行走
    // 注意：这可能会陷入局部最优，但对于"返回出生点"通常足够，
    // 如果需要更强壮的逻辑，可以替换为完整的 A*
    
    float minCost = std::numeric_limits<float>::max();
    Position nextStep = start;
    bool found = false;

    const int dirs[8][2] = {
        {0, 1}, {0, -1}, {1, 0}, {-1, 0},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (auto& dir : dirs) {
        int nx = startX + dir[0];
        int ny = startY + dir[1];

        if (isWalkable(nx, ny)) {
            float dx = (float)(nx - endX);
            float dy = (float)(ny - endY);
            float distSq = dx*dx + dy*dy;
            
            if (distSq < minCost) {
                minCost = distSq;
                nextStep = { nx * 10.0f + 5.0f, ny * 10.0f + 5.0f };
                found = true;
            }
        }
    }

    return found ? nextStep : start;
}

// --- 占位符实现 (FogOfWarSystem 已经有自己的实现文件) ---
void MapSystem::renderFog(const Camera2D& camera) const {
    // 此函数在 FogOfWarSystem 中实现，这里只是为了接口完整性
    // 实际调用是在 Game::render 中直接调用 FogOfWarSystem::renderFog
}

void MapSystem::updateVisibility(const Position& playerPos, float viewRadius) {
    // 实际逻辑在 FogOfWarSystem 中
}

void MapSystem::ensureConnectivity(std::vector<Tile>& grid, int width, int height) {
    // 已经在 CaveMapGenerator 中处理
}

void MapSystem::floodFill(int startX, int startY, std::vector<bool>& visited, std::vector<int>& regionMap, int regionId) {
    // 辅助函数
}

void MapSystem::updateFogTexture() {
    // 辅助函数
}

void MapSystem::initializeFogTexture(int width, int height) {
    // 辅助函数
}