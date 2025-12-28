#include "ResourceManager.hpp"
#include "../tools/Logger.hpp"

ResourceManager::~ResourceManager() {
    LOG_INFO("Shutting down ResourceManager...");
    unloadAll();
    LOG_INFO("ResourceManager shutdown completed");
}

Texture2D ResourceManager::loadTexture(entt::id_type id, const std::string& path) { // 加载纹理
 LOG_TRACE("正在加载纹理，ID: {}，路径: {}", id, path);
    // 检查是否已加载
    if (auto it = m_textures.find(id); it != m_textures.end()) {
 LOG_DEBUG("纹理已加载，返回缓存纹理 (ID: {})", id);
        return it->second;
    }

    if (!FileExists(path.c_str())) {
 LOG_ERROR("ResourceManager: 文件未找到: {}", path);
        return { 0 };
    }

    Texture2D tex = LoadTexture(path.c_str());
    if (tex.id == 0) {
 LOG_ERROR("ResourceManager: 从 '{}' 加载纹理失败，无效纹理ID", path);
        return { 0 };
    }
    m_textures.emplace(id, tex);
    // Note: We can't log the string 'name' easily unless we store it, but path is sufficient.
    LOG_INFO("ResourceManager: Loaded texture (ID: {}) from '{}'", id, path);
    return tex;
}

Texture2D ResourceManager::getTexture(entt::id_type id) { // 获取纹理
 LOG_TRACE("正在获取纹理，ID: {}", id);
    if (auto it = m_textures.find(id); it != m_textures.end()) {
 LOG_DEBUG("在缓存中找到纹理 (ID: {})", id);
        return it->second;
    }
 LOG_WARN("ResourceManager: 未找到纹理ID {}！", id);
    return { 0 };
}

Font ResourceManager::loadFont(entt::id_type id, const std::string& path, int fontSize, int* codepoints, int codepointCount) { // 加载字体
 LOG_TRACE("正在加载字体，ID: {}，路径: {}", id, path);
    if (auto it = m_fonts.find(id); it != m_fonts.end()) {
        return it->second;
    }

    if (!FileExists(path.c_str())) {
 LOG_ERROR("ResourceManager: 未找到字体文件: {}", path);
        return GetFontDefault();
    }

    Font font;
    if (codepoints && codepointCount > 0) {
 LOG_TRACE("ResourceManager: 正在加载包含 {} 个码点的字体", codepointCount);
        font = LoadFontEx(path.c_str(), fontSize, codepoints, codepointCount);
    } else {
 LOG_TRACE("ResourceManager: 正在加载包含默认码点的字体");
        font = LoadFont(path.c_str());
    }

    if (font.texture.id == 0) { // 如果字体纹理ID为0，则加载失败
 LOG_ERROR("ResourceManager: 从 '{}' 加载字体失败", path);
        return { 0 }; // 返回空字体，允许调用者处理
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

void ResourceManager::unloadAll() { // 卸载所有资源
 LOG_DEBUG("正在卸载所有纹理，数量: {}", m_textures.size());
    for (auto& [id, tex] : m_textures) {
        if (tex.id != 0) {
            UnloadTexture(tex);
 LOG_TRACE("已卸载纹理，ID: {}", id);
        }
    }
    m_textures.clear();

 LOG_DEBUG("正在卸载所有字体，数量: {}", m_fonts.size());
    for (auto& [id, font] : m_fonts) {
        if (font.texture.id != 0) {
            UnloadFont(font);
 LOG_TRACE("已卸载字体，ID: {}", id);
        }
    }
    m_fonts.clear();

    LOG_INFO("ResourceManager: All resources unloaded.");
}
