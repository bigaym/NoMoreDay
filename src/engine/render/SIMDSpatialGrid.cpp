#include "engine/render/SIMDSpatialGrid.hpp"
#include <limits>

namespace NoMoreDay::systems {

SIMDSpatialGrid::SIMDSpatialGrid(int width, int height, float cellSize)
    : m_cellSize(cellSize), m_gridWidth(width), m_gridHeight(height)
{
    // Width and Height are expected to be Grid Dimensions (Cols, Rows)
    m_bucketCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (m_bucketCount == 0) m_bucketCount = 1;
    m_buckets.resize(m_bucketCount, ~0u);
}

uint32_t SIMDSpatialGrid::getHash(float x, float y) const {
    int gx = static_cast<int>(std::floor(x / m_cellSize));
    int gy = static_cast<int>(std::floor(y / m_cellSize));
    return getHashFromGrid(gx, gy);
}

uint32_t SIMDSpatialGrid::getHashFromGrid(int gx, int gy) const {
    // Clamp to valid grid range
    if (gx < 0) gx = 0;
    if (gx >= m_gridWidth) gx = m_gridWidth - 1;
    if (gy < 0) gy = 0;
    if (gy >= m_gridHeight) gy = m_gridHeight - 1;
    
    return static_cast<uint32_t>(gy * m_gridWidth + gx);
}

void SIMDSpatialGrid::buildBuckets() {
    // Reset buckets
    std::fill(m_buckets.begin(), m_buckets.end(), ~0u);
    
    // m_cellHash is sorted.
    // Iterate and mark start of each hash sequence.
    for (size_t i = 0; i < m_cellHash.size(); ++i) {
        uint32_t h = m_cellHash[i];
        if (h == ~0u) continue; // Skip padding

        if (h < m_buckets.size()) {
            // If this bucket is empty (~0u), set start index
            // Since it's sorted, the first time we see 'h', it's the start.
            if (m_buckets[h] == ~0u) {
                m_buckets[h] = static_cast<uint32_t>(i);
            }
        }
    }
}

} // namespace NoMoreDay::systems
