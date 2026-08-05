#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
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

    // Shader ownership contract:
    //   Shaders returned by loadShader / loadComputeShader are OWNED by the
    //   ResourceManager: it registered the GL program with the GPU resource
    //   registry and it is the SOLE entity allowed to release them (raylib
    //   UnloadShader) during unloadAll() / the destructor. Consumers BORROW
    //   these Shader objects and must never call UnloadShader on them —
    //   doing so double-frees shader.locs (heap corruption) when the manager
    //   later tears down. A consumer that needs an early release must call
    //   ReleaseShader(id) instead of touching GL directly.
    Shader loadShader(entt::id_type id, const std::string& vsPath, const std::string& fsPath);
    // 加载 Compute Shader (see ownership contract above)
    Shader loadComputeShader(entt::id_type id, const std::string& path);
    Shader getShader(entt::id_type id);

    // Releases the shader owned under `id` (registry unregister + GL release,
    // synchronized, then removed from the manager cache). Fail-safe and
    // idempotent: an unknown/never-loaded id returns false and is a no-op.
    // After this call the Shader must be considered dangling — callers must
    // drop their references immediately. Returns true when a shader was
    // actually released (recorded in the release ledger).
    bool ReleaseShader(entt::id_type id);

    // Ownership-release ledger: monotonically increasing count of shader
    // releases performed by this manager (headless dummy releases included —
    // GL is skipped, ownership release is still recorded) plus the ordered
    // list of released ids. Tests use this to prove exactly-once teardown.
    [[nodiscard]] size_t GetShaderReleaseCount() const;
    [[nodiscard]] std::vector<entt::id_type> GetShaderReleaseIds() const;

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
    // Single release choke point (must be called with m_mutex held). Erases the
    // entry, unregisters the registry record BEFORE the GL release (RG-3
    // observer pairing), performs the raylib UnloadShader (skipped for
    // headless dummies), and records the release in the ledger.
    bool ReleaseShaderLocked(entt::id_type id);

    std::unordered_map<entt::id_type, Texture2D> m_textures;
    std::unordered_map<entt::id_type, std::string> m_texturePaths; // 用于按需加载的路径注册表
    std::unordered_map<entt::id_type, Font> m_fonts;
    std::unordered_map<entt::id_type, Shader> m_shaders;
    unsigned int m_entityTextureArray = 0;
    std::unordered_map<std::string, int> m_textureLayerMap; // name -> layer index
    bool m_headless = false;
    mutable std::shared_mutex m_mutex;
    size_t m_shaderReleaseCount = 0;
    std::vector<entt::id_type> m_shaderReleaseIds;
};
