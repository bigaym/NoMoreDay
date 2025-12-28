#include "AssetLoadingSystem.hpp"
#include "../tools/Logger.hpp"

namespace NoMoreDay {

ResourceManager* AssetLoadingSystem::m_resourceManager = nullptr;
std::vector<Font> AssetLoadingSystem::m_loadedFonts;

void AssetLoadingSystem::Initialize(ResourceManager& resourceManager) {
    m_resourceManager = &resourceManager;
    LOG_INFO("AssetLoadingSystem initialized.");
}

Font AssetLoadingSystem::LoadUIFont(const std::string& path, int fontSize) {
    if (!m_resourceManager) {
        LOG_ERROR("AssetLoadingSystem: Not initialized!");
        return GetFontDefault();
    }
    
    // Using path hash as ID for UI fonts managed through this system
    entt::id_type id = entt::hashed_string(path.c_str());
    return m_resourceManager->loadFont(id, path, fontSize);
}

Texture2D AssetLoadingSystem::LoadUITexture(entt::id_type id, const std::string& path) {
    if (!m_resourceManager) {
        LOG_ERROR("AssetLoadingSystem: Not initialized!");
        return { 0 };
    }
    return m_resourceManager->loadTexture(id, path);
}

void AssetLoadingSystem::Shutdown() {
    m_resourceManager = nullptr;
    m_loadedFonts.clear();
    LOG_INFO("AssetLoadingSystem shutdown.");
}

} // namespace NoMoreDay
