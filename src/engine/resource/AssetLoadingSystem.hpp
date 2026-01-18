#pragma once
#include <entt/entt.hpp>
#include "engine/resource/ResourceManager.hpp"
#include <string>
#include <vector>

namespace NoMoreDay {

class AssetLoadingSystem {
public:
    static void Initialize(ResourceManager& resourceManager);
    
    // 加载字体并存储以供UI使用
    static Font LoadUIFont(const std::string& path, int fontSize);
    
    // 加载UI纹理并返回其ID
    static Texture2D LoadUITexture(entt::id_type id, const std::string& path);

    // 如果已加载，则通过ID获取纹理
    static Texture2D GetTexture(entt::id_type id);

    // Load all equipment assets defined in EquipmentAssetRegistry
    static void LoadAllEquipment();

    // Register all standard UI textures (slots, icons, etc.)
    static void RegisterUITextures();

    // Register and load core shaders
    static void RegisterShaders();

    // Register all rune assets
    static void RegisterRunes();

    static void Shutdown();

private:
    static ResourceManager* m_resourceManager;
    static std::vector<Font> m_loadedFonts;
};

} // namespace NoMoreDay
