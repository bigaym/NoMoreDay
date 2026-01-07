#include "ResourceManager.hpp"
#include "../tools/Logger.hpp"
#include "rlgl.h"

ResourceManager::~ResourceManager() {
    LOG_INFO("Shutting down ResourceManager...");
    unloadAll();
    LOG_INFO("ResourceManager shutdown completed");
}

Texture2D ResourceManager::loadTexture(entt::id_type id, const std::string& path) {
    LOG_TRACE("Loading texture, ID: {}, Path: {}", id, path);
    if (auto it = m_textures.find(id); it != m_textures.end()) {
        return it->second;
    }

    if (m_headless) {
        LOG_WARN("ResourceManager (Headless): Returning dummy texture for '{}'", path);
        Texture2D dummy = { 1, 32, 32, 1, 7 }; 
        m_textures.emplace(id, dummy);
        return dummy;
    }

    if (!FileExists(path.c_str())) {
        LOG_ERROR("ResourceManager: File not found: {}", path);
        return { 0 };
    }

    Texture2D tex = LoadTexture(path.c_str());
    if (tex.id == 0) {
        LOG_ERROR("ResourceManager: Failed to load texture from '{}'", path);
        return { 0 };
    }
    m_textures.emplace(id, tex);
    LOG_TRACE("ResourceManager: Loaded texture (ID: {}) from '{}'", id, path);
    return tex;
}

void ResourceManager::registerTexture(entt::id_type id, const std::string& path) {
    m_texturePaths[id] = path;
}

Texture2D ResourceManager::getTexture(entt::id_type id) {
    if (id == 0) return { 0 };

    if (auto it = m_textures.find(id); it != m_textures.end()) {
        return it->second;
    }

    if (auto it = m_texturePaths.find(id); it != m_texturePaths.end()) {
        return loadTexture(id, it->second);
    }

    return { 0 };
}

Font ResourceManager::loadFont(entt::id_type id, const std::string& path, int fontSize, int* codepoints, int codepointCount) {
    if (auto it = m_fonts.find(id); it != m_fonts.end()) {
        return it->second;
    }

    if (m_headless) {
        LOG_WARN("ResourceManager (Headless): Returning dummy font for '{}'", path);
        Font dummy = { 0 };
        dummy.baseSize = fontSize;
        dummy.glyphCount = 0;
        dummy.texture = { 1, 32, 32, 1, 7 }; 
        m_fonts.emplace(id, dummy);
        return dummy;
    }

    if (!FileExists(path.c_str())) {
        LOG_ERROR("ResourceManager: Font file not found: {}", path);
        return GetFontDefault();
    }

    Font font;
    if (codepoints && codepointCount > 0) {
        font = LoadFontEx(path.c_str(), fontSize, codepoints, codepointCount);
    } else {
        font = LoadFont(path.c_str());
    }

    if (font.texture.id == 0) {
        LOG_ERROR("ResourceManager: Failed to load font from '{}'", path);
        return { 0 };
    }

    m_fonts.emplace(id, font);
    LOG_INFO("ResourceManager: Loaded font (ID: {}) from '{}'", id, path);
    return font;
}

Font ResourceManager::getFont(entt::id_type id) {
    if (m_fonts.find(id) != m_fonts.end()) {
        return m_fonts[id];
    }
    return GetFontDefault();
}

Shader ResourceManager::loadShader(entt::id_type id, const std::string& vsPath, const std::string& fsPath) {
    if (m_shaders.find(id) != m_shaders.end()) {
        return m_shaders[id];
    }

    if (m_headless) {
        LOG_WARN("ResourceManager (Headless): Returning dummy shader for '{}'", fsPath);
        Shader dummy = { 0 };
        dummy.id = 1;
        m_shaders[id] = dummy;
        return dummy;
    }

    Shader shader = LoadShader(vsPath.c_str(), fsPath.c_str());
    m_shaders[id] = shader;
    return shader;
}

Shader ResourceManager::loadComputeShader(entt::id_type id, const std::string& path) {
    if (auto it = m_shaders.find(id); it != m_shaders.end()) return it->second;

    if (m_headless) {
        LOG_WARN("ResourceManager (Headless): Returning dummy compute shader for '{}'", path);
        Shader dummy = { 0 };
        dummy.id = 1;
        m_shaders[id] = dummy;
        return dummy;
    }

    if (!FileExists(path.c_str())) {
        LOG_ERROR("ResourceManager: Compute shader file not found: {}", path);
        return { 0 };
    }

    char* source = LoadFileText(path.c_str());
    if (source == nullptr) return { 0 };

    // Unified rlgl loading
    unsigned int shaderId = rlCompileShader(source, RL_COMPUTE_SHADER);
    unsigned int programId = 0;
    
    if (shaderId != 0) {
        programId = rlLoadComputeShaderProgram(shaderId);
    }

    UnloadFileText(source);

    if (programId == 0) {
        LOG_ERROR("ResourceManager: Failed to load compute shader program from '{}'", path);
        return { 0 };
    }

    Shader shader = { 0 };
    shader.id = programId;
    shader.locs = (int*)RL_MALLOC(32 * sizeof(int));
    for (int i = 0; i < 32; i++) shader.locs[i] = -1;

    m_shaders[id] = shader;
    LOG_INFO("ResourceManager: Loaded compute shader (ID: {}) from '{}'", id, path);
    return shader;
}

Shader ResourceManager::getShader(entt::id_type id) {
    if (auto it = m_shaders.find(id); it != m_shaders.end()) return it->second;
    return { 0 };
}

void ResourceManager::unloadAll() {
    for (auto& [id, tex] : m_textures) {
        if (tex.id != 0 && !m_headless) UnloadTexture(tex);
    }
    m_textures.clear();
    m_texturePaths.clear();

    for (auto& [id, font] : m_fonts) {
        if (font.texture.id != 0 && !m_headless) UnloadFont(font);
    }
    m_fonts.clear();

    for (auto& [id, shader] : m_shaders) {
        if (shader.id != 0 && !m_headless) UnloadShader(shader);
    }
    m_shaders.clear();

    LOG_INFO("ResourceManager: All resources unloaded.");
}