#include "ResourceManager.hpp"
#include "../tools/Logger.hpp"

ResourceManager::~ResourceManager() {
    LOG_INFO("Shutting down ResourceManager...");
    unloadAll();
    LOG_INFO("ResourceManager shutdown completed");
}

Texture2D ResourceManager::loadTexture(entt::id_type id, const std::string& path) {
    LOG_TRACE("Loading texture with ID: {}, path: {}", id, path);
    // Check if already loaded
    if (auto it = m_textures.find(id); it != m_textures.end()) {
        LOG_DEBUG("Texture already loaded, returning cached texture (ID: {})", id);
        return it->second;
    }

    if (!FileExists(path.c_str())) {
        LOG_ERROR("ResourceManager: File not found: {}", path);
        return { 0 };
    }

    Texture2D tex = LoadTexture(path.c_str());
    if (tex.id == 0) {
        LOG_ERROR("ResourceManager: Failed to load texture from '{}', invalid texture ID", path);
        return { 0 };
    }
    m_textures.emplace(id, tex);
    // Note: We can't log the string 'name' easily unless we store it, but path is sufficient.
    LOG_INFO("ResourceManager: Loaded texture (ID: {}) from '{}'", id, path);
    return tex;
}

Texture2D ResourceManager::getTexture(entt::id_type id) {
    LOG_TRACE("Getting texture with ID: {}", id);
    if (auto it = m_textures.find(id); it != m_textures.end()) {
        LOG_DEBUG("Found texture in cache (ID: {})", id);
        return it->second;
    }
    LOG_WARN("ResourceManager: Texture ID {} not found!", id);
    return { 0 };
}

Font ResourceManager::loadFont(entt::id_type id, const std::string& path, int fontSize, int* codepoints, int codepointCount) {
    LOG_TRACE("Loading font with ID: {}, path: {}", id, path);
    if (auto it = m_fonts.find(id); it != m_fonts.end()) {
        return it->second;
    }

    if (!FileExists(path.c_str())) {
        LOG_ERROR("ResourceManager: Font file not found: {}", path);
        return GetFontDefault();
    }

    Font font;
    if (codepoints && codepointCount > 0) {
        LOG_TRACE("ResourceManager: Loading font with {} codepoints", codepointCount);
        font = LoadFontEx(path.c_str(), fontSize, codepoints, codepointCount);
    } else {
        LOG_TRACE("ResourceManager: Loading font with default codepoints");
        font = LoadFont(path.c_str());
    }

    if (font.texture.id == 0) {
        LOG_ERROR("ResourceManager: Failed to load font from '{}'", path);
        return { 0 }; // Return empty font to allow caller to handle it
    }

    m_fonts.emplace(id, font);
    LOG_INFO("ResourceManager: Loaded font (ID: {}) from '{}'", id, path);
    return font;
}

Font ResourceManager::getFont(entt::id_type id) {
    if (auto it = m_fonts.find(id); it != m_fonts.end()) {
        return it->second;
    }
    return GetFontDefault();
}

void ResourceManager::unloadAll() {
    LOG_DEBUG("Unloading all textures, count: {}", m_textures.size());
    for (auto& [id, tex] : m_textures) {
        if (tex.id != 0) {
            UnloadTexture(tex);
            LOG_TRACE("Unloaded texture with ID: {}", id);
        }
    }
    m_textures.clear();

    LOG_DEBUG("Unloading all fonts, count: {}", m_fonts.size());
    for (auto& [id, font] : m_fonts) {
        if (font.texture.id != 0) {
            UnloadFont(font);
            LOG_TRACE("Unloaded font with ID: {}", id);
        }
    }
    m_fonts.clear();

    LOG_INFO("ResourceManager: All resources unloaded.");
}
