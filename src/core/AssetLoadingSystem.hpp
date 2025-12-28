#pragma once
#include <entt/entt.hpp>
#include "ResourceManager.hpp"
#include <string>
#include <vector>

namespace NoMoreDay {

class AssetLoadingSystem {
public:
    static void Initialize(ResourceManager& resourceManager);
    
    // Loads a font and stores it for UI use
    static Font LoadUIFont(const std::string& path, int fontSize);
    
    // Loads a UI texture and returns its ID
    static Texture2D LoadUITexture(entt::id_type id, const std::string& path);

    // Gets a texture by ID if it's already loaded
    static Texture2D GetTexture(entt::id_type id);

    static void Shutdown();

private:
    static ResourceManager* m_resourceManager;
    static std::vector<Font> m_loadedFonts;
};

} // namespace NoMoreDay
