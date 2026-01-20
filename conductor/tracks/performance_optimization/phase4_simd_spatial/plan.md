# Phase 4: SIMD SpatialGrid Query 实施计划

**Track ID**: `performance_optimization/phase4_simd_spatial`  
**状态**: 📋 Planned  
**预计工时**: 2 天

---

## 任务分解 (Task Breakdown)

### Task 4.1: 创建 SIMDSpatialGrid 类骨架 ⬜
**优先级**: Critical  
**预计时间**: 2h

**操作**:
1. 创建 `src/engine/physics/SIMDSpatialGrid.hpp`
2. 定义类接口和 SOA 成员变量
3. 使用 `xsimd::aligned_allocator` 保证内存对齐

**代码框架**:
```cpp
#pragma once
#include <xsimd/xsimd.hpp>
#include <entt/entt.hpp>
#include <vector>
#include "game/components/Common.hpp"

namespace NoMoreDay::systems {

template<typename T, size_t Align = 32>
using AlignedVector = std::vector<T, xsimd::aligned_allocator<T, Align>>;

class SIMDSpatialGrid {
public:
    SIMDSpatialGrid(int width, int height, float cellSize);
    
    template<typename View>
    void rebuild(const View& view, const entt::registry& reg);
    
    template<typename Func>
    void query(const Position& center, float radius, Func&& callback) const;
    
private:
    float m_cellSize;
    size_t m_bucketCount;
    
    // SOA 布局
    AlignedVector<float> m_x;
    AlignedVector<float> m_y;
    std::vector<uint32_t> m_cellHash;
    std::vector<entt::entity> m_entities;
    
    std::vector<uint32_t> m_buckets;
    
    uint32_t getHash(float x, float y) const;
    uint32_t getHashFromGrid(int gx, int gy) const;
};

} // namespace
```

**验收条件**:
- [ ] 编译通过
- [ ] xsimd 正确链接

---

### Task 4.2: 实现 rebuild() SOA 转换 ⬜
**优先级**: Critical  
**预计时间**: 1.5h

**操作**:
1. 从 View 收集所有实体的位置
2. 分离存储到 SOA 数组
3. 按 cellHash 排序
4. 构建桶索引

**代码**:
```cpp
template<typename View>
void SIMDSpatialGrid::rebuild(const View& view, const entt::registry& reg) {
    m_x.clear();
    m_y.clear();
    m_cellHash.clear();
    m_entities.clear();
    
    // 1. 收集数据
    view.each([&](auto entity, const auto& pos) {
        m_x.push_back(pos.x);
        m_y.push_back(pos.y);
        m_cellHash.push_back(getHash(pos.x, pos.y));
        m_entities.push_back(entity);
    });
    
    // 2. 按 cellHash 排序 (保持 SOA 同步)
    std::vector<size_t> indices(m_entities.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return m_cellHash[a] < m_cellHash[b];
    });
    
    // 3. 重排 SOA 数据
    reorderByIndices(indices);
    
    // 4. 构建桶
    buildBuckets();
    
    // 5. 对齐填充到 SIMD 宽度的倍数
    padToSimdWidth();
}
```

**注意**: 需要保证数组大小是 SIMD 宽度 (8) 的倍数，尾部填充无效数据。

---

### Task 4.3: 实现 SIMD query() 核心算法 ⬜
**优先级**: Critical  
**预计时间**: 3h

**操作**:
1. 实现批量距离计算
2. 实现掩码生成和压缩
3. 调用用户回调

**代码**:
```cpp
template<typename Func>
void SIMDSpatialGrid::query(const Position& center, float radius, Func&& callback) const {
    using batch = xsimd::batch<float, xsimd::best_arch>;
    constexpr size_t W = batch::size;
    
    float radiusSq = radius * radius;
    batch cx(center.x);
    batch cy(center.y);
    batch rsq(radiusSq);
    
    // 遍历相关单元格
    int minGx = floor((center.x - radius) / m_cellSize);
    int maxGx = floor((center.x + radius) / m_cellSize);
    int minGy = floor((center.y - radius) / m_cellSize);
    int maxGy = floor((center.y + radius) / m_cellSize);
    
    for (int gy = minGy; gy <= maxGy; ++gy) {
        for (int gx = minGx; gx <= maxGx; ++gx) {
            uint32_t hash = getHashFromGrid(gx, gy);
            uint32_t start = m_buckets[hash];
            if (start == ~0u) continue;
            
            // 找到该单元格的结束位置
            size_t end = start;
            while (end < m_cellHash.size() && m_cellHash[end] == hash) ++end;
            
            // SIMD 批处理
            for (size_t i = start; i < end; i += W) {
                batch ex = batch::load_aligned(&m_x[i]);
                batch ey = batch::load_aligned(&m_y[i]);
                
                batch dx = ex - cx;
                batch dy = ey - cy;
                batch distSq = dx * dx + dy * dy;
                
                auto mask = distSq <= rsq;
                
                // 提取命中结果
                alignas(32) bool hits[W];
                for (size_t j = 0; j < W; ++j) {
                    hits[j] = mask.get(j);
                }
                
                for (size_t j = 0; j < W && (i + j) < end; ++j) {
                    if (hits[j]) {
                        callback(m_entities[i + j], Position{m_x[i+j], m_y[i+j]});
                    }
                }
            }
        }
    }
}
```

---

### Task 4.4: 集成到 ProjectileSystem ⬜
**优先级**: High  
**预计时间**: 1.5h

**操作**:
1. 在 `ProjectileSystem` 中添加 `SIMDSpatialGrid` 成员
2. 替换现有空间查询调用
3. 保留原有实现作为对比

---

### Task 4.5: 集成到 AISystem ⬜
**优先级**: Medium  
**预计时间**: 1.5h

**操作**:
1. 在 `AISystem` 中使用 `SIMDSpatialGrid`
2. 优化 `FindNearbyTargets` 查询

---

### Task 4.6: 创建 SIMDSpatialGridTest ⬜
**优先级**: High  
**预计时间**: 1h

**操作**:
1. 创建 `tests/unit/SIMDSpatialGridTest.hpp`
2. 测试场景:
   - 正确性: 与标量实现结果一致
   - 边界: 空网格、单实体、刚好在边界
   - 对齐: 非 8 倍数的实体数量

---

### Task 4.7: Benchmark 对比测试 ⬜
**优先级**: High  
**预计时间**: 1h

**操作**:
1. 创建 `benchmarks/SpatialGridBenchmark.cpp`
2. 对比场景:
   - 1000 实体, 1000 次查询
   - 10000 实体, 100 次查询
3. 输出性能对比表

---

## 依赖关系

```
Task 4.1 ──► Task 4.2 ──► Task 4.3
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
         Task 4.4                        Task 4.5
              │                               │
              └───────────────┬───────────────┘
                              ▼
                         Task 4.6 ──► Task 4.7
```

---

## 验收清单

- [ ] `SIMDSpatialGrid` 类实现完成
- [ ] SOA 转换和内存对齐正确
- [ ] SIMD 查询结果与标量一致
- [ ] `ProjectileSystem` 使用 SIMD 网格
- [ ] `AISystem` 使用 SIMD 网格
- [ ] `SIMDSpatialGridTest` 通过
- [ ] Benchmark 显示 >= 3x 性能提升

---

## 回滚计划

若遇到严重问题:
1. 将系统调用切回 `SpatialHashGrid`
2. `SIMDSpatialGrid` 代码保留待修复

---

*规划者: Gemini (Skill: designer)*
