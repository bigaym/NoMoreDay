#include "engine/resource/ResourceManager.hpp"
#include "GLFW/glfw3.h"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/debug/ShaderReloadGovernance.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "rlgl.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

using namespace NoMoreDay;

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

namespace {

bool ReadTextFileUtf8(const std::filesystem::path &path, std::string &outText) {
  std::ifstream file(path, std::ios::in | std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  outText = buffer.str();
  return true;
}

bool PreprocessShaderIncludesImpl(const std::filesystem::path &path,
                                  std::string &outText,
                                  std::unordered_set<std::string> &includeStack,
                                  int depth) {
  if (depth > 32) {
    LOG_ERROR("ResourceManager: shader include nesting too deep: {}", path.string());
    return false;
  }

  const std::filesystem::path normalizedPath =
      std::filesystem::absolute(path).lexically_normal();
  const std::string key = normalizedPath.string();
  if (!includeStack.insert(key).second) {
    LOG_ERROR("ResourceManager: cyclic shader include detected: {}", key);
    return false;
  }

  std::string fileText;
  if (!ReadTextFileUtf8(normalizedPath, fileText)) {
    includeStack.erase(key);
    LOG_ERROR("ResourceManager: failed to read shader source '{}'", normalizedPath.string());
    return false;
  }

  std::istringstream input(fileText);
  std::string line;
  std::ostringstream output;
  while (std::getline(input, line)) {
    std::string trimmed = line;
    const size_t firstNonWs = trimmed.find_first_not_of(" \t");
    if (firstNonWs != std::string::npos) {
      trimmed = trimmed.substr(firstNonWs);
    }

    if (trimmed.rfind("#include \"", 0) == 0) {
      const size_t begin = std::string("#include \"").size();
      const size_t end = trimmed.find('"', begin);
      if (end == std::string::npos) {
        includeStack.erase(key);
        LOG_ERROR("ResourceManager: malformed include in '{}': {}",
                  normalizedPath.string(), line);
        return false;
      }
      const std::string includeRel = trimmed.substr(begin, end - begin);
      const std::filesystem::path includePath = normalizedPath.parent_path() / includeRel;

      std::string includeText;
      if (!PreprocessShaderIncludesImpl(includePath, includeText, includeStack, depth + 1)) {
        includeStack.erase(key);
        return false;
      }
      output << includeText << '\n';
      continue;
    }

    output << line << '\n';
  }

  outText = output.str();
  includeStack.erase(key);
  return true;
}

bool PreprocessShaderIncludes(const std::string &path, std::string &outText) {
  if (path.empty()) {
    outText.clear();
    return true;
  }
  std::unordered_set<std::string> includeStack;
  return PreprocessShaderIncludesImpl(std::filesystem::path(path), outText,
                                      includeStack, 0);
}

} // namespace

ResourceManager::~ResourceManager() {
  LOG_INFO("Shutting down ResourceManager...");
  unloadAll();
  LOG_INFO("ResourceManager shutdown completed");
}

Texture2D ResourceManager::loadTexture(entt::id_type id,
                                       const std::string &path) {
  LOG_TRACE("Loading texture, ID: {}, Path: {}", id, path);
  
  {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (auto it = m_textures.find(id); it != m_textures.end()) {
      return it->second;
    }
  }

  std::unique_lock<std::shared_mutex> lock(m_mutex);
  // Re-check after acquiring write lock
  if (auto it = m_textures.find(id); it != m_textures.end()) {
    return it->second;
  }

  if (m_headless) {
    LOG_WARN("ResourceManager (Headless): Returning dummy texture for '{}'",
             path);
    Texture2D dummy = {1, 32, 32, 1, 7};
    m_textures.emplace(id, dummy);
    return dummy;
  }

  if (!FileExists(path.c_str())) {
    LOG_ERROR("ResourceManager: File not found: {}", path);
    return {0};
  }

  Texture2D tex = LoadTexture(path.c_str());
  if (tex.id == 0) {
    LOG_ERROR("ResourceManager: Failed to load texture from '{}'", path);
    return {0};
  }
  m_textures.emplace(id, tex);
  LOG_TRACE("ResourceManager: Loaded texture (ID: {}) from '{}'", id, path);
  return tex;
}

void ResourceManager::registerTexture(entt::id_type id,
                                      const std::string &path) {
  std::unique_lock<std::shared_mutex> lock(m_mutex);
  m_texturePaths[id] = path;
}

Texture2D ResourceManager::getTexture(entt::id_type id) {
  if (id == 0)
    return {0};

  {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (auto it = m_textures.find(id); it != m_textures.end()) {
      return it->second;
    }

    if (m_texturePaths.find(id) == m_texturePaths.end()) {
      return {0};
    }
  }

  // Need to load - path exists but texture doesn't
  std::string path;
  {
      std::shared_lock<std::shared_mutex> lock(m_mutex);
      path = m_texturePaths[id];
  }
  return loadTexture(id, path);
}

Font ResourceManager::loadFont(entt::id_type id, const std::string &path,
                               int fontSize, int *codepoints,
                               int codepointCount) {
  {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (auto it = m_fonts.find(id); it != m_fonts.end()) {
      return it->second;
    }
  }

  std::unique_lock<std::shared_mutex> lock(m_mutex);
  if (auto it = m_fonts.find(id); it != m_fonts.end()) {
    return it->second;
  }

  if (m_headless) {
    LOG_WARN("ResourceManager (Headless): Returning dummy font for '{}'", path);
    Font dummy = {0};
    dummy.baseSize = fontSize;
    dummy.glyphCount = 0;
    dummy.texture = {1, 32, 32, 1, 7};
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
    return {0};
  }

  m_fonts.emplace(id, font);
  LOG_INFO("ResourceManager: Loaded font (ID: {}) from '{}'", id, path);
  return font;
}

Font ResourceManager::getFont(entt::id_type id) {
  std::shared_lock<std::shared_mutex> lock(m_mutex);
  if (m_fonts.find(id) != m_fonts.end()) {
    return m_fonts.at(id);
  }
  return GetFontDefault();
}

Shader ResourceManager::loadShader(entt::id_type id, const std::string &vsPath,
                                   const std::string &fsPath) {
  {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (m_shaders.find(id) != m_shaders.end()) {
      return m_shaders.at(id);
    }
  }

  std::unique_lock<std::shared_mutex> lock(m_mutex);
  if (m_shaders.find(id) != m_shaders.end()) {
    return m_shaders.at(id);
  }

  if (m_headless) {
    LOG_WARN("ResourceManager (Headless): Returning dummy shader for '{}'",
             fsPath);
    Shader dummy = {0};
    dummy.id = 1;
    m_shaders[id] = dummy;
    return dummy;
  }

  std::string vertexSource;
  std::string fragmentSource;
  const char *vertexSourcePtr = nullptr;
  const char *fragmentSourcePtr = nullptr;

  if (!vsPath.empty()) {
    if (!PreprocessShaderIncludes(vsPath, vertexSource)) {
      LOG_ERROR("ResourceManager: Failed to preprocess vertex shader '{}'", vsPath);
      return {0};
    }
    vertexSourcePtr = vertexSource.c_str();
  }

  if (!fsPath.empty()) {
    if (!PreprocessShaderIncludes(fsPath, fragmentSource)) {
      LOG_ERROR("ResourceManager: Failed to preprocess fragment shader '{}'", fsPath);
      return {0};
    }
    fragmentSourcePtr = fragmentSource.c_str();
  }

  Shader shader = LoadShaderFromMemory(vertexSourcePtr, fragmentSourcePtr);
  if (shader.id == 0) {
    LOG_ERROR("ResourceManager: Failed to load shader VS='{}' FS='{}'", vsPath, fsPath);
    if (!vsPath.empty()) {
      std::vector<std::string> vsIncludeChain;
      const uint64_t vsHash =
          NoMoreDay::render::debug::ShaderReloadGovernance::Get().ComputeIncludeHash(
              vsPath, vsIncludeChain);
      NoMoreDay::render::debug::ShaderReloadGovernance::Get().RecordReloadAttempt(
          vsPath, false, vsHash, vsIncludeChain, "VS/FS", vsPath,
          "LoadShaderFromMemory failed");
    }
    if (!fsPath.empty()) {
      std::vector<std::string> fsIncludeChain;
      const uint64_t fsHash =
          NoMoreDay::render::debug::ShaderReloadGovernance::Get().ComputeIncludeHash(
              fsPath, fsIncludeChain);
      NoMoreDay::render::debug::ShaderReloadGovernance::Get().RecordReloadAttempt(
          fsPath, false, fsHash, fsIncludeChain, "VS/FS", fsPath,
          "LoadShaderFromMemory failed");
    }
    return {0};
  }

  // RG-3 (observer-only): register the VS/FS program with the GPU resource
  // registry; ResourceManager (unloadAll) remains the sole GL releaser.
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      shader.id, NoMoreDay::render::graph::ResourceKind::ShaderProgram,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u,
      "ResourceManagerShader");

  // RenderDoc 可读性: 用 shader 路径 basename 命名 program
  // (显示为 "Program 192 (mdi_render)" 之类)。
  {
    const char *namePath = (!vsPath.empty()) ? vsPath.c_str() : fsPath.c_str();
    const std::string programLabel =
        NoMoreDay::utils::GPUUtils::BaseNameNoExt(namePath);
    NoMoreDay::utils::GPUUtils::LabelProgram(shader.id, programLabel.c_str());
  }

  // F-group contract: VS/FS load attempts are recorded by ShaderReloadGovernance.
  if (!vsPath.empty()) {
    std::vector<std::string> vsIncludeChain;
    const uint64_t vsHash =
        NoMoreDay::render::debug::ShaderReloadGovernance::Get().ComputeIncludeHash(
            vsPath, vsIncludeChain);
    NoMoreDay::render::debug::ShaderReloadGovernance::Get().RecordReloadAttempt(
        vsPath, true, vsHash, vsIncludeChain, "VS/FS", vsPath, "");
  }
  if (!fsPath.empty()) {
    std::vector<std::string> fsIncludeChain;
    const uint64_t fsHash =
        NoMoreDay::render::debug::ShaderReloadGovernance::Get().ComputeIncludeHash(
            fsPath, fsIncludeChain);
    NoMoreDay::render::debug::ShaderReloadGovernance::Get().RecordReloadAttempt(
        fsPath, true, fsHash, fsIncludeChain, "VS/FS", fsPath, "");
  }

  m_shaders[id] = shader;
  return shader;
}

Shader ResourceManager::loadComputeShader(entt::id_type id,
                                          const std::string &path) {
  {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (auto it = m_shaders.find(id); it != m_shaders.end())
      return it->second;
  }

  std::unique_lock<std::shared_mutex> lock(m_mutex);
  if (auto it = m_shaders.find(id); it != m_shaders.end())
    return it->second;

  if (m_headless) {
    LOG_WARN(
        "ResourceManager (Headless): Returning dummy compute shader for '{}'",
        path);
    Shader dummy = {0};
    dummy.id = 1;
    m_shaders[id] = dummy;
    return dummy;
  }

  if (!FileExists(path.c_str())) {
    LOG_ERROR("ResourceManager: Compute shader file not found: {}", path);
    return {0};
  }

  std::string source;
  if (!PreprocessShaderIncludes(path, source)) {
    LOG_ERROR("ResourceManager: Failed to preprocess compute shader '{}'", path);
    return {0};
  }
  if (source.empty()) {
    LOG_ERROR("ResourceManager: Empty compute shader source '{}'", path);
    return {0};
  }

  // Unified rlgl loading
  unsigned int shaderId = rlCompileShader(source.c_str(), RL_COMPUTE_SHADER);
  unsigned int programId = 0;

  if (shaderId != 0) {
    programId = rlLoadComputeShaderProgram(shaderId);
  }

  std::vector<std::string> includeChain;
  uint64_t hash = NoMoreDay::render::debug::ShaderReloadGovernance::Get().ComputeIncludeHash(path, includeChain);
  NoMoreDay::render::debug::ShaderReloadGovernance::Get().RecordReloadAttempt(
      path, (programId != 0), hash, includeChain, "ComputeShader", path,
      (programId != 0) ? "" : "rlLoadComputeShaderProgram failed");

  if (programId == 0) {
    LOG_ERROR(
        "ResourceManager: Failed to load compute shader program from '{}'",
        path);
    return {0};
  }

  Shader shader = {0};
  shader.id = programId;
  shader.locs = (int *)RL_MALLOC(32 * sizeof(int));
  for (int i = 0; i < 32; i++)
    shader.locs[i] = -1;

  // RG-3 (observer-only): register the compute program with the GPU resource
  // registry; ResourceManager (unloadAll) remains the sole GL releaser.
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      shader.id, NoMoreDay::render::graph::ResourceKind::ShaderProgram,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u,
      "ResourceManagerComputeShader");

  // RenderDoc 可读性: 用 compute shader 路径 basename 命名 program。
  {
    const std::string programLabel =
        NoMoreDay::utils::GPUUtils::BaseNameNoExt(path.c_str());
    NoMoreDay::utils::GPUUtils::LabelProgram(shader.id, programLabel.c_str());
  }

  m_shaders[id] = shader;
  LOG_INFO("ResourceManager: Loaded compute shader (ID: {}) from '{}'", id,
           path);
  return shader;
}

Shader ResourceManager::getShader(entt::id_type id) {
  std::shared_lock<std::shared_mutex> lock(m_mutex);
  if (auto it = m_shaders.find(id); it != m_shaders.end())
    return it->second;
  return {0};
}

bool ResourceManager::ReleaseShader(entt::id_type id) {
  std::unique_lock<std::shared_mutex> lock(m_mutex);
  return ReleaseShaderLocked(id);
}

bool ResourceManager::ReleaseShaderLocked(entt::id_type id) {
  auto it = m_shaders.find(id);
  if (it == m_shaders.end()) {
    return false;
  }

  Shader shader = it->second;
  m_shaders.erase(it); // Erase first: any subsequent release attempt no-ops.

  if (shader.id != 0) {
    if (!m_headless) {
      // RG-3: unregister the registry record BEFORE the GL release so the
      // observer never outlives the backing it tracks.
      NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
          shader.id, NoMoreDay::render::graph::ResourceKind::ShaderProgram);
      UnloadShader(shader);
    }
    // Ownership-release ledger: recorded for every release (headless dummies
    // included — GL is skipped there, ownership release is still counted).
    m_shaderReleaseCount++;
    m_shaderReleaseIds.push_back(id);
  }

  return true;
}

size_t ResourceManager::GetShaderReleaseCount() const {
  std::shared_lock<std::shared_mutex> lock(m_mutex);
  return m_shaderReleaseCount;
}

std::vector<entt::id_type> ResourceManager::GetShaderReleaseIds() const {
  std::shared_lock<std::shared_mutex> lock(m_mutex);
  return m_shaderReleaseIds;
}

unsigned int

ResourceManager::loadTextureArray(const std::vector<std::string> &paths) {

  if (paths.empty())

    return 0;



  std::unique_lock<std::shared_mutex> lock(m_mutex);



  const int layerWidth = 128;

  const int layerHeight = 128;

  const int layerCount = static_cast<int>(paths.size());



  if (m_headless) {

        for (int i = 0; i < layerCount; i++) {

          std::string name = std::filesystem::path(paths[i]).stem().string();

          m_textureLayerMap[name] = i;

        }

    m_entityTextureArray = 1;

    return 1;

  }



  // Create the Texture Array using unified GPUUtils

  unsigned int texArrayId = 0;

  utils::GPUUtils::GenTextures(1, &texArrayId);

  if (texArrayId == 0)

    return 0;



  utils::GPUUtils::BindTexture(GL_TEXTURE_2D_ARRAY, texArrayId);

  utils::GPUUtils::TexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, layerWidth,

                                layerHeight, layerCount);



  // Set sampling parameters

  utils::GPUUtils::TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,

                                 GL_LINEAR);

  utils::GPUUtils::TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER,

                                 GL_LINEAR);

  utils::GPUUtils::TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S,

                                 GL_CLAMP_TO_EDGE);

  utils::GPUUtils::TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T,

                                 GL_CLAMP_TO_EDGE);



  LOG_INFO("ResourceManager: Created Texture Array ID: {} ({}x{}x{})",

           texArrayId, layerWidth, layerHeight, layerCount);



  for (int i = 0; i < layerCount; i++) {

    const std::string &path = paths[i];

    if (!FileExists(path.c_str())) {

      LOG_WARN("ResourceManager: Sprite file not found for texture array: {}",

               path);

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

    utils::GPUUtils::TexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, layerWidth,

                                   layerHeight, 1, GL_RGBA, GL_UNSIGNED_BYTE,

                                   img.data);



        // Store mapping



        std::string name = std::filesystem::path(path).stem().string();



        m_textureLayerMap[name] = i;



    UnloadImage(img);

    LOG_TRACE("ResourceManager: Loaded layer {} for texture array: {}", i,

              name);

  }



  utils::GPUUtils::BindTexture(GL_TEXTURE_2D_ARRAY, 0);

  m_entityTextureArray = texArrayId;

  LOG_INFO("ResourceManager: Loaded Texture Array with {} layers (ID: {})",

           layerCount, texArrayId);

  return texArrayId;

}



int ResourceManager::getTextureLayerIndex(const std::string &name) const {

  std::shared_lock<std::shared_mutex> lock(m_mutex);

  if (auto it = m_textureLayerMap.find(name); it != m_textureLayerMap.end()) {

    return it->second;

  }

  return -1;

}



void ResourceManager::unloadAll() {

  std::unique_lock<std::shared_mutex> lock(m_mutex);

  for (auto &[id, tex] : m_textures) {

    if (tex.id != 0 && !m_headless)

      UnloadTexture(tex);

  }

  m_textures.clear();

  m_texturePaths.clear();



  for (auto &[id, font] : m_fonts) {

    if (font.texture.id != 0 && !m_headless)

      UnloadFont(font);

  }

  m_fonts.clear();



  // All shaders are released through the single ReleaseShaderLocked choke
  // point so the ownership-release ledger records exactly-once teardown.
  std::vector<entt::id_type> shaderIds;

  shaderIds.reserve(m_shaders.size());

  for (auto &[id, shader] : m_shaders) {

    shaderIds.push_back(id);

  }

  for (entt::id_type id : shaderIds) {

    ReleaseShaderLocked(id);

  }



  if (m_entityTextureArray != 0) {

    if (!m_headless)

      rlUnloadTexture(m_entityTextureArray);

    m_entityTextureArray = 0;

  }

  m_textureLayerMap.clear();



  LOG_INFO("ResourceManager: All resources unloaded.");

}
