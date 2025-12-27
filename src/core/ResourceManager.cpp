#include "ResourceManager.hpp"
#include "../tools/Logger.hpp"

ResourceManager::~ResourceManager() {
    unloadAll();
}

Texture2D ResourceManager::loadTexture(entt::id_type id, const std::string& path) {
    // Check if already loaded
    if (m_textures.find(id) != m_textures.end()) {
        return m_textures[id];
    }

    if (!FileExists(path.c_str())) {
        LOG_ERROR("ResourceManager: File not found: {}", path);
        return { 0 };
    }

    Texture2D tex = LoadTexture(path.c_str());
    m_textures[id] = tex;
    // Note: We can't log the string 'name' easily unless we store it, but path is sufficient.
    LOG_INFO("ResourceManager: Loaded texture (ID: {}) from '{}'", id, path);
    return tex;
}

Texture2D ResourceManager::getTexture(entt::id_type id) {
    if (m_textures.find(id) != m_textures.end()) {
        return m_textures[id];
    }
    LOG_WARN("ResourceManager: Texture ID {} not found!", id);
    return { 0 };
}

void ResourceManager::unloadAll() {
    for (auto& [id, tex] : m_textures) {
        UnloadTexture(tex);
    }
    m_textures.clear();
    LOG_INFO("ResourceManager: All resources unloaded.");
}
