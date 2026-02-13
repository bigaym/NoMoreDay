#include "engine/render/passes/DistortionPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/FullscreenQuad.hpp"

#include <algorithm>
#include <cstddef>

namespace NoMoreDay::render::passes {
namespace {

constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLShaderStorageBuffer = 0x90D2;
constexpr uint32_t kGLDynamicDraw = 0x88E8;
constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLTexture0 = 0x84C0;
constexpr uint32_t kGLRgba8 = 0x8058;
constexpr uint32_t kGLRg16f = 0x822F;
constexpr const char *kFullscreenVertexShader =
    "assets/shaders/postprocess/fullscreen.vert";
constexpr const char *kDistortionWriteFragmentShader =
    "assets/shaders/postprocess/distortion_write.frag";
constexpr const char *kDistortionApplyFragmentShader =
    "assets/shaders/postprocess/distortion_apply.frag";

void BindFramebufferAndViewport(const resources::FramebufferHandle &handle) {
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, handle.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, handle.width, handle.height);
}

} // namespace

DistortionPass::DistortionPass() = default;

DistortionPass::~DistortionPass() { Shutdown(); }

void DistortionPass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read("PostProcessColor");
  builder.Write("DistortionColor");
}

bool DistortionPass::Initialize() {
  if (m_initialized) {
    return true;
  }

  m_distortionWriteShader =
      LoadShader(kFullscreenVertexShader, kDistortionWriteFragmentShader);
  m_distortionApplyShader =
      LoadShader(kFullscreenVertexShader, kDistortionApplyFragmentShader);
  if (m_distortionWriteShader.id == 0 || m_distortionApplyShader.id == 0) {
    LOG_ERROR("DistortionPass shader initialization failed");
    Shutdown();
    return false;
  }

  m_sourceCountLoc = GetShaderLocation(m_distortionWriteShader, "uSourceCount");
  m_cameraOffsetLoc = GetShaderLocation(m_distortionWriteShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_distortionWriteShader, "uScreenSize");

  m_applySceneLoc = GetShaderLocation(m_distortionApplyShader, "uSceneTexture");
  m_applyDistortionLoc =
      GetShaderLocation(m_distortionApplyShader, "uDistortionTexture");
  m_applyScaleLoc = GetShaderLocation(m_distortionApplyShader, "uDistortionScale");

  NoMoreDay::utils::GPUUtils::GenBuffers(1, &m_ssbo);
  if (m_ssbo == 0) {
    LOG_ERROR("DistortionPass SSBO creation failed");
    Shutdown();
    return false;
  }
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, m_ssbo);
  NoMoreDay::utils::GPUUtils::BufferData(
      kGLShaderStorageBuffer,
      static_cast<ptrdiff_t>(sizeof(components::GPUDistortionSource) *
                             MAX_DISTORTION_SOURCES),
      nullptr, kGLDynamicDraw);
  NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, 0);

  m_initialized = true;
  return true;
}

bool DistortionPass::ReloadShaders() {
  Shader writeShader =
      LoadShader(kFullscreenVertexShader, kDistortionWriteFragmentShader);
  Shader applyShader =
      LoadShader(kFullscreenVertexShader, kDistortionApplyFragmentShader);
  if (writeShader.id == 0 || applyShader.id == 0) {
    if (writeShader.id != 0) {
      UnloadShader(writeShader);
    }
    if (applyShader.id != 0) {
      UnloadShader(applyShader);
    }
    LOG_WARN("DistortionPass: shader reload failed, keeping previous program");
    return false;
  }

  if (m_distortionWriteShader.id != 0) {
    UnloadShader(m_distortionWriteShader);
  }
  if (m_distortionApplyShader.id != 0) {
    UnloadShader(m_distortionApplyShader);
  }
  m_distortionWriteShader = writeShader;
  m_distortionApplyShader = applyShader;

  m_sourceCountLoc = GetShaderLocation(m_distortionWriteShader, "uSourceCount");
  m_cameraOffsetLoc = GetShaderLocation(m_distortionWriteShader, "uCameraOffset");
  m_screenSizeLoc = GetShaderLocation(m_distortionWriteShader, "uScreenSize");
  m_applySceneLoc = GetShaderLocation(m_distortionApplyShader, "uSceneTexture");
  m_applyDistortionLoc =
      GetShaderLocation(m_distortionApplyShader, "uDistortionTexture");
  m_applyScaleLoc = GetShaderLocation(m_distortionApplyShader, "uDistortionScale");
  LOG_INFO("DistortionPass: shader hot reloaded");
  return true;
}

void DistortionPass::Shutdown() {
  if (m_distortionWriteShader.id != 0) {
    UnloadShader(m_distortionWriteShader);
    m_distortionWriteShader = {};
  }
  if (m_distortionApplyShader.id != 0) {
    UnloadShader(m_distortionApplyShader);
    m_distortionApplyShader = {};
  }

  resources::FramebufferManager::Destroy(m_distortionBuffer);
  resources::FramebufferManager::Destroy(m_applyBuffer);
  m_finalOutputBuffer = {};

  if (m_ssbo != 0) {
    NoMoreDay::utils::GPUUtils::DeleteBuffers(1, &m_ssbo);
    m_ssbo = 0;
  }

  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_initialized = false;
  ResetSources();
}

void DistortionPass::OnResize(int width, int height) {
  if (width <= 0 || height <= 0) {
    return;
  }

  m_cachedWidth = width;
  m_cachedHeight = height;
  resources::FramebufferManager::Resize(m_distortionBuffer, width, height);
  resources::FramebufferManager::Resize(m_applyBuffer, width, height);
}

void DistortionPass::EnsureWorkingBuffers(int width, int height) {
  if (!m_distortionBuffer.IsValid()) {
    m_distortionBuffer =
        resources::FramebufferManager::Create(width, height, kGLRg16f, false);
  } else if (m_distortionBuffer.width != width || m_distortionBuffer.height != height) {
    resources::FramebufferManager::Resize(m_distortionBuffer, width, height);
  }

  if (!m_applyBuffer.IsValid()) {
    m_applyBuffer = resources::FramebufferManager::Create(width, height, kGLRgba8, false);
  } else if (m_applyBuffer.width != width || m_applyBuffer.height != height) {
    resources::FramebufferManager::Resize(m_applyBuffer, width, height);
  }
}

void DistortionPass::AddDistortionSource(float worldX, float worldY, float radius,
                                         float strength) {
  if (m_activeCount >= MAX_DISTORTION_SOURCES) {
    return;
  }

  auto &dst = m_sources[static_cast<size_t>(m_activeCount)];
  dst.posX = worldX;
  dst.posY = worldY;
  dst.radius = std::max(0.0f, radius);
  dst.strength = strength;
  ++m_activeCount;
}

void DistortionPass::ResetSources() { m_activeCount = 0; }

void DistortionPass::Execute(graph::RenderContext &context) {
  if (!m_initialized && !Initialize()) {
    return;
  }
  if (m_inputBuffer == nullptr || !m_inputBuffer->IsValid() ||
      context.qualityManager == nullptr || context.camera == nullptr) {
    m_finalOutputBuffer = {};
    ResetSources();
    return;
  }

  const auto &config = context.qualityManager->GetConfig();
  if (!config.distortionEnabled) {
    m_finalOutputBuffer = *m_inputBuffer;
    ResetSources();
    return;
  }

  const int width = m_inputBuffer->width;
  const int height = m_inputBuffer->height;
  if (width <= 0 || height <= 0) {
    m_finalOutputBuffer = *m_inputBuffer;
    ResetSources();
    return;
  }
  if (m_cachedWidth != width || m_cachedHeight != height) {
    OnResize(width, height);
  }
  EnsureWorkingBuffers(width, height);
  if (!m_distortionBuffer.IsValid() || !m_applyBuffer.IsValid()) {
    m_finalOutputBuffer = *m_inputBuffer;
    ResetSources();
    return;
  }

  BindFramebufferAndViewport(m_distortionBuffer);
  ClearBackground(BLANK);

  if (m_activeCount > 0 && m_ssbo != 0) {
    const int uploadCount = std::min(m_activeCount, MAX_DISTORTION_SOURCES);
    NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, m_ssbo);
    NoMoreDay::utils::GPUUtils::BufferSubData(
        kGLShaderStorageBuffer, 0,
        static_cast<ptrdiff_t>(uploadCount * sizeof(components::GPUDistortionSource)),
        m_sources.data());
    NoMoreDay::utils::GPUUtils::BindBuffer(kGLShaderStorageBuffer, 0);
    NoMoreDay::utils::GPUUtils::BindBufferBase(
        static_cast<uint32_t>(NoMoreDay::RenderConstants::Binding::SSBO_DISTORTION_DATA),
        m_ssbo);

    const float zoom = std::max(context.camera->zoom, 0.0001f);
    const float screenSize[2] = {static_cast<float>(width) / zoom,
                                 static_cast<float>(height) / zoom};
    const float cameraOffset[2] = {
        context.camera->target.x - (context.camera->offset.x / zoom),
        context.camera->target.y - (context.camera->offset.y / zoom)};

    BeginShaderMode(m_distortionWriteShader);
    if (m_sourceCountLoc >= 0) {
      SetShaderValue(m_distortionWriteShader, m_sourceCountLoc, &uploadCount,
                     SHADER_UNIFORM_INT);
    }
    if (m_cameraOffsetLoc >= 0) {
      SetShaderValue(m_distortionWriteShader, m_cameraOffsetLoc, cameraOffset,
                     SHADER_UNIFORM_VEC2);
    }
    if (m_screenSizeLoc >= 0) {
      SetShaderValue(m_distortionWriteShader, m_screenSizeLoc, screenSize,
                     SHADER_UNIFORM_VEC2);
    }
    resources::FullscreenQuad::Draw();
    EndShaderMode();
  }

  BindFramebufferAndViewport(m_applyBuffer);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, m_inputBuffer->colorTexture);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0 + 1);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, m_distortionBuffer.colorTexture);

  BeginShaderMode(m_distortionApplyShader);
  const int sceneUnit = 0;
  const int distortionUnit = 1;
  const float distortionScale = 1.0f;
  if (m_applySceneLoc >= 0) {
    SetShaderValue(m_distortionApplyShader, m_applySceneLoc, &sceneUnit,
                   SHADER_UNIFORM_INT);
  }
  if (m_applyDistortionLoc >= 0) {
    SetShaderValue(m_distortionApplyShader, m_applyDistortionLoc, &distortionUnit,
                   SHADER_UNIFORM_INT);
  }
  if (m_applyScaleLoc >= 0) {
    SetShaderValue(m_distortionApplyShader, m_applyScaleLoc, &distortionScale,
                   SHADER_UNIFORM_FLOAT);
  }
  resources::FullscreenQuad::Draw();
  EndShaderMode();

  m_finalOutputBuffer = m_applyBuffer;
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
  ResetSources();
}

} // namespace NoMoreDay::render::passes
