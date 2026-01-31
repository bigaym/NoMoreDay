#pragma once

#include <xsimd/xsimd.hpp>
#include <entt/entt.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include "game/components/Common.hpp"
#include <numeric>

namespace NoMoreDay::systems {

class SIMDSpatialGrid {
public:
    // Aligned vector alias for SIMD operations (32-byte alignment for AVX2)
    template<typename T>
    using AlignedVector = std::vector<T, xsimd::aligned_allocator<T, 32>>;

    SIMDSpatialGrid(int width, int height, float cellSize);
    ~SIMDSpatialGrid() = default;

    // Rebuild the grid from an entity view
    template<typename View>
    void rebuild(const View& view, const entt::registry& reg);

    // Query the grid for entities within radius
    // Callback: bool(entt::entity, const Vector2& pos) - return false to stop query
    template<typename Func>
    void query(const Position& center, float radius, Func&& callback) const;

private:
    float m_cellSize;
    int m_gridWidth;
    int m_gridHeight;
    size_t m_bucketCount;

    // SOA Data
    AlignedVector<float> m_x;
    AlignedVector<float> m_y;
    std::vector<uint32_t> m_cellHash;
    std::vector<entt::entity> m_entities;

    std::vector<uint32_t> m_buckets;

    // Helpers
    uint32_t getHash(float x, float y) const;
    uint32_t getHashFromGrid(int gx, int gy) const;
    void buildBuckets();
};

// --- Template Implementations ---

template<typename View>
void SIMDSpatialGrid::rebuild(const View& view, const entt::registry& reg) {
    m_x.clear();
    m_y.clear();
    m_cellHash.clear();
    m_entities.clear();

    // 1. Collect Data - Reserve if possible
    size_t size = 0;
    if constexpr (requires { view.size_hint(); }) {
        size = view.size_hint();
    } else if constexpr (requires { view.size(); }) {
        size = view.size();
    }
    
    if (size > 0) {
        m_x.reserve(size);
        m_y.reserve(size);
        m_cellHash.reserve(size);
        m_entities.reserve(size);
    }

    for (auto entity : view) {
        const auto& pos = view.template get<Position>(entity);
        m_x.push_back(pos.x);
        m_y.push_back(pos.y);
        m_cellHash.push_back(getHash(pos.x, pos.y));
        m_entities.push_back(entity);
    }

    if (m_entities.empty()) return;

    // 2. Sort by Cell Hash
    std::vector<size_t> indices(m_entities.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return m_cellHash[a] < m_cellHash[b];
    });

    // 3. Reorder SOA data
    AlignedVector<float> newX; newX.reserve(m_entities.size());
    AlignedVector<float> newY; newY.reserve(m_entities.size());
    std::vector<uint32_t> newHash; newHash.reserve(m_entities.size());
    std::vector<entt::entity> newEntities; newEntities.reserve(m_entities.size());

    for (size_t i : indices) {
        newX.push_back(m_x[i]);
        newY.push_back(m_y[i]);
        newHash.push_back(m_cellHash[i]);
        newEntities.push_back(m_entities[i]);
    }

    // Pad to SIMD width
    using batch = xsimd::batch<float>;
    constexpr size_t W = batch::size;
    
    while (newX.size() % W != 0) {
        newX.push_back(std::numeric_limits<float>::infinity());
        newY.push_back(std::numeric_limits<float>::infinity());
        newHash.push_back(~0u);
        newEntities.push_back(entt::null);
    }

    m_x = std::move(newX);
    m_y = std::move(newY);
    m_cellHash = std::move(newHash);
    m_entities = std::move(newEntities);

    // 4. Build Buckets
    buildBuckets();
}

template<typename Func>
void SIMDSpatialGrid::query(const Position& center, float radius, Func&& callback) const {
    if (m_entities.empty()) return;

    using batch = xsimd::batch<float>;
    constexpr size_t W = batch::size;

    float radiusSq = radius * radius;
    batch cx(center.x);
    batch cy(center.y);
    batch rsq(radiusSq);

    int minGx = static_cast<int>(std::floor((center.x - radius) / m_cellSize));
    int maxGx = static_cast<int>(std::floor((center.x + radius) / m_cellSize));
    int minGy = static_cast<int>(std::floor((center.y - radius) / m_cellSize));
    int maxGy = static_cast<int>(std::floor((center.y + radius) / m_cellSize));

    // Clamp to grid bounds to prevent duplicate cell checks due to hash clamping
    minGx = std::max(0, minGx);
    maxGx = std::min(m_gridWidth - 1, maxGx);
    minGy = std::max(0, minGy);
    maxGy = std::min(m_gridHeight - 1, maxGy);

    for (int gy = minGy; gy <= maxGy; ++gy) {
        for (int gx = minGx; gx <= maxGx; ++gx) {
            uint32_t hash = getHashFromGrid(gx, gy);
            
            if (hash >= m_buckets.size()) continue;
            uint32_t start = m_buckets[hash];
            if (start == ~0u) continue;

            size_t current = start;
            size_t end = start;
            while (end < m_cellHash.size() && m_cellHash[end] == hash) {
                end++;
            }

            // SIMD Process
            for (size_t i = current; i < end; i += W) {
                batch ex = batch::load_unaligned(&m_x[i]);
                batch ey = batch::load_unaligned(&m_y[i]);
                
                batch dx = ex - cx;
                batch dy = ey - cy;
                batch distSq = dx * dx + dy * dy;
                
                auto mask = distSq <= rsq;
                
                if (xsimd::any(mask)) {
                    for (size_t j = 0; j < W; ++j) {
                        size_t idx = i + j;
                        if (idx < end && mask.get(j)) {
                            if (!callback(m_entities[idx], {m_x[idx], m_y[idx]})) return;
                        }
                    }
                }
            }
        }
    }
}

} // namespace NoMoreDay::systems