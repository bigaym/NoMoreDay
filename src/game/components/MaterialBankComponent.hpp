#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace NoMoreDay {

struct MaterialEntry {
    uint32_t id;
    int32_t count;
    
    // For sorting
    bool operator<(const MaterialEntry& other) const {
        return id < other.id;
    }
    bool operator==(const MaterialEntry& other) const {
        return id == other.id;
    }
};

inline void to_json(nlohmann::json& j, const MaterialEntry& e) {
    j = nlohmann::json{{"id", e.id}, {"count", e.count}};
}
inline void from_json(const nlohmann::json& j, MaterialEntry& e) {
    j.at("id").get_to(e.id);
    j.at("count").get_to(e.count);
}

struct MaterialBankComponent {
    std::vector<MaterialEntry> materials;

    /**
     * @brief Adds material to the bank.
     * @param materialId ID of the material.
     * @param amount Amount to add.
     * @return New total count of the material.
     */
    int32_t Add(uint32_t materialId, int32_t amount) {
        if (amount <= 0) return GetCount(materialId);

        auto it = std::lower_bound(materials.begin(), materials.end(), MaterialEntry{materialId, 0});
        if (it != materials.end() && it->id == materialId) {
            it->count += amount;
            return it->count;
        } else {
            materials.insert(it, MaterialEntry{materialId, amount});
            return amount;
        }
    }

    /**
     * @brief Removes material from the bank.
     * @param materialId ID of the material.
     * @param amount Amount to remove.
     * @return True if successful (enough materials), False otherwise (no change made).
     */
    bool Remove(uint32_t materialId, int32_t amount) {
        if (amount <= 0) return true;

        auto it = std::lower_bound(materials.begin(), materials.end(), MaterialEntry{materialId, 0});
        if (it != materials.end() && it->id == materialId) {
            if (it->count >= amount) {
                it->count -= amount;
                if (it->count == 0) {
                    materials.erase(it);
                }
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Gets the count of a specific material.
     * @param materialId ID of the material.
     * @return Current count, or 0 if not found.
     */
    int32_t GetCount(uint32_t materialId) const {
        auto it = std::lower_bound(materials.begin(), materials.end(), MaterialEntry{materialId, 0});
        if (it != materials.end() && it->id == materialId) {
            return it->count;
        }
        return 0;
    }
    
    /**
     * @brief Checks if the bank has at least the specified amount of material.
     * @param materialId ID of the material.
     * @param amount Amount required.
     * @return True if has enough, False otherwise.
     */
    bool Has(uint32_t materialId, int32_t amount) const {
        return GetCount(materialId) >= amount;
    }
};

inline void to_json(nlohmann::json& j, const MaterialBankComponent& c) {
    j = nlohmann::json{{"materials", c.materials}};
}
inline void from_json(const nlohmann::json& j, MaterialBankComponent& c) {
    if (j.contains("materials")) {
        j.at("materials").get_to(c.materials);
        // Ensure sorted after loading just in case the JSON was manually edited or corrupted
        std::sort(c.materials.begin(), c.materials.end());
    }
}

} // namespace NoMoreDay
