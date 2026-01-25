#include "engine/resource/ResourceManager.hpp"
#include "core/logging/Logger.hpp"
#include "rlgl.h"
#include "GLFW/glfw3.h"

// OpenGL 4.3 constants for Texture Arrays
#ifndef GL_TEXTURE_2D_ARRAY
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#endif

#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif

#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE 0x1401
#endif

#ifndef GL_TEXTURE_WRAP_S
#define GL_TEXTURE_WRAP_S 0x2802
#endif

#ifndef GL_TEXTURE_WRAP_T
#define GL_TEXTURE_WRAP_T 0x2803
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef GL_TEXTURE_MIN_FILTER
#define GL_TEXTURE_MIN_FILTER 0x2801
#endif

#ifndef GL_TEXTURE_MAG_FILTER
#define GL_TEXTURE_MAG_FILTER 0x2800
#endif

#ifndef GL_LINEAR
#define GL_LINEAR 0x2601
#endif

#ifndef APIENTRY
    #if defined(_WIN32)
        #define APIENTRY __stdcall
    #else
        #define APIENTRY
    #endif
#endif

typedef void (APIENTRY *PFNGLTEXSTORAGE3DPROC)(unsigned int target, int levels, unsigned int internalformat, int width, int height, int depth);
typedef void (APIENTRY *PFNGLTEXSUBIMAGE3DPROC)(unsigned int target, int level, int xoffset, int yoffset, int zoffset, int width, int height, int depth, unsigned int format, unsigned int type, const void *pixels);
typedef void (APIENTRY *PFNGLGENTEXTURESPROC)(int n, unsigned int *textures);
typedef void (APIENTRY *PFNGLBINDTEXTUREPROC)(unsigned int target, unsigned int texture);
typedef void (APIENTRY *PFNGLTEXPARAMETERIPROC)(unsigned int target, unsigned int pname, int param);
typedef void (APIENTRY *PFNGLACTIVETEXTUREPROC)(unsigned int texture);

static PFNGLTEXSTORAGE3DPROC glTexStorage3D_ptr = nullptr;
static PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D_ptr = nullptr;
static PFNGLGENTEXTURESPROC glGenTextures_ptr = nullptr;
static PFNGLBINDTEXTUREPROC glBindTexture_ptr = nullptr;
static PFNGLTEXPARAMETERIPROC glTexParameteri_ptr = nullptr;
static PFNGLACTIVETEXTUREPROC glActiveTexture_ptr = nullptr;

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

unsigned int ResourceManager::loadTextureArray(const std::vector<std::string>& paths) {
    if (paths.empty()) return 0;

    const int layerWidth = 128;
    const int layerHeight = 128;
    const int layerCount = static_cast<int>(paths.size());

    if (m_headless) {
        for (int i = 0; i < layerCount; i++) {
            std::string name = GetFileNameWithoutExt(paths[i].c_str());
            m_textureLayerMap[name] = i;
        }
        m_entityTextureArray = 1;
        return 1;
    }

    // Load function pointers if not already loaded
    if (glTexStorage3D_ptr == nullptr) {
        glTexStorage3D_ptr = (PFNGLTEXSTORAGE3DPROC)glfwGetProcAddress("glTexStorage3D");
        glTexSubImage3D_ptr = (PFNGLTEXSUBIMAGE3DPROC)glfwGetProcAddress("glTexSubImage3D");
        glGenTextures_ptr = (PFNGLGENTEXTURESPROC)glfwGetProcAddress("glGenTextures");
        glBindTexture_ptr = (PFNGLBINDTEXTUREPROC)glfwGetProcAddress("glBindTexture");
        glTexParameteri_ptr = (PFNGLTEXPARAMETERIPROC)glfwGetProcAddress("glTexParameteri");
        glActiveTexture_ptr = (PFNGLACTIVETEXTUREPROC)glfwGetProcAddress("glActiveTexture");
        
        if (!glTexStorage3D_ptr || !glTexSubImage3D_ptr || !glGenTextures_ptr || !glBindTexture_ptr) {
            LOG_ERROR("ResourceManager: Failed to load OpenGL 4.3 function pointers");
            return 0;
        }
    }

    // Create the Texture Array using raw GL to avoid raylib side effects
    unsigned int texArrayId = 0;
    glGenTextures_ptr(1, &texArrayId);
    if (texArrayId == 0) return 0;

    glBindTexture_ptr(GL_TEXTURE_2D_ARRAY, texArrayId);
    glTexStorage3D_ptr(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, layerWidth, layerHeight, layerCount);

    // Set sampling parameters IMMEDIATELY after storage allocation
    glTexParameteri_ptr(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri_ptr(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri_ptr(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri_ptr(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    LOG_INFO("ResourceManager: Created Texture Array ID: {} ({}x{}x{})", texArrayId, layerWidth, layerHeight, layerCount);

    for (int i = 0; i < layerCount; i++) {
        const std::string& path = paths[i];
        if (!FileExists(path.c_str())) {
            LOG_WARN("ResourceManager: Sprite file not found for texture array: {}", path);
            continue;
        }

        Image img = LoadImage(path.c_str());
        if (img.data == nullptr) {
            LOG_WARN("ResourceManager: Failed to load image: {}", path);
            continue;
        }

        // Standardize image
        if (img.width != layerWidth || img.height != layerHeight) {
            ImageResize(&img, layerWidth, layerHeight);
        }
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        // Upload to specific layer
        glTexSubImage3D_ptr(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, layerWidth, layerHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE, img.data);
        
        // Store mapping
        std::string name = GetFileNameWithoutExt(path.c_str());
        m_textureLayerMap[name] = i;
        
        UnloadImage(img);
        LOG_TRACE("ResourceManager: Loaded layer {} for texture array: {}", i, name);
    }

    glBindTexture_ptr(GL_TEXTURE_2D_ARRAY, 0);
    m_entityTextureArray = texArrayId;
    LOG_INFO("ResourceManager: Loaded Texture Array with {} layers (ID: {})", layerCount, texArrayId);
    return texArrayId;
}

int ResourceManager::getTextureLayerIndex(const std::string& name) const {
    if (auto it = m_textureLayerMap.find(name); it != m_textureLayerMap.end()) {
        return it->second;
    }
    return -1;
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

    if (m_entityTextureArray != 0) {
        if (!m_headless) rlUnloadTexture(m_entityTextureArray);
        m_entityTextureArray = 0;
    }
    m_textureLayerMap.clear();

    LOG_INFO("ResourceManager: All resources unloaded.");
}