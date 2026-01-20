#include "game/systems/item/MaterialRegistry.hpp"
#include <fstream>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace NoMoreDay {

MaterialRegistry& MaterialRegistry::Get() {
    static MaterialRegistry instance;
    return instance;
}

void MaterialRegistry::LoadMaterials(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("Failed to open material data file: {}", path);
        return;
    }

    nlohmann::json data;
    try {
        file >> data;
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::error("Failed to parse material data JSON: {}", e.what());
        return;
    }

    materialMap_.clear();
    allMaterials_.clear();

    if (!data.contains("materials") || !data["materials"].is_array()) {
         spdlog::error("Invalid material data format: 'materials' array missing.");
         return;
    }

    for (const auto& item : data["materials"]) {
        MaterialDefinition def;
        def.id = item.value("id", 0u);
        def.name = item.value("name", "Unknown Material");
        def.description = item.value("description", "");
        def.category = item.value("category", "Misc");
        def.categoryEnum = static_cast<MaterialCategory>(item.value("category_id", 0));
        
        static const std::unordered_map<std::string, Rarity> kStringToRarity = {
            {"Common", Rarity::Common},
            {"Magic", Rarity::Magic},
            {"Rare", Rarity::Rare},
            {"Uncommon", Rarity::Uncommon},
            {"Epic", Rarity::Epic},
            {"Legendary", Rarity::Legendary},
            {"Mythic", Rarity::Mythic},
            {"Ancient", Rarity::Ancient},
            {"Set", Rarity::Set}
        };

        std::string rarityStr = item.value("rarity", "Common");
        auto it = kStringToRarity.find(rarityStr);
        if (it != kStringToRarity.end()) {
            def.rarity = it->second;
        } else {
            def.rarity = Rarity::Common;
        }

        def.icon = item.value("icon", "");
        def.maxStack = item.value("max_stack", 9999);

        materialMap_[def.id] = def;
        allMaterials_.push_back(def);
    }

    spdlog::info("Loaded {} materials from {}", allMaterials_.size(), path);
}

const MaterialDefinition* MaterialRegistry::GetMaterial(uint32_t id) const {
    auto it = materialMap_.find(id);
    if (it != materialMap_.end()) {
        return &it->second;
    }
    return nullptr;
}

const std::vector<MaterialDefinition>& MaterialRegistry::GetAllMaterials() const {
    return allMaterials_;
}

} // namespace NoMoreDay
