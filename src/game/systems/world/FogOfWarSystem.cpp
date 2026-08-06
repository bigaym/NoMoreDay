#include "game/systems/world/FogOfWarSystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "rlgl.h"
#include <cmath>


FogOfWarSystem::FogOfWarSystem() = default;

FogOfWarSystem::~FogOfWarSystem() { shutdown(); }

FogOfWarSystem::FogOfWarSystem(FogOfWarSystem &&other) noexcept
    : m_width(other.m_width), m_height(other.m_height),
      m_initialized(other.m_initialized),
      m_visibilityBuffer(std::move(other.m_visibilityBuffer)),
      m_fogShader(other.m_fogShader), m_fogTexture(other.m_fogTexture),
      m_cpuVisibilityCache(std::move(other.m_cpuVisibilityCache)),
      m_cpuCacheDirty(other.m_cpuCacheDirty) {
  other.m_fogTexture.id = 0;
  other.m_fogShader.id = 0;
  other.m_initialized = false;
  other.m_width = 0;
  other.m_height = 0;
}

FogOfWarSystem &FogOfWarSystem::operator=(FogOfWarSystem &&other) noexcept {
  if (this != &other) {
    shutdown();

    m_width = other.m_width;
    m_height = other.m_height;
    m_initialized = other.m_initialized;
    m_visibilityBuffer = std::move(other.m_visibilityBuffer);
    m_fogShader = other.m_fogShader;
    m_fogTexture = other.m_fogTexture;
    m_cpuVisibilityCache = std::move(other.m_cpuVisibilityCache);
    m_cpuCacheDirty = other.m_cpuCacheDirty;

    other.m_fogTexture.id = 0;
    other.m_fogShader.id = 0;
    other.m_initialized = false;
    other.m_width = 0;
    other.m_height = 0;
  }
  return *this;
}

void FogOfWarSystem::initialize(ResourceManager &resources, int width,
                                int height) {
  if (m_initialized) {
    shutdown();
  }

  m_width = width;
  m_height = height;

  LOG_INFO("Initializing GPU FogOfWarSystem ({}x{})...", width, height);

  // 1. 加载 Compute Shader
  m_fogShader = resources.loadComputeShader(
      entt::hashed_string{"fog_update"}, "assets/shaders/fog_update.compute");

  if (m_fogShader.id == 0) {
    LOG_ERROR("Failed to load fog_update.compute shader!");
    return;
  }

  // 2. 创建 GPU 可见性缓冲区 (SSBO)
  size_t cellCount = static_cast<size_t>(width) * height;
  std::vector<uint32_t> initialVisibility(cellCount, VISIBILITY_UNEXPLORED);
  m_visibilityBuffer.Create(cellCount * sizeof(uint32_t),
                            initialVisibility.data(), RL_DYNAMIC_DRAW);

  // 3. 创建输出纹理 (RGBA8, GPU 可写)
  Image fogImage = GenImageColor(width, height, BLACK);
  m_fogTexture = LoadTextureFromImage(fogImage);
  UnloadImage(fogImage);

  // 设置纹理为可被 Compute Shader 写入
  // 注意: Raylib 的纹理默认就是 GL_TEXTURE_2D, 我们需要用 rlgl 绑定为 image

  // 4. 初始化 CPU 缓存
  m_cpuVisibilityCache.resize(cellCount, VISIBILITY_UNEXPLORED);
  m_cpuCacheDirty = true;

  m_initialized = true;
  LOG_INFO("GPU FogOfWarSystem initialized successfully.");
}

void FogOfWarSystem::updateVisibility(const Position &playerPos,
                                      float viewRadius) {
  if (!m_initialized || m_fogShader.id == 0)
    return;

  // 转换世界坐标到网格坐标
  using namespace NoMoreDay::Constants::World;
  using namespace NoMoreDay::Constants::World::Fog;
  float playerGridX = playerPos.x / GRID_TILE_SIZE;
  float playerGridY = playerPos.y / GRID_TILE_SIZE;
  float gridRadius =
      (viewRadius / GRID_TILE_SIZE) + VIEW_RADIUS_BUFFER; // 额外缓冲

  // 启用 Compute Shader
  rlEnableShader(m_fogShader.id);

  // 设置 Uniform
  int locWidth = rlGetLocationUniform(m_fogShader.id, "width");
  int locHeight = rlGetLocationUniform(m_fogShader.id, "height");
  int locPlayerPos = rlGetLocationUniform(m_fogShader.id, "playerGridPos");
  int locRadius = rlGetLocationUniform(m_fogShader.id, "gridRadius");

  rlSetUniform(locWidth, &m_width, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locHeight, &m_height, RL_SHADER_UNIFORM_INT, 1);

  float playerPosArray[2] = {playerGridX, playerGridY};
  rlSetUniform(locPlayerPos, playerPosArray, RL_SHADER_UNIFORM_VEC2, 1);
  rlSetUniform(locRadius, &gridRadius, RL_SHADER_UNIFORM_FLOAT, 1);

  // 绑定 SSBO
  m_visibilityBuffer.BindBase(NoMoreDay::RenderConstants::FogOfWarCS::VISIBILITY_BUFFER);

  // 绑定输出纹理为 Image (binding = 0)
  // 使用 GPUUtils 封装的 glBindImageTexture (使用默认参数 GL_WRITE_ONLY,
  // GL_RGBA8)
  NoMoreDay::utils::GPUUtils::BindImageTexture(
      NoMoreDay::RenderConstants::FogOfWarCS::OUTPUT_IMAGE, m_fogTexture.id);

  // 调度 Compute Shader
  using namespace NoMoreDay::Constants::World::Fog;
  int groupsX = (m_width + (COMPUTE_GROUP_SIZE - 1)) / COMPUTE_GROUP_SIZE;
  int groupsY = (m_height + (COMPUTE_GROUP_SIZE - 1)) / COMPUTE_GROUP_SIZE;

  // The compute shader writes both the visibility SSBO and fog image, which
  // are consumed immediately by CPU readback and the following texture draw.
  NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(groupsX, groupsY, 1);
  NoMoreDay::render::core::ApplyComputeToFragmentBarrierTemplate();

  rlDisableShader();

  // 标记 CPU 缓存为脏
  m_cpuCacheDirty = true;
}

void FogOfWarSystem::renderFog() const {
  using namespace NoMoreDay::Constants::World;
  using namespace NoMoreDay::Constants::World::Fog;
  float scale = GRID_TILE_SIZE;
  float mapPixelW = m_width * scale;
  float mapPixelH = m_height * scale;

  // 绘制地图外的黑色背景
  DrawRectangle((int)-BACKGROUND_PADDING, (int)-BACKGROUND_PADDING,
                static_cast<int>(mapPixelW + BACKGROUND_PADDING * 2.0f),
                (int)BACKGROUND_PADDING, BLACK);
  DrawRectangle((int)-BACKGROUND_PADDING, static_cast<int>(mapPixelH),
                static_cast<int>(mapPixelW + BACKGROUND_PADDING * 2.0f),
                (int)BACKGROUND_PADDING, BLACK);
  DrawRectangle((int)-BACKGROUND_PADDING, 0, (int)BACKGROUND_PADDING,
                static_cast<int>(mapPixelH), BLACK);
  DrawRectangle(static_cast<int>(mapPixelW), 0, (int)BACKGROUND_PADDING,
                static_cast<int>(mapPixelH), BLACK);

  // 绘制迷雾纹理 (GPU 已生成)
  DrawTextureEx(m_fogTexture, {0.0f, 0.0f}, 0.0f, scale, WHITE);
}

void FogOfWarSystem::syncToCPU() const {
  if (!m_initialized || !m_cpuCacheDirty)
    return;

  size_t cellCount = static_cast<size_t>(m_width) * m_height;
  m_visibilityBuffer.Read(m_cpuVisibilityCache.data(),
                          cellCount * sizeof(uint32_t));
  m_cpuCacheDirty = false;
}

bool FogOfWarSystem::isVisible(int x, int y) const {
  return getVisibility(x, y) >= VISIBILITY_VISIBLE;
}

bool FogOfWarSystem::isExplored(int x, int y) const {
  return getVisibility(x, y) >= VISIBILITY_EXPLORED;
}

uint32_t FogOfWarSystem::getVisibility(int x, int y) const {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return VISIBILITY_UNEXPLORED;
  }

  // 如果需要 CPU 访问, 先同步
  if (m_cpuCacheDirty) {
    syncToCPU();
  }

  return m_cpuVisibilityCache[y * m_width + x];
}

void FogOfWarSystem::setVisibility(int x, int y, uint32_t visibility) {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    return;

  // 更新 CPU 缓存
  if (m_cpuCacheDirty) {
    syncToCPU();
  }

  size_t index = y * m_width + x;
  m_cpuVisibilityCache[index] = visibility;

  // 更新 GPU 缓冲区 (单个值)
  m_visibilityBuffer.Update(&visibility, sizeof(uint32_t),
                            index * sizeof(uint32_t));
}

void FogOfWarSystem::shutdown() {
  if (!m_initialized)
    return;

  LOG_INFO("Shutting down GPU FogOfWarSystem...");

  m_visibilityBuffer.Release();

  if (m_fogTexture.id != 0) {
    UnloadTexture(m_fogTexture);
    m_fogTexture.id = 0;
  }

  m_cpuVisibilityCache.clear();
  m_width = 0;
  m_height = 0;
  m_initialized = false;
}
