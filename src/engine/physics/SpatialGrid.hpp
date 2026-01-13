#pragma once

#include "game/components/Common.hpp"
#include <algorithm>
#include <cmath>
#include <entt/entt.hpp>
#include <limits>
#include <vector>


namespace NoMoreDay {
namespace systems {

class SpatialHashGrid {
public:
  struct GridEntry {
    uint32_t cellHash; // 单元格哈希值
    entt::entity entity;
    Position pos; // 缓存位置以避免查询时的 registry.get

    // 用于排序的重载
    bool operator<(const GridEntry &other) const {
      return cellHash < other.cellHash;
    }
  };

  // 配置
  // 单元格大小应略大于最大代理直径
  SpatialHashGrid(int width, int height, float cellSize)
      : m_cellSize(cellSize), m_bucketCount(width * height) {

    // 使用“无效”哨兵初始化桶
    m_buckets.resize(m_bucketCount, 0xFFFFFFFF);
    // 为1万个实体预留内存，以避免重新分配
    m_entries.reserve(20000);
  }

  void resize(int width, int height, float cellSize) {
    m_cellSize = cellSize;
    m_bucketCount = width * height;
    m_buckets.resize(m_bucketCount);
  }

  // “黑魔法”构建步骤
  // O(N log N) 但对缓存极其友好，每帧零分配
  template <typename View>
  void rebuild(const View &view, const entt::registry &registry) {
    // 1. 清除之前的数据（保留容量！）
    m_entries.clear();

    // 2. 并行就绪：计算所有实体的单元格哈希
    // 使用 view.each 自动处理组件获取
    view.each([&](auto entity, const auto &pos) {
      uint32_t hash = getHash(pos.x, pos.y);
      m_entries.push_back({hash, entity, pos});
    });

    // 3. 按单元格哈希排序
    // 这使得同一单元格中的实体在内存中是连续的
    std::sort(m_entries.begin(), m_entries.end());

    // 4. 重置桶
    // 使用 0xFFFFFFFF 作为“空”
    std::fill(m_buckets.begin(), m_buckets.end(), 0xFFFFFFFF);

    // 5. 构建起始索引（类似前缀和）
    for (size_t i = 0; i < m_entries.size(); ++i) {
      uint32_t hash = m_entries[i].cellHash;
      // 如果这是我们第一次看到此哈希，则记录索引
      if (m_buckets[hash] == 0xFFFFFFFF) {
        m_buckets[hash] = static_cast<uint32_t>(i);
      }
    }
  }

  // 高效查询
  // 回调函数：void(entt::entity neighbor)
  template <typename Func>
  void query(const Position &pos, float searchRadius, Func &&callback) const {
    // Determine search bounds in grid coordinates
    int minX =
        static_cast<int>(std::floor((pos.x - searchRadius) / m_cellSize));
    int maxX =
        static_cast<int>(std::floor((pos.x + searchRadius) / m_cellSize));
    int minY =
        static_cast<int>(std::floor((pos.y - searchRadius) / m_cellSize));
    int maxY =
        static_cast<int>(std::floor((pos.y + searchRadius) / m_cellSize));

    // 遍历相关单元格
    for (int y = minY; y <= maxY; ++y) {
      for (int x = minX; x <= maxX; ++x) {
        uint32_t hash = getHashFromGrid(x, y);

        // 检查桶是否为空
        uint32_t start = m_buckets[hash];
        if (start == 0xFFFFFFFF)
          continue;

        // 遍历此单元格中连续的实体
        for (size_t i = start; i < m_entries.size(); ++i) {
          const auto &entry = m_entries[i];

          // 如果离开当前单元格则停止（排序数组属性）
          if (entry.cellHash != hash)
            break;

          // 传递给回调函数（包括缓存的位置）
          callback(entry.entity, entry.pos);
        }
      }
    }
  }

private:
  float m_cellSize;
  size_t m_bucketCount; // 桶的数量

  // 用于数据导向设计的扁平数组
  std::vector<GridEntry> m_entries; // 存储 (哈希, 实体) 对
  std::vector<uint32_t> m_buckets;  // 存储每个哈希的起始索引

  // 简单的二维哈希
  uint32_t getHash(float x, float y) const {
    int gx = static_cast<int>(std::floor(x / m_cellSize));
    int gy = static_cast<int>(std::floor(y / m_cellSize));
    return getHashFromGrid(gx, gy);
  }

  // 空间哈希函数
  // 使用大素数来最小化冲突
  uint32_t getHashFromGrid(int gx, int gy) const {
    constexpr uint32_t p1 = 73856093;
    constexpr uint32_t p2 = 19349663;
    // Handle negative coordinates correctly for infinite world
    return ((static_cast<uint32_t>(gx) * p1) ^
            (static_cast<uint32_t>(gy) * p2)) %
           m_bucketCount;
  }
};

} // namespace systems
} // namespace NoMoreDay
