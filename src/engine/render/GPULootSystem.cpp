#include "engine/render/GPULootSystem.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "rlgl.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace NoMoreDay::render {
namespace {

constexpr uint32_t kWorkGroupSize = 64u;
constexpr uint32_t kIndirectBufferBinding = 3u;
constexpr uint32_t kLootRenderInstanceBinding = 0u;
constexpr uint32_t kVisibleIndexBinding = 1u;
constexpr uint32_t kCounterBinding = 2u;
constexpr uint32_t kForceBinding = 4u;
constexpr uint32_t kGridBinding = 5u;

constexpr uint32_t kGLDrawIndirectBuffer = 0x8F3F;
constexpr uint32_t kGLTriangles = 0x0004;
constexpr uint32_t kGLVertexShader = 0x8B31;
constexpr uint32_t kGLFragmentShader = 0x8B30;
constexpr uint32_t kGLCompileStatus = 0x8B81;
constexpr uint32_t kGLInfoLogLength = 0x8B84;

using MultiDrawArraysIndirectCountFn = void(APIENTRY *)(uint32_t, const void *,
                                                        int32_t, int32_t);
using CreateShaderFn = uint32_t(APIENTRY *)(uint32_t);
using ShaderSourceFn = void(APIENTRY *)(uint32_t, int32_t, const char *const *,
                                         const int32_t *);
using CompileShaderFn = void(APIENTRY *)(uint32_t);
using GetShaderIvFn = void(APIENTRY *)(uint32_t, uint32_t, int32_t *);
using GetShaderInfoLogFn = void(APIENTRY *)(uint32_t, int32_t, int32_t *, char *);
using DeleteShaderFn = void(APIENTRY *)(uint32_t);

uint32_t CompileShaderWithGLFallback(const uint32_t stage, const std::string &source,
                                     const char *label) {
  static CreateShaderFn createShader = nullptr;
  static ShaderSourceFn shaderSource = nullptr;
  static CompileShaderFn compileShader = nullptr;
  static GetShaderIvFn getShaderIv = nullptr;
  static GetShaderInfoLogFn getShaderInfoLog = nullptr;
  static DeleteShaderFn deleteShader = nullptr;
  static bool loaded = false;

  if (!loaded) {
    createShader = reinterpret_cast<CreateShaderFn>(glfwGetProcAddress("glCreateShader"));
    shaderSource = reinterpret_cast<ShaderSourceFn>(glfwGetProcAddress("glShaderSource"));
    compileShader = reinterpret_cast<CompileShaderFn>(glfwGetProcAddress("glCompileShader"));
    getShaderIv = reinterpret_cast<GetShaderIvFn>(glfwGetProcAddress("glGetShaderiv"));
    getShaderInfoLog =
        reinterpret_cast<GetShaderInfoLogFn>(glfwGetProcAddress("glGetShaderInfoLog"));
    deleteShader = reinterpret_cast<DeleteShaderFn>(glfwGetProcAddress("glDeleteShader"));
    loaded = true;
  }

  if (createShader == nullptr || shaderSource == nullptr || compileShader == nullptr ||
      getShaderIv == nullptr || getShaderInfoLog == nullptr || deleteShader == nullptr) {
    LOG_ERROR("GPULootSystem: OpenGL fallback compiler unavailable for '{}'", label);
    return 0u;
  }

  const uint32_t shaderId = createShader(stage);
  if (shaderId == 0u) {
    LOG_ERROR("GPULootSystem: OpenGL fallback failed to create shader '{}'", label);
    return 0u;
  }

  const char *srcPtr = source.c_str();
  const int32_t srcLen = static_cast<int32_t>(source.size());
  shaderSource(shaderId, 1, &srcPtr, &srcLen);
  compileShader(shaderId);

  int32_t status = 0;
  getShaderIv(shaderId, kGLCompileStatus, &status);
  if (status == 1) {
    LOG_WARN("GPULootSystem: OpenGL fallback compiled shader '{}' after rlCompileShader "
             "failed",
             label);
    return shaderId;
  }

  int32_t infoLen = 0;
  getShaderIv(shaderId, kGLInfoLogLength, &infoLen);
  std::string infoLog;
  if (infoLen > 1) {
    infoLog.resize(static_cast<size_t>(infoLen), '\0');
    int32_t written = 0;
    getShaderInfoLog(shaderId, infoLen, &written, infoLog.data());
    if (written > 0 && written < infoLen) {
      infoLog.resize(static_cast<size_t>(written));
    }
  }
  LOG_ERROR("GPULootSystem: OpenGL fallback compile failed for '{}': {}",
            label, infoLog.empty() ? "no info log" : infoLog);
  deleteShader(shaderId);
  return 0u;
}

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
    LOG_ERROR("GPULootSystem: shader include nesting too deep: {}", path.string());
    return false;
  }

  const std::filesystem::path normalizedPath =
      std::filesystem::absolute(path).lexically_normal();
  const std::string key = normalizedPath.string();
  if (!includeStack.insert(key).second) {
    LOG_ERROR("GPULootSystem: cyclic shader include detected: {}", key);
    return false;
  }

  std::string fileText;
  if (!ReadTextFileUtf8(normalizedPath, fileText)) {
    includeStack.erase(key);
    LOG_ERROR("GPULootSystem: failed to read compute shader: {}", normalizedPath.string());
    return false;
  }

  std::istringstream input(fileText);
  std::ostringstream output;
  std::string line;
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
        LOG_ERROR("GPULootSystem: malformed include in '{}': {}",
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
  std::unordered_set<std::string> includeStack;
  return PreprocessShaderIncludesImpl(std::filesystem::path(path), outText,
                                      includeStack, 0);
}

Shader LoadShaderFromFilesWithIncludes(const char *vertexPath,
                                       const char *fragmentPath) {
  if (!FileExists(vertexPath)) {
    LOG_ERROR("GPULootSystem: vertex shader file missing: {}", vertexPath);
    return {};
  }
  if (!FileExists(fragmentPath)) {
    LOG_ERROR("GPULootSystem: fragment shader file missing: {}", fragmentPath);
    return {};
  }

  std::string vsSource;
  if (!PreprocessShaderIncludes(vertexPath, vsSource) || vsSource.empty()) {
    LOG_ERROR("GPULootSystem: failed to preprocess vertex shader: {}", vertexPath);
    return {};
  }

  std::string fsSource;
  if (!PreprocessShaderIncludes(fragmentPath, fsSource) || fsSource.empty()) {
    LOG_ERROR("GPULootSystem: failed to preprocess fragment shader: {}", fragmentPath);
    return {};
  }

  auto compileStage = [](const int stage, const std::string &source,
                         const char *label) -> unsigned int {
    const unsigned int shaderId = rlCompileShader(source.c_str(), stage);
    if (shaderId == 0u) {
      std::istringstream stream(source);
      std::string line;
      std::ostringstream firstLines;
      int lineNo = 0;
      while (lineNo < 12 && std::getline(stream, line)) {
        ++lineNo;
        firstLines << lineNo << ": " << line << '\n';
      }
      LOG_ERROR("GPULootSystem: {} shader compile failed (first lines):\n{}",
                label, firstLines.str());
      const uint32_t glStage =
          (stage == RL_VERTEX_SHADER) ? kGLVertexShader : kGLFragmentShader;
      return CompileShaderWithGLFallback(glStage, source, label);
    }
    return shaderId;
  };

  const unsigned int vertexShaderId =
      compileStage(RL_VERTEX_SHADER, vsSource, vertexPath);
  if (vertexShaderId == 0u) {
    LOG_ERROR("GPULootSystem: failed to compile vertex shader '{}'", vertexPath);
    return {};
  }

  const unsigned int fragmentShaderId =
      compileStage(RL_FRAGMENT_SHADER, fsSource, fragmentPath);
  if (fragmentShaderId == 0u) {
    LOG_ERROR("GPULootSystem: failed to compile fragment shader '{}'", fragmentPath);
    return {};
  }

  const unsigned int programId = rlLoadShaderProgram(vertexShaderId, fragmentShaderId);
  if (programId == 0u) {
    LOG_ERROR("GPULootSystem: failed to link render shader pair: {} + {}",
              vertexPath, fragmentPath);
    return {};
  }

  Shader shader = {};
  shader.id = programId;
  shader.locs = static_cast<int *>(RL_MALLOC(32 * sizeof(int)));
  if (shader.locs != nullptr) {
    for (int i = 0; i < 32; ++i) {
      shader.locs[i] = -1;
    }
  }
  return shader;
}

Shader LoadComputeShaderFromFile(const char *path) {
  if (!FileExists(path)) {
    LOG_ERROR("GPULootSystem: compute shader file missing: {}", path);
    return {};
  }

  std::string source;
  if (!PreprocessShaderIncludes(path, source) || source.empty()) {
    LOG_ERROR("GPULootSystem: failed to preprocess compute shader: {}", path);
    return {};
  }

  const unsigned int shaderId = rlCompileShader(source.c_str(), RL_COMPUTE_SHADER);
  if (shaderId == 0) {
    LOG_ERROR("GPULootSystem: failed to compile compute shader: {}", path);
    return {};
  }

  const unsigned int programId = rlLoadComputeShaderProgram(shaderId);
  if (programId == 0) {
    LOG_ERROR("GPULootSystem: failed to link compute shader: {}", path);
    return {};
  }

  Shader shader = {};
  shader.id = programId;
  shader.locs = static_cast<int *>(RL_MALLOC(32 * sizeof(int)));
  if (shader.locs != nullptr) {
    for (int i = 0; i < 32; ++i) {
      shader.locs[i] = -1;
    }
  }
  return shader;
}

} // namespace

void GPULootSystem::Init(const uint32_t maxInstances) {
  if (m_initialized) {
    return;
  }

  m_maxInstances = std::max(1u, maxInstances);
  m_syncedInstanceCount = 0;
  m_visibleInstanceCount = 0;
  m_gridWidth = 128;
  m_gridHeight = 128;
  m_instances.clear();
  m_instances.reserve(m_maxInstances);

  m_instanceBuffer.Create(m_maxInstances * sizeof(components::GPULootInstance), nullptr,
                          RL_DYNAMIC_DRAW);
  m_visibleIndexBuffer.Create(m_maxInstances * sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  m_counterBuffer.Create(sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  m_indirectBuffer.Create(sizeof(DrawArraysIndirectCommand), nullptr, RL_DYNAMIC_DRAW);
  m_forceBuffer.Create(m_maxInstances * sizeof(Vector2), nullptr, RL_DYNAMIC_DRAW);
  m_gridCountBuffer.Create(m_gridWidth * m_gridHeight * sizeof(uint32_t), nullptr,
                           RL_DYNAMIC_DRAW);

  m_instanceBuffer.BindBase(NoMoreDay::RenderConstants::LootPassBinding::INSTANCE_SSBO);

  m_cullShader =
      LoadComputeShaderFromFile("assets/shaders/loot/loot_frustum_cull.compute");
  m_indirectArgsShader =
      LoadComputeShaderFromFile("assets/shaders/loot/loot_indirect_args.compute");
  m_gridHashShader =
      LoadComputeShaderFromFile("assets/shaders/loot/loot_grid_hash.compute");
  m_repulsionShader =
      LoadComputeShaderFromFile("assets/shaders/loot/loot_repulsion.compute");
  m_positionUpdateShader =
      LoadComputeShaderFromFile("assets/shaders/loot/loot_position_update.compute");
  // Try raylib file-path loader first; if it fails, fall back to explicit
  // compile path so we can capture stage-level diagnostics.
  m_renderShader = LoadShader("assets/shaders/loot/loot_quad.vert",
                              "assets/shaders/loot/loot_quad.frag");
  if (m_renderShader.id == 0) {
    LOG_WARN("GPULootSystem: LoadShader(file-path) failed for loot render pair, "
             "falling back to explicit compile path");
    m_renderShader = LoadShaderFromFilesWithIncludes(
        "assets/shaders/loot/loot_quad.vert", "assets/shaders/loot/loot_quad.frag");
  }

  if (m_cullShader.id == 0 || m_indirectArgsShader.id == 0 ||
      m_gridHashShader.id == 0 || m_repulsionShader.id == 0 ||
      m_positionUpdateShader.id == 0 || m_renderShader.id == 0) {
    LOG_ERROR(
        "GPULootSystem: shader init failed ids[cull={}, indirect={}, grid={}, "
        "repulsion={}, position={}, render={}]",
        m_cullShader.id, m_indirectArgsShader.id, m_gridHashShader.id,
        m_repulsionShader.id, m_positionUpdateShader.id, m_renderShader.id);
    Shutdown();
    return;
  }

  m_locCullCount = rlGetLocationUniform(m_cullShader.id, "uInstanceCount");
  m_locCullViewRect = rlGetLocationUniform(m_cullShader.id, "uViewRect");
  m_locIndirectMaxCount =
      rlGetLocationUniform(m_indirectArgsShader.id, "uMaxInstanceCount");
  m_locGridVisibleCount =
      rlGetLocationUniform(m_gridHashShader.id, "uVisibleCount");
  m_locGridCellSize = rlGetLocationUniform(m_gridHashShader.id, "uGridCellSize");
  m_locGridGridWidth = rlGetLocationUniform(m_gridHashShader.id, "uGridWidth");
  m_locGridGridHeight = rlGetLocationUniform(m_gridHashShader.id, "uGridHeight");
  m_locRepulsionVisibleCount =
      rlGetLocationUniform(m_repulsionShader.id, "uVisibleCount");
  m_locRepulsionMinDist = rlGetLocationUniform(m_repulsionShader.id, "uMinDistance");
  m_locRepulsionStiffness = rlGetLocationUniform(m_repulsionShader.id, "uStiffness");
  m_locRepulsionMaxForce = rlGetLocationUniform(m_repulsionShader.id, "uMaxForce");
  m_locRepulsionDamping = rlGetLocationUniform(m_repulsionShader.id, "uDamping");
  m_locUpdateVisibleCount =
      rlGetLocationUniform(m_positionUpdateShader.id, "uVisibleCount");
  m_locUpdateDamping = rlGetLocationUniform(m_positionUpdateShader.id, "uDamping");
  m_locUpdateMaxOffset =
      rlGetLocationUniform(m_positionUpdateShader.id, "uMaxOffset");
  m_locRenderMvp = rlGetLocationUniform(m_renderShader.id, "uMVP");
  m_locRenderGlowEnabled = rlGetLocationUniform(m_renderShader.id, "uGlowEnabled");

  const float vertices[] = {
      -0.5f, -0.5f, 0.0f, 0.0f, 0.5f,  -0.5f, 1.0f, 0.0f,
      0.5f,  0.5f,  1.0f, 1.0f, -0.5f, -0.5f, 0.0f, 0.0f,
      0.5f,  0.5f,  1.0f, 1.0f, -0.5f, 0.5f,  0.0f, 1.0f};
  m_vao = rlLoadVertexArray();
  rlEnableVertexArray(m_vao);
  m_vbo = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
  rlSetVertexAttribute(0, 2, RL_FLOAT, false, 4 * sizeof(float), 0);
  rlEnableVertexAttribute(0);
  rlSetVertexAttribute(1, 2, RL_FLOAT, false, 4 * sizeof(float), 2 * sizeof(float));
  rlEnableVertexAttribute(1);
  rlDisableVertexArray();

  ResetDispatchState();
  m_initialized = true;
  LOG_INFO("GPULootSystem: initialized maxInstances={}", m_maxInstances);
}

void GPULootSystem::Shutdown() {
  m_instances.clear();
  m_syncedInstanceCount = 0;
  m_visibleInstanceCount = 0;

  if (m_vao != 0) {
    rlUnloadVertexArray(m_vao);
    m_vao = 0;
  }
  if (m_vbo != 0) {
    rlUnloadVertexBuffer(m_vbo);
    m_vbo = 0;
  }

  if (m_renderShader.id != 0) {
    UnloadShader(m_renderShader);
    m_renderShader = {};
  }
  if (m_cullShader.id != 0) {
    UnloadShader(m_cullShader);
    m_cullShader = {};
  }
  if (m_indirectArgsShader.id != 0) {
    UnloadShader(m_indirectArgsShader);
    m_indirectArgsShader = {};
  }
  if (m_gridHashShader.id != 0) {
    UnloadShader(m_gridHashShader);
    m_gridHashShader = {};
  }
  if (m_repulsionShader.id != 0) {
    UnloadShader(m_repulsionShader);
    m_repulsionShader = {};
  }
  if (m_positionUpdateShader.id != 0) {
    UnloadShader(m_positionUpdateShader);
    m_positionUpdateShader = {};
  }

  m_instanceBuffer.Release();
  m_visibleIndexBuffer.Release();
  m_counterBuffer.Release();
  m_indirectBuffer.Release();
  m_forceBuffer.Release();
  m_gridCountBuffer.Release();

  m_maxInstances = 0;
  m_initialized = false;
}

void GPULootSystem::EnsureCapacity(const uint32_t requiredInstances) {
  if (!m_initialized || requiredInstances <= m_maxInstances) {
    return;
  }

  const uint32_t newCapacity = std::max(requiredInstances, m_maxInstances * 2u);
  m_maxInstances = newCapacity;
  m_instances.reserve(m_maxInstances);
  m_instanceBuffer.Create(m_maxInstances * sizeof(components::GPULootInstance), nullptr,
                          RL_DYNAMIC_DRAW);
  m_visibleIndexBuffer.Create(m_maxInstances * sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  m_forceBuffer.Create(m_maxInstances * sizeof(Vector2), nullptr, RL_DYNAMIC_DRAW);
  m_instanceBuffer.BindBase(NoMoreDay::RenderConstants::LootPassBinding::INSTANCE_SSBO);
  LOG_INFO("GPULootSystem: resized instance capacity to {}", m_maxInstances);
}

void GPULootSystem::UploadInstances(
    const std::span<const components::GPULootInstance> instances) {
  if (!m_initialized) {
    return;
  }

  // The Game-side GPULootAdapter has already measured real demand; the span is
  // the exact instance set, so sizing to it avoids truncation.
  const uint32_t instanceCount = static_cast<uint32_t>(instances.size());
  EnsureCapacity(instanceCount);

  m_instances.clear();
  m_instances.reserve(m_maxInstances);
  m_instances.assign(instances.begin(), instances.end());

  m_syncedInstanceCount = instanceCount;

  m_debugSnapshot.required = instanceCount;
  m_debugSnapshot.synced = m_syncedInstanceCount;
  m_debugSnapshot.maxInstances = m_maxInstances;

  if (m_instances.empty()) {
    ResetDispatchState();
    return;
  }

  const size_t uploadSize = m_instances.size() * sizeof(components::GPULootInstance);
  m_instanceBuffer.OrphanAndUpload(m_instances.data(), uploadSize, RL_DYNAMIC_DRAW);
  m_instanceBuffer.BindBase(NoMoreDay::RenderConstants::LootPassBinding::INSTANCE_SSBO);
}

void GPULootSystem::ResetDispatchState() {
  m_visibleInstanceCount = 0;
  const uint32_t zero = 0u;
  const DrawArraysIndirectCommand cmd = {6u, 0u, 0u, 0u};
  m_counterBuffer.Update(&zero, sizeof(zero), 0);
  m_indirectBuffer.Update(&cmd, sizeof(cmd), 0);
}

void GPULootSystem::Dispatch(const Camera2D &camera, const int screenWidth,
                             const int screenHeight,
                             const bool enableForceDirected) {
  if (!m_initialized || m_syncedInstanceCount == 0) {
    ResetDispatchState();
    return;
  }

  const uint32_t zero = 0u;
  m_counterBuffer.Update(&zero, sizeof(zero), 0);

  const Vector2 worldTL = GetScreenToWorld2D({0.0f, 0.0f}, camera);
  const Vector2 worldBR = GetScreenToWorld2D(
      {static_cast<float>(screenWidth), static_cast<float>(screenHeight)}, camera);
  const float viewRect[4] = {
      std::min(worldTL.x, worldBR.x) - 80.0f, std::min(worldTL.y, worldBR.y) - 80.0f,
      std::max(worldTL.x, worldBR.x) + 80.0f, std::max(worldTL.y, worldBR.y) + 80.0f};

  // Render shader uses a dedicated low binding index for wider GLSL compiler compatibility.
  m_instanceBuffer.BindBase(kLootRenderInstanceBinding);
  m_visibleIndexBuffer.BindBase(kVisibleIndexBinding);
  m_counterBuffer.BindBase(kCounterBinding);
  m_indirectBuffer.BindBase(kIndirectBufferBinding);
  m_forceBuffer.BindBase(kForceBinding);
  m_gridCountBuffer.BindBase(kGridBinding);

  rlEnableShader(m_cullShader.id);
  const int instanceCount = static_cast<int>(m_syncedInstanceCount);
  if (m_locCullCount >= 0) {
    rlSetUniform(m_locCullCount, &instanceCount, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_locCullViewRect >= 0) {
    rlSetUniform(m_locCullViewRect, viewRect, RL_SHADER_UNIFORM_VEC4, 1);
  }
  const uint32_t cullGroups = (m_syncedInstanceCount + kWorkGroupSize - 1u) / kWorkGroupSize;
  NoMoreDay::utils::GPUUtils::DispatchCompute(cullGroups, 1u, 1u);
  rlDisableShader();

  rlEnableShader(m_indirectArgsShader.id);
  const int maxInstanceCount = static_cast<int>(m_maxInstances);
  if (m_locIndirectMaxCount >= 0) {
    rlSetUniform(m_locIndirectMaxCount, &maxInstanceCount, RL_SHADER_UNIFORM_INT, 1);
  }
  NoMoreDay::utils::GPUUtils::DispatchCompute(1u, 1u, 1u);
  rlDisableShader();

  m_counterBuffer.Read(&m_visibleInstanceCount, sizeof(m_visibleInstanceCount), 0);
  m_debugSnapshot.visible = m_visibleInstanceCount;
  
  if (m_visibleInstanceCount == 0 || !enableForceDirected) {
    return;
  }

  const std::vector<uint32_t> zeroGrid(m_gridWidth * m_gridHeight, 0u);
  m_gridCountBuffer.OrphanAndUpload(zeroGrid.data(),
                                    zeroGrid.size() * sizeof(uint32_t), RL_DYNAMIC_DRAW);

  rlEnableShader(m_gridHashShader.id);
  const int visibleCount = static_cast<int>(m_visibleInstanceCount);
  const int gridWidth = static_cast<int>(m_gridWidth);
  const int gridHeight = static_cast<int>(m_gridHeight);
  if (m_locGridVisibleCount >= 0) {
    rlSetUniform(m_locGridVisibleCount, &visibleCount, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_locGridCellSize >= 0) {
    rlSetUniform(m_locGridCellSize, &m_gridCellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  if (m_locGridGridWidth >= 0) {
    rlSetUniform(m_locGridGridWidth, &gridWidth, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_locGridGridHeight >= 0) {
    rlSetUniform(m_locGridGridHeight, &gridHeight, RL_SHADER_UNIFORM_INT, 1);
  }
  const uint32_t gridGroups =
      (m_visibleInstanceCount + kWorkGroupSize - 1u) / kWorkGroupSize;
  NoMoreDay::utils::GPUUtils::DispatchCompute(gridGroups, 1u, 1u);
  rlDisableShader();

  rlEnableShader(m_repulsionShader.id);
  constexpr float kMinDistance = 34.0f;
  constexpr float kStiffness = 16.0f;
  constexpr float kMaxForce = 4.0f;
  constexpr float kDamping = 0.86f;
  if (m_locRepulsionVisibleCount >= 0) {
    rlSetUniform(m_locRepulsionVisibleCount, &visibleCount, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_locRepulsionMinDist >= 0) {
    rlSetUniform(m_locRepulsionMinDist, &kMinDistance, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  if (m_locRepulsionStiffness >= 0) {
    rlSetUniform(m_locRepulsionStiffness, &kStiffness, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  if (m_locRepulsionMaxForce >= 0) {
    rlSetUniform(m_locRepulsionMaxForce, &kMaxForce, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  if (m_locRepulsionDamping >= 0) {
    rlSetUniform(m_locRepulsionDamping, &kDamping, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  NoMoreDay::utils::GPUUtils::DispatchCompute(gridGroups, 1u, 1u);
  rlDisableShader();

  rlEnableShader(m_positionUpdateShader.id);
  constexpr float kOffsetDamping = 0.78f;
  constexpr float kMaxOffset = 84.0f;
  if (m_locUpdateVisibleCount >= 0) {
    rlSetUniform(m_locUpdateVisibleCount, &visibleCount, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_locUpdateDamping >= 0) {
    rlSetUniform(m_locUpdateDamping, &kOffsetDamping, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  if (m_locUpdateMaxOffset >= 0) {
    rlSetUniform(m_locUpdateMaxOffset, &kMaxOffset, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  NoMoreDay::utils::GPUUtils::DispatchCompute(gridGroups, 1u, 1u);
  rlDisableShader();
}

void GPULootSystem::Render(const Matrix &viewProj, const bool enableGlow) const {
  if (!m_initialized || m_renderShader.id == 0 || m_vao == 0 ||
      m_visibleInstanceCount == 0) {
    return;
  }

  static MultiDrawArraysIndirectCountFn s_multiDrawArraysIndirect = nullptr;
  static bool s_multiDrawProbeDone = false;
  if (!s_multiDrawProbeDone) {
    s_multiDrawArraysIndirect =
        reinterpret_cast<MultiDrawArraysIndirectCountFn>(
            glfwGetProcAddress("glMultiDrawArraysIndirect"));
    s_multiDrawProbeDone = true;
  }

  rlDrawRenderBatchActive();
  rlEnableShader(m_renderShader.id);
  if (m_locRenderMvp >= 0) {
    rlSetUniformMatrix(m_locRenderMvp, viewProj);
  }
  if (m_locRenderGlowEnabled >= 0) {
    const int glowFlag = enableGlow ? 1 : 0;
    rlSetUniform(m_locRenderGlowEnabled, &glowFlag, RL_SHADER_UNIFORM_INT, 1);
  }

  m_instanceBuffer.BindBase(NoMoreDay::RenderConstants::LootPassBinding::INSTANCE_SSBO);
  m_visibleIndexBuffer.BindBase(kVisibleIndexBinding);
  m_indirectBuffer.Bind(kGLDrawIndirectBuffer);
  NoMoreDay::utils::GPUUtils::MemoryBarrier(NoMoreDay::RenderConstants::Barrier::All);

  rlEnableVertexArray(m_vao);
  if (s_multiDrawArraysIndirect != nullptr) {
    s_multiDrawArraysIndirect(kGLTriangles, nullptr, 1, 0);
  } else {
    NoMoreDay::utils::GPUUtils::DrawArraysIndirect(kGLTriangles, 0);
  }
  rlDisableVertexArray();

  NoMoreDay::utils::GPUUtils::BindBuffer(kGLDrawIndirectBuffer, 0);
  rlDisableShader();
}

} // namespace NoMoreDay::render
