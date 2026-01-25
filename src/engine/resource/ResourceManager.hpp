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

    Shader loadShader(entt::id_type id, const std::string& vsPath, const std::string& fsPath);
    // 加载 Compute Shader
    Shader loadComputeShader(entt::id_type id, const std::string& path);
    Shader getShader(entt::id_type id);

    // 加载多个纹理到一个 GL_TEXTURE_2D_ARRAY
    // paths: 纹理文件路径列表 (按顺序对应 layer index 0, 1, 2...)
    unsigned int loadTextureArray(const std::vector<std::string>& paths);
    
    // 获取已加载的实体纹理数组
    unsigned int getEntityTextureArray() const { return m_entityTextureArray; }
    
    // 获取指定纹理名称对应的 Layer Index (-1 if not found)
    int getTextureLayerIndex(const std::string& name) const;

    void unloadAll();

    // Headless Mode Support (For Testing)
    void SetHeadless(bool headless) { m_headless = headless; }
    bool IsHeadless() const { return m_headless; }

private:
    std::unordered_map<entt::id_type, Texture2D> m_textures;
    std::unordered_map<entt::id_type, std::string> m_texturePaths; // 用于按需加载的路径注册表
    std::unordered_map<entt::id_type, Font> m_fonts;
    std::unordered_map<entt::id_type, Shader> m_shaders;
    unsigned int m_entityTextureArray = 0;
    std::unordered_map<std::string, int> m_textureLayerMap; // name -> layer index
    bool m_headless = false;
};
