#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include "game/components/ItemComponent.hpp" 

namespace NoMoreDay {

struct MaterialDefinition {
    uint32_t id;
    std::string name;
    std::string description;
    std::string category;
    Rarity rarity;
    std::string icon;
    int maxStack;
};

class MaterialRegistry {
public:
    static MaterialRegistry& Get();

    void LoadMaterials(const std::string& path);
    const MaterialDefinition* GetMaterial(uint32_t id) const;
    const std::vector<MaterialDefinition>& GetAllMaterials() const;

private:
    MaterialRegistry() = default;
    
    std::unordered_map<uint32_t, MaterialDefinition> materialMap_;
    std::vector<MaterialDefinition> allMaterials_; 
};

} // namespace NoMoreDay
