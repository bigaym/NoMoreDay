#pragma once
#include <string>
#include <unordered_map>
#include <entt/entt.hpp>
#include "raylib.h"

class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager();

    // 如果尚未加载，则从磁盘加载纹理。
    // 用法: loadTexture("sword"_hs, "path/...")
    Texture2D loadTexture(entt::id_type id, const std::string& path);
    
    // 注册纹理路径以便按需加载 (Lazy Loading)
    void registerTexture(entt::id_type id, const std::string& path);

    // 获取已加载的纹理。如果未找到，则返回空纹理。
    Texture2D getTexture(entt::id_type id);

    // 如果尚未加载，则从磁盘加载字体
    Font loadFont(entt::id_type id, const std::string& path, int fontSize, int* codepoints = nullptr, int codepointCount = 0);

    // 获取已加载的字体
    Font getFont(entt::id_type id);

    void unloadAll();

private:
    std::unordered_map<entt::id_type, Texture2D> m_textures;
    std::unordered_map<entt::id_type, std::string> m_texturePaths; // 用于按需加载的路径注册表
    std::unordered_map<entt::id_type, Font> m_fonts;
};
