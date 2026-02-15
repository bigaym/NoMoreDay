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
    // 1. Collect Data into local buffers
    size_t size_hint = 0;
    if constexpr (requires { view.size_hint(); }) {
        size_hint = view.size_hint();
    } else if constexpr (requires { view.size(); }) {
        size_hint = view.size();
    }

    std::vector<float> tempX; tempX.reserve(size_hint);
    std::vector<float> tempY; tempY.reserve(size_hint);
    std::vector<uint32_t> tempHash; tempHash.reserve(size_hint);
    std::vector<entt::entity> tempEntities; tempEntities.reserve(size_hint);

    for (auto entity : view) {
        const auto& pos = view.template get<Position>(entity);
        tempX.push_back(pos.x);
        tempY.push_back(pos.y);
        tempHash.push_back(getHash(pos.x, pos.y));
        tempEntities.push_back(entity);
    }

    if (tempEntities.empty()) {
        m_x.clear();
        m_y.clear();
        m_cellHash.clear();
        m_entities.clear();
        m_buckets.assign(m_bucketCount, ~0u);
        return;
    }

    // 2. Sort by Cell Hash
    std::vector<size_t> indices(tempEntities.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return tempHash[a] < tempHash[b];
    });

    // 3. Reorder and Pad to SIMD width
    using batch = xsimd::batch<float>;
    constexpr size_t W = batch::size;
    // Calculate padded size and add one extra batch for safety (Total Padding >= W)
    size_t paddedSize = ((tempEntities.size() + W - 1) / W) * W + W;

    AlignedVector<float> newX; newX.reserve(paddedSize);
    AlignedVector<float> newY; newY.reserve(paddedSize);
    std::vector<uint32_t> newHash; newHash.reserve(paddedSize);
    std::vector<entt::entity> newEntities; newEntities.reserve(paddedSize);

    for (size_t i : indices) {
        newX.push_back(tempX[i]);
        newY.push_back(tempY[i]);
        newHash.push_back(tempHash[i]);
        newEntities.push_back(tempEntities[i]);
    }

    // Fill padding
    while (newX.size() < paddedSize) {
        newX.push_back(std::numeric_limits<float>::infinity());
        newY.push_back(std::numeric_limits<float>::infinity());
        newHash.push_back(~0u);
        newEntities.push_back(entt::null);
    }

    // Atomic-ish update of state
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