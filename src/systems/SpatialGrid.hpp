#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <entt/entt.hpp>
#include "../components/Common.hpp"

namespace systems {

class SpatialHashGrid {
public:
    struct GridEntry {
        uint32_t cellHash;
        entt::entity entity;
        
        // Overload for sorting
        bool operator<(const GridEntry& other) const {
            return cellHash < other.cellHash;
        }
    };

    // Configuration
    // Cell size should be slightly larger than the largest agent diameter
    SpatialHashGrid(int width, int height, float cellSize) 
        : m_cellSize(cellSize), m_bucketCount(width * height) {
        
        // Initialize buckets with "Invalid" sentinel
        m_buckets.resize(m_bucketCount, 0xFFFFFFFF);
        // Reserve memory for 10k entities to avoid reallocs
        m_entries.reserve(20000); 
    }

    void resize(int width, int height, float cellSize) {
        m_cellSize = cellSize;
        m_bucketCount = width * height;
        m_buckets.resize(m_bucketCount);
    }

    // The "Black Magic" Build Step
    // O(N log N) but extremely cache friendly and zero-alloc per frame
    template<typename View>
    void rebuild(const View& view, const entt::registry& registry) {
        // 1. Clear previous data (Keep capacity!)
        m_entries.clear();
        
        // 2. Parallel-ready: Compute Cell Hash for all entities
        // Currently serial, but easy to parallelize with Taskflow
        for (auto entity : view) {
            const auto& pos = registry.get<Position>(entity);
            uint32_t hash = getHash(pos.x, pos.y);
            m_entries.push_back({hash, entity});
        }

        // 3. Sort by Cell Hash
        // This makes entities in the same cell contiguous in memory
        std::sort(m_entries.begin(), m_entries.end());

        // 4. Reset Buckets
        // Using 0xFFFFFFFF as "Empty"
        std::fill(m_buckets.begin(), m_buckets.end(), 0xFFFFFFFF);

        // 5. Build Start Indices (Prefix Sum-ish)
        for (size_t i = 0; i < m_entries.size(); ++i) {
            uint32_t hash = m_entries[i].cellHash;
            // If this is the first time we see this hash, record the index
            if (m_buckets[hash] == 0xFFFFFFFF) {
                m_buckets[hash] = static_cast<uint32_t>(i);
            }
        }
    }

    // Efficient Query
    // Callback: void(entt::entity neighbor)
    template<typename Func>
    void query(const Position& pos, float searchRadius, Func&& callback) {
        // Determine search bounds in grid coordinates
        int minX = static_cast<int>(std::floor((pos.x - searchRadius) / m_cellSize));
        int maxX = static_cast<int>(std::floor((pos.x + searchRadius) / m_cellSize));
        int minY = static_cast<int>(std::floor((pos.y - searchRadius) / m_cellSize));
        int maxY = static_cast<int>(std::floor((pos.y + searchRadius) / m_cellSize));

        // Iterate over relevant cells
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                uint32_t hash = getHashFromGrid(x, y);
                
                // Check if bucket is empty
                uint32_t start = m_buckets[hash];
                if (start == 0xFFFFFFFF) continue;

                // Iterate contiguous entities in this cell
                for (size_t i = start; i < m_entries.size(); ++i) {
                    const auto& entry = m_entries[i];
                    
                    // Stop if we leave the current cell (Sorted Array property)
                    if (entry.cellHash != hash) break;

                    // Pass to callback (Caller must check exact distance)
                    callback(entry.entity);
                }
            }
        }
    }

private:
    float m_cellSize;
    size_t m_bucketCount;

    // Flat Arrays for Data Oriented Design
    std::vector<GridEntry> m_entries;  // Stores (Hash, Entity) pairs
    std::vector<uint32_t> m_buckets;   // Stores start index for each hash

    // Simple 2D Hash
    uint32_t getHash(float x, float y) const {
        int gx = static_cast<int>(std::floor(x / m_cellSize));
        int gy = static_cast<int>(std::floor(y / m_cellSize));
        return getHashFromGrid(gx, gy);
    }

    // Spatial Hashing Function
    // Uses large primes to minimize collisions
    uint32_t getHashFromGrid(int gx, int gy) const {
        constexpr uint32_t p1 = 73856093;
        constexpr uint32_t p2 = 19349663;
        // Handle negative coordinates correctly for infinite world
        return ((static_cast<uint32_t>(gx) * p1) ^ (static_cast<uint32_t>(gy) * p2)) % m_bucketCount;
    }
};

} // namespace systems
