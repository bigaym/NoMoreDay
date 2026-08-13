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
        LOG_ERROR("Failed to open material data file: {}", path);
        return;
    }

    nlohmann::json data;
    try {
        file >> data;
    } catch (const nlohmann::json::parse_error& e) {
        LOG_ERROR("Failed to parse material data JSON: {}", e.what());
        return;
    }

    materialMap_.clear();
    allMaterials_.clear();

    if (!data.contains("materials") || !data["materials"].is_array()) {
         LOG_ERROR("Invalid material data format: 'materials' array missing.");
         return;
    }

    for (const auto& item : data["materials"]) {
        MaterialDefinition def;
        def.id = item.value("id", 0u);
        def.name = item.value("name", "Unknown Material");
        def.description = item.value("description", "");
        def.category = item.value("category", "Misc");
        def.categoryEnum = static_cast<MaterialCategory>(item.value("category_id", 0));

        // 稀有度解析统一走 ItemComponent.hpp 的 RarityFromString (大小写不敏感单源)
        def.rarity = RarityFromString(item.value("rarity", "Common"));

        def.icon = item.value("icon", "");
        def.maxStack = item.value("max_stack", 9999);

        materialMap_[def.id] = def;
        allMaterials_.push_back(def);
    }

    LOG_INFO("Loaded {} materials from {}", allMaterials_.size(), path);
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
