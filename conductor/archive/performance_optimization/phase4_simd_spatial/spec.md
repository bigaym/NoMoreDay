# Phase 4: SIMD SpatialGrid Query 规格说明书

**Track ID**: `performance_optimization/phase4_simd_spatial`  
**优先级**: P2 (逻辑提速)  
**预计收益**: 空间查询性能提升 4-6x  
**依赖**: 无 (可独立进行)

---

## 1. 问题陈述 (Problem Statement)

### 当前实现分析

```cpp
// SpatialHashGrid::query() 当前实现
for (size_t i = start; i < m_entries.size(); ++i) {
    const auto& entry = m_entries[i];
    if (entry.cellHash != hash) break;
    
    // 标量距离计算 - 每次只处理 1 个实体
    float dx = entry.pos.x - pos.x;
    float dy = entry.pos.y - pos.y;
    float distSq = dx * dx + dy * dy;
    
    if (distSq <= radiusSq) {
        callback(entry.entity, entry.pos);
    }
}
```

### 性能瓶颈
| 操作 | 标量耗时 | SIMD 潜力 |
|------|----------|-----------|
| 距离平方计算 | 4 条指令/实体 | 8 实体/指令 (AVX2) |
| 范围比较 | 1 条指令/实体 | 8 实体/指令 |
| 总吞吐量 | ~100M ops/s | ~600M ops/s |

---

## 2. 技术方案 (Technical Design)

### 2.1 SIMD 批处理架构

```
┌─────────────────────────────────────────────────────────────────┐
│                  SIMDSpatialGrid::query()                       │
├─────────────────────────────────────────────────────────────────┤
│  1. 收集单元格内所有实体坐标到 SOA 数组                           │
│  2. SIMD 批量计算距离 (8 实体/批次)                              │
│  3. 生成结果掩码                                                 │
│  4. 压缩存储命中实体                                             │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 SOA (Structure of Arrays) 布局

```cpp
// 传统 AOS (Array of Structures)
struct GridEntry {
    float x, y;
    entt::entity entity;
};
std::vector<GridEntry> entries;  // entries[i].x, entries[i].y

// SOA 布局 - 对 SIMD 友好
struct SOAEntries {
    std::vector<float> x;      // 连续的 x 坐标
    std::vector<float> y;      // 连续的 y 坐标
    std::vector<entt::entity> entities;
};
// x[0], x[1], ..., x[7] 可以一次性加载到 AVX 寄存器
```

### 2.3 xsimd 核心算法

```cpp
#include <xsimd/xsimd.hpp>

using batch = xsimd::batch<float, xsimd::avx2>;
constexpr size_t SIMD_WIDTH = batch::size;  // 8 for AVX2

void SIMDSpatialGrid::queryBatch(
    const float* px, const float* py,  // 实体坐标 (SOA)
    size_t count,
    float cx, float cy, float radiusSq,
    std::vector<size_t>& outIndices
) {
    batch center_x(cx);
    batch center_y(cy);
    batch radius_sq(radiusSq);
    
    for (size_t i = 0; i < count; i += SIMD_WIDTH) {
        // 加载 8 个实体坐标
        batch ex = batch::load_aligned(px + i);
        batch ey = batch::load_aligned(py + i);
        
        // 计算距离平方
        batch dx = ex - center_x;
        batch dy = ey - center_y;
        batch dist_sq = dx * dx + dy * dy;
        
        // 比较生成掩码
        auto mask = dist_sq <= radius_sq;
        
        // 压缩存储命中索引
        alignas(32) bool hits[SIMD_WIDTH];
        mask.store_aligned(hits);
        
        for (size_t j = 0; j < SIMD_WIDTH && (i + j) < count; ++j) {
            if (hits[j]) {
                outIndices.push_back(i + j);
            }
        }
    }
}
```

### 2.4 内存对齐要求

```cpp
// SOA 数组必须 32 字节对齐 (AVX2 要求)
struct alignas(32) SIMDSpatialGrid {
    std::vector<float, xsimd::aligned_allocator<float, 32>> m_x;
    std::vector<float, xsimd::aligned_allocator<float, 32>> m_y;
    std::vector<entt::entity> m_entities;
};
```

---

## 3. 数据结构变更

### 3.1 新增 SIMDSpatialGrid 类

```cpp
// src/engine/physics/SIMDSpatialGrid.hpp

class SIMDSpatialGrid {
public:
    void resize(int width, int height, float cellSize);
    
    // 重建网格，将 AOS 转换为 SOA
    template<typename View>
    void rebuild(const View& view, const entt::registry& registry);
    
    // SIMD 加速查询
    template<typename Func>
    void query(const Position& pos, float radius, Func&& callback) const;
    
    // Fallback 标量查询 (用于小批量或调试)
    template<typename Func>
    void queryScalar(const Position& pos, float radius, Func&& callback) const;
    
private:
    float m_cellSize;
    size_t m_bucketCount;
    
    // SOA 存储
    std::vector<float, AlignedAllocator<float>> m_x;
    std::vector<float, AlignedAllocator<float>> m_y;
    std::vector<uint32_t> m_cellHash;
    std::vector<entt::entity> m_entities;
    
    // 桶索引
    std::vector<uint32_t> m_buckets;
    
    // 临时结果缓冲
    mutable std::vector<size_t> m_hitBuffer;
};
```

### 3.2 与现有 SpatialHashGrid 的关系

```
SpatialHashGrid (原有)
├── AOS 布局
├── 标量查询
└── 保留作为 Fallback

SIMDSpatialGrid (新增)
├── SOA 布局  
├── SIMD 批处理
└── 继承相同的哈希算法
```

---

## 4. 使用模式

### 4.1 ProjectileSystem 集成

```cpp
void ProjectileSystem::Update(entt::registry& reg, float dt) {
    auto& grid = m_simdGrid;  // 或通过依赖注入
    
    // 重建网格 (敌人)
    auto enemyView = reg.view<EnemyTag, Position>();
    grid.rebuild(enemyView, reg);
    
    // 对每个投射物进行碰撞检测
    auto projView = reg.view<Projectile, Position>();
    for (auto [entity, proj, pos] : projView.each()) {
        grid.query(pos, proj.radius, [&](entt::entity enemy, const Position& epos) {
            // 精确碰撞检测和伤害处理
            OnProjectileHit(entity, enemy);
        });
    }
}
```

### 4.2 AISystem 集成

```cpp
void AISystem::FindNearbyEnemies(entt::registry& reg, entt::entity self, float range) {
    auto& pos = reg.get<Position>(self);
    
    m_nearbyBuffer.clear();
    m_simdGrid.query(pos, range, [&](entt::entity other, const Position&) {
        if (other != self) {
            m_nearbyBuffer.push_back(other);
        }
    });
    
    return m_nearbyBuffer;
}
```

---

## 5. 实现计划

### Task 4.1: 创建 SIMDSpatialGrid 核心类
**文件**: `src/engine/physics/SIMDSpatialGrid.hpp`

### Task 4.2: 实现 SOA 布局和内存对齐

### Task 4.3: 实现 SIMD 批查询算法

### Task 4.4: 集成到 ProjectileSystem

### Task 4.5: 集成到 AISystem

### Task 4.6: 创建 Benchmark 对比测试

---

## 6. 验收标准

| 指标 | 基准 (标量) | 目标 (SIMD) |
|------|-------------|-------------|
| 1000 次查询耗时 | ~0.5ms | < 0.1ms |
| 吞吐量 | ~100M dist/s | > 500M dist/s |
| 正确性 | 100% | 100% |

---

## 7. 风险与缓解

| 风险 | 缓解策略 |
|------|----------|
| 指令集不支持 | xsimd 自动选择最佳可用指令集 (SSE4.2/AVX2/AVX512) |
| 内存对齐问题 | 使用 `xsimd::aligned_allocator` |
| SOA 转换开销 | 在 rebuild 中完成，摊销到所有查询 |

---

*设计者: Gemini (Skill: designer)*
