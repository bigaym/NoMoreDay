#include "engine/render/passes/GICompositePass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/core/RenderSyncContracts.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/lighting/LightManager.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"

#include <entt/entt.hpp>
#include <algorithm>
#include <cmath>

namespace NoMoreDay::render::passes {
namespace {

using namespace entt::literals;

constexpr uint32_t kGLReadFramebuffer = 0x8CA8;
constexpr uint32_t kGLDrawFramebuffer = 0x8CA9;
constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLReadOnly = 0x88B8;
constexpr uint32_t kGLWriteOnly = 0x88B9;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLTexture0 = 0x84C0;
constexpr uint32_t kGLTexture1 = 0x84C1;
constexpr uint32_t kGLTexture2 = 0x84C2;
constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLTexture2DArray = 0x8C1A;
constexpr int kRadianceTexUnit = 1;
constexpr int kHeightFieldTexUnit = 2;
constexpr uint32_t kGLColorBufferBit = 0x00004000;
constexpr uint32_t kGLComputeGroupSize = 8u;

uint32_t DivUp(const uint32_t value, const uint32_t divisor) {
  return (value + divisor - 1u) / divisor;
}

} // namespace

GICompositePass::GICompositePass() = default;

GICompositePass::~GICompositePass() { Shutdown(); }

void GICompositePass::Setup(graph::RenderGraphBuilder &builder) {
  builder.Read(graph::RenderResourceTag::SceneHdrColor,
               graph::RenderOwnerTag::GIComposite);
  builder.Read(graph::RenderResourceTag::RadianceMap,
               graph::RenderOwnerTag::RadianceCascades);
  builder.Write(graph::RenderResourceTag::SceneHdrColor,
                graph::RenderOwnerTag::GIComposite);

  // Same-pass phase barrier: the composite compute dispatch writes the output
  // scene image; the framebuffer blit that follows in this Execute reads it.
  // Declared here and emitted via EmitPhaseBarrier(Compute, Fragment) at the
  // exact execution point between dispatch and blit.
  builder.AddPhaseBarrier(
      graph::PipelineStage::Compute, graph::PipelineStage::Fragment,
      static_cast<uint32_t>(RenderConstants::Barrier::Image) |
          static_cast<uint32_t>(RenderConstants::Barrier::Buffer));
}

bool GICompositePass::Initialize(ResourceManager &resources) {
  if (m_initialized) {
    return true;
  }

  m_compositeShader = resources.loadComputeShader(
      "v5_gi_composite_compute"_hs, "assets/shaders/lighting/v5_gi_composite.comp");
  if (m_compositeShader.id == 0) {
    Shutdown();
    return false;
  }

  m_sceneResolutionLoc = rlGetLocationUniform(m_compositeShader.id, "uSceneResolution");
  m_temporalWeightLoc = rlGetLocationUniform(m_compositeShader.id, "uTemporalWeight");
  m_giIntensityLoc = rlGetLocationUniform(m_compositeShader.id, "uGiIntensity");
  m_resetHistoryLoc = rlGetLocationUniform(m_compositeShader.id, "uResetHistory");
  m_cameraDeltaUvLoc = rlGetLocationUniform(m_compositeShader.id, "uCameraDeltaUv");
  m_zoomRatioLoc = rlGetLocationUniform(m_compositeShader.id, "uZoomRatio");
  m_occupancyEnabledLoc =
      rlGetLocationUniform(m_compositeShader.id, "uOccupancyEnabled");
  m_radianceTexLoc = rlGetLocationUniform(m_compositeShader.id, "uRadianceAtlas");
  if (m_radianceTexLoc < 0) {
    m_radianceTexLoc = rlGetLocationUniform(m_compositeShader.id, "uRadianceTex");
  }
  m_heightFieldTexLoc =
      rlGetLocationUniform(m_compositeShader.id, "uHeightFieldTex");
  m_heightFieldEnabledLoc =
      rlGetLocationUniform(m_compositeShader.id, "uHeightFieldEnabled");
  m_raysPerProbeLoc =
      rlGetLocationUniform(m_compositeShader.id, "uRaysPerProbe");
  m_dirtyRectCountLoc = rlGetLocationUniform(m_compositeShader.id, "uDirtyRectCount");
  m_dirtyRectsLoc = rlGetLocationUniform(m_compositeShader.id, "uDirtyRects");

  m_initialized = true;
  return true;
}

void GICompositePass::Shutdown() {
  m_compositeShader = {};
  resources::FramebufferManager::Destroy(m_outputScene);
  resources::FramebufferManager::Destroy(m_historyA);
  resources::FramebufferManager::Destroy(m_historyB);
  resources::FramebufferManager::Destroy(m_occupancyHistoryA);
  resources::FramebufferManager::Destroy(m_occupancyHistoryB);
  m_sceneResolutionLoc = -1;
  m_temporalWeightLoc = -1;
  m_giIntensityLoc = -1;
  m_resetHistoryLoc = -1;
  m_cameraDeltaUvLoc = -1;
  m_zoomRatioLoc = -1;
  m_occupancyEnabledLoc = -1;
  m_radianceTexLoc = -1;
  m_heightFieldTexLoc = -1;
  m_heightFieldEnabledLoc = -1;
  m_raysPerProbeLoc = -1;
  m_dirtyRectCountLoc = -1;
  m_dirtyRectsLoc = -1;
  m_cachedWidth = 0;
  m_cachedHeight = 0;
  m_initialized = false;
  m_historyValid = false;
  m_readHistoryA = true;
  m_prevCameraValid = false;
  m_prevCameraTarget = {0.0f, 0.0f};
  m_prevCameraZoom = 0.0f;
  m_prevActiveLights.clear();
  m_prevOccluderMaskVersion = 0u;
  m_prevVfxEmissionSnapshotVersion = 0u;
}

void GICompositePass::OnResize(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return;
  }
  m_cachedWidth = width;
  m_cachedHeight = height;
}

bool GICompositePass::EnsureResources(const int width, const int height) {
  if (width <= 0 || height <= 0) {
    return false;
  }

  if (!m_outputScene.IsValid()) {
    m_outputScene = resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
  } else if (m_outputScene.width != width || m_outputScene.height != height) {
    resources::FramebufferManager::Resize(m_outputScene, width, height);
  }

  if (!m_historyA.IsValid()) {
    m_historyA = resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
  } else if (m_historyA.width != width || m_historyA.height != height) {
    resources::FramebufferManager::Resize(m_historyA, width, height);
  }

  if (!m_historyB.IsValid()) {
    m_historyB = resources::FramebufferManager::Create(width, height, kGLRgba16f, false);
  } else if (m_historyB.width != width || m_historyB.height != height) {
    resources::FramebufferManager::Resize(m_historyB, width, height);
  }

  // M0-A R3: persistent R8 occupancy/depth history ping-pong. The current-frame
  // occupancy (occluder mask) is stored each execute so the next frame can
  // compare occupancy at the reprojected UV for disocclusion rejection.
  const uint32_t occupancyFormat = RenderConstants::V5GI::kOccluderMaskFormat;
  if (!m_occupancyHistoryA.IsValid()) {
    m_occupancyHistoryA =
        resources::FramebufferManager::Create(width, height, occupancyFormat, false);
  } else if (m_occupancyHistoryA.width != width ||
             m_occupancyHistoryA.height != height) {
    resources::FramebufferManager::Resize(m_occupancyHistoryA, width, height);
  }

  if (!m_occupancyHistoryB.IsValid()) {
    m_occupancyHistoryB =
        resources::FramebufferManager::Create(width, height, occupancyFormat, false);
  } else if (m_occupancyHistoryB.width != width ||
             m_occupancyHistoryB.height != height) {
    resources::FramebufferManager::Resize(m_occupancyHistoryB, width, height);
  }

  if (m_cachedWidth != width || m_cachedHeight != height) {
    m_historyValid = false;
    m_cachedWidth = width;
    m_cachedHeight = height;
  }

  return m_outputScene.IsValid() && m_historyA.IsValid() && m_historyB.IsValid() &&
         m_occupancyHistoryA.IsValid() && m_occupancyHistoryB.IsValid();
}

void GICompositePass::Execute(graph::RenderContext &context) {
  if (context.qualityManager == nullptr || context.resources == nullptr ||
      context.camera == nullptr) {
    return;
  }
  const auto &config = context.qualityManager->GetConfig();
  if (!config.giEnabled || !context.hdrSceneBuffer.IsValid() ||
      context.giRadianceTexture == 0u) {
    return;
  }
  if (!m_initialized && !Initialize(*context.resources)) {
    return;
  }

  const int width = context.hdrSceneBuffer.width;
  const int height = context.hdrSceneBuffer.height;
  const int previousWidth = m_cachedWidth;
  const int previousHeight = m_cachedHeight;
  if (!EnsureResources(width, height)) {
    return;
  }
  const bool extentChanged =
      (previousWidth != width || previousHeight != height);

  // T4.1: light/emissive changes must NOT trigger a global history reset.
  // History resets only on extent/resize or uninitialized history; occluder
  // mask changes invalidate the affected screen region via local dirty rects
  // (AD-4) instead of a full reset.
  const bool resetHistory = !m_historyValid || extentChanged;

  float temporalWeight = std::clamp(config.giTemporalWeight, 0.88f, 0.92f);
  Vector2 cameraDeltaUv = {0.0f, 0.0f};
  float zoomRatio = 1.0f;
  if (m_prevCameraValid) {
    const Vector2 delta = {
        context.camera->target.x - m_prevCameraTarget.x,
        context.camera->target.y - m_prevCameraTarget.y,
    };

    const float zoom = std::max(context.camera->zoom, 0.0001f);
    const float prevZoom = std::max(m_prevCameraZoom, 0.0001f);
    zoomRatio = zoom / prevZoom;

    const float worldWidth = static_cast<float>(width) / zoom;
    const float worldHeight = static_cast<float>(height) / zoom;
    cameraDeltaUv.x = (worldWidth > 1e-4f) ? (delta.x / worldWidth) : 0.0f;
    cameraDeltaUv.y = (worldHeight > 1e-4f) ? (delta.y / worldHeight) : 0.0f;
  }

  if (resetHistory) {
    temporalWeight = 0.0f;
  }

  // T4.3: Local invalidation rectangles for changed lights / emissive sources /
  // occluder mask changes. Bounded fixed-capacity buffer (no per-frame heap
  // allocation); on overflow all rects are unioned into a single rect, which
  // matches the previous unbounded-vector union behavior.
  struct DirtyRect {
    float minU;
    float minV;
    float maxU;
    float maxV;
  };
  constexpr size_t kMaxDirtyRects = 16;
  std::array<DirtyRect, kMaxDirtyRects> dirtyRects = {};
  int dirtyRectCount = 0;
  bool dirtyRectsOverflowed = false;
  const auto UnionInto = [](DirtyRect &a, const DirtyRect &b) {
    a.minU = std::min(a.minU, b.minU);
    a.minV = std::min(a.minV, b.minV);
    a.maxU = std::max(a.maxU, b.maxU);
    a.maxV = std::max(a.maxV, b.maxV);
  };
  auto AppendDirtyRect = [&](const DirtyRect &rect) {
    if (dirtyRectCount < static_cast<int>(kMaxDirtyRects)) {
      dirtyRects[dirtyRectCount++] = rect;
    } else if (!dirtyRectsOverflowed) {
      // First overflow: union every stored rect plus this one into slot 0
      // (matches the previous unbounded-vector union-to-single-rect behavior).
      DirtyRect merged = dirtyRects[0];
      for (int i = 1; i < static_cast<int>(kMaxDirtyRects); ++i) {
        UnionInto(merged, dirtyRects[i]);
      }
      UnionInto(merged, rect);
      dirtyRects[0] = merged;
      dirtyRectsOverflowed = true;
    } else {
      // Subsequent overflow: accumulate into the union slot.
      UnionInto(dirtyRects[0], rect);
    }
  };

  const auto &activeLights = lighting::LightManager::Get().GetActiveLightsCpu();
  bool lightsChanged = (activeLights.size() != m_prevActiveLights.size());
  const uint64_t vfxEmissionVersion =
      (m_radianceCascadesPass != nullptr)
          ? m_radianceCascadesPass->GetVfxEmissionSnapshotVersion()
          : 0u;

  if (m_historyValid && !resetHistory) {
    auto AddLightDirtyRect = [&](const components::GPULight &light,
                                 const Camera2D &cam, int w, int h) {
      const float zoom = std::max(cam.zoom, 0.0001f);
      const float screenX = (light.posX - cam.target.x) * zoom + cam.offset.x;
      const float screenY = (light.posY - cam.target.y) * zoom + cam.offset.y;
      const float projectedRadiusPx = light.radius * zoom;
      // Expand screen AABB by 2 * projectedRadiusPx => total half-extent = 3 * projectedRadiusPx
      const float halfExtent = 3.0f * projectedRadiusPx;
      const float minX = screenX - halfExtent;
      const float maxX = screenX + halfExtent;
      const float minY = screenY - halfExtent;
      const float maxY = screenY + halfExtent;
      const float invW = 1.0f / static_cast<float>(w);
      const float invH = 1.0f / static_cast<float>(h);
      const float minU = std::clamp(minX * invW, 0.0f, 1.0f);
      const float minV = std::clamp(minY * invH, 0.0f, 1.0f);
      const float maxU = std::clamp(maxX * invW, 0.0f, 1.0f);
      const float maxV = std::clamp(maxY * invH, 0.0f, 1.0f);
      if (maxU > minU && maxV > minV) {
        AppendDirtyRect({minU, minV, maxU, maxV});
      }
    };

    const size_t commonCount =
        std::min(activeLights.size(), m_prevActiveLights.size());
    for (size_t i = 0; i < commonCount; ++i) {
      const auto &curr = activeLights[i];
      const auto &prev = m_prevActiveLights[i];
      const bool posChanged =
          (curr.posX != prev.posX || curr.posY != prev.posY);
      const bool radiusChanged = (curr.radius != prev.radius);
      const bool intensityChanged = (curr.intensity != prev.intensity);
      const bool colorChanged =
          (curr.colorR != prev.colorR || curr.colorG != prev.colorG ||
           curr.colorB != prev.colorB || curr.colorA != prev.colorA);
      const bool typeChanged =
          (curr.lightType != prev.lightType || curr.flags != prev.flags);

      if (posChanged || radiusChanged || intensityChanged || colorChanged ||
          typeChanged) {
        lightsChanged = true;
        AddLightDirtyRect(curr, *context.camera, width, height);
        if (posChanged && m_prevCameraValid) {
          Camera2D prevCam = *context.camera;
          prevCam.target = m_prevCameraTarget;
          prevCam.zoom = m_prevCameraZoom;
          AddLightDirtyRect(prev, prevCam, width, height);
        }
      }
    }

    for (size_t i = commonCount; i < activeLights.size(); ++i) {
      lightsChanged = true;
      AddLightDirtyRect(activeLights[i], *context.camera, width, height);
    }
    for (size_t i = commonCount; i < m_prevActiveLights.size(); ++i) {
      // Removed light: project under the previous camera (the pose under which
      // it was last rendered) so the rect does not drift with camera motion.
      if (m_prevCameraValid) {
        Camera2D prevCam = *context.camera;
        prevCam.target = m_prevCameraTarget;
        prevCam.zoom = m_prevCameraZoom;
        AddLightDirtyRect(m_prevActiveLights[i], prevCam, width, height);
      } else {
        AddLightDirtyRect(m_prevActiveLights[i], *context.camera, width, height);
      }
    }

    // T4.1/AD-4: occluder mask version change invalidates the occluder screen
    // region (previous + current frame bounds) instead of resetting history.
    const uint64_t occluderMaskVersion =
        (m_occluderExtractPass != nullptr)
            ? m_occluderExtractPass->GetMaskVersion()
            : 0u;
    if (m_prevOccluderMaskVersion != 0u &&
        occluderMaskVersion != m_prevOccluderMaskVersion) {
      const auto AddOccluderBoundsRect = [&](const render::gi::JFARect &bounds) {
        if (bounds.IsEmpty()) {
          // Mask changed but no on-screen occluder bounds: conservative
          // full-screen invalidation.
          AppendDirtyRect({0.0f, 0.0f, 1.0f, 1.0f});
          return;
        }
        const float invW = 1.0f / static_cast<float>(width);
        const float invH = 1.0f / static_cast<float>(height);
        AppendDirtyRect({
            std::clamp(static_cast<float>(bounds.minX) * invW, 0.0f, 1.0f),
            std::clamp(static_cast<float>(bounds.minY) * invH, 0.0f, 1.0f),
            std::clamp(static_cast<float>(bounds.maxX) * invW, 0.0f, 1.0f),
            std::clamp(static_cast<float>(bounds.maxY) * invH, 0.0f, 1.0f),
        });
      };
      AddOccluderBoundsRect(m_occluderExtractPass->GetPreviousOccluderScreenBounds());
      AddOccluderBoundsRect(m_occluderExtractPass->GetCurrentOccluderScreenBounds());
    }

    if (m_prevVfxEmissionSnapshotVersion != 0u &&
        vfxEmissionVersion != m_prevVfxEmissionSnapshotVersion) {
      // AD-4: VFX emissive snapshots are baked into a single combined emissive
      // texture with no per-source world positions available here, so a local
      // projection is not possible. Fallback: full-screen invalidation on
      // emissive snapshot version change (documented design decision).
      AppendDirtyRect({0.0f, 0.0f, 1.0f, 1.0f});
    }
  }

  std::array<float, kMaxDirtyRects * 4> dirtyRectsData = {};
  if (dirtyRectsOverflowed) {
    // Slot 0 already holds the union of every rect; the shader receives one.
    dirtyRectCount = 1;
  }
  for (int i = 0; i < dirtyRectCount; ++i) {
    dirtyRectsData[i * 4 + 0] = dirtyRects[i].minU;
    dirtyRectsData[i * 4 + 1] = dirtyRects[i].minV;
    dirtyRectsData[i * 4 + 2] = dirtyRects[i].maxU;
    dirtyRectsData[i * 4 + 3] = dirtyRects[i].maxV;
  }

  const auto &historyRead = m_readHistoryA ? m_historyA : m_historyB;
  auto &historyWrite = m_readHistoryA ? m_historyB : m_historyA;
  if (!historyRead.IsValid() || !historyWrite.IsValid()) {
    return;
  }

  rlEnableShader(m_compositeShader.id);
  const int sceneResolution[2] = {width, height};
  if (m_sceneResolutionLoc >= 0) {
    rlSetUniform(m_sceneResolutionLoc, sceneResolution, RL_SHADER_UNIFORM_IVEC2, 1);
  }
  if (m_temporalWeightLoc >= 0) {
    rlSetUniform(m_temporalWeightLoc, &temporalWeight, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  const float giIntensity = std::max(0.0f, config.giIntensity);
  if (m_giIntensityLoc >= 0) {
    rlSetUniform(m_giIntensityLoc, &giIntensity, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  const int resetHistoryInt = resetHistory ? 1 : 0;
  if (m_resetHistoryLoc >= 0) {
    rlSetUniform(m_resetHistoryLoc, &resetHistoryInt, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_cameraDeltaUvLoc >= 0) {
    const float deltaArray[2] = {cameraDeltaUv.x, cameraDeltaUv.y};
    rlSetUniform(m_cameraDeltaUvLoc, deltaArray, RL_SHADER_UNIFORM_VEC2, 1);
  }
  if (m_zoomRatioLoc >= 0) {
    rlSetUniform(m_zoomRatioLoc, &zoomRatio, RL_SHADER_UNIFORM_FLOAT, 1);
  }
  if (m_dirtyRectCountLoc >= 0) {
    rlSetUniform(m_dirtyRectCountLoc, &dirtyRectCount, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_dirtyRectsLoc >= 0 && dirtyRectCount > 0) {
    rlSetUniform(m_dirtyRectsLoc, dirtyRectsData.data(), RL_SHADER_UNIFORM_VEC4, dirtyRectCount);
  }

  if (m_radianceTexLoc >= 0) {
    rlSetUniform(m_radianceTexLoc, &kRadianceTexUnit, RL_SHADER_UNIFORM_INT, 1);
  }
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture1);
  // The radiance atlas is a GL_TEXTURE_2D_ARRAY (one layer per direction).
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2DArray, context.giRadianceTexture);

  // AD-3: surface normals for cosine-weighted L0 aggregation come from the
  // height field (HeightShadowPass / GlobalHeightField producer, exposed via
  // context.heightFieldTexture). The texture is optional: when absent the
  // shader runs with uHeightFieldEnabled=0 (no normal weighting), keeping the
  // aggregation valid. The height field is bound on unit 2 and unbound after
  // the dispatch below.
  const uint32_t heightFieldTexture = context.heightFieldTexture;
  const int heightFieldEnabledInt = (heightFieldTexture != 0u) ? 1 : 0;
  if (m_heightFieldTexLoc >= 0) {
    rlSetUniform(m_heightFieldTexLoc, &kHeightFieldTexUnit, RL_SHADER_UNIFORM_INT, 1);
  }
  if (m_heightFieldEnabledLoc >= 0) {
    rlSetUniform(m_heightFieldEnabledLoc, &heightFieldEnabledInt, RL_SHADER_UNIFORM_INT, 1);
  }
  if (heightFieldTexture != 0u) {
    NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture2);
    NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, heightFieldTexture);
    NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture1);
  }

  // AD-3: L0 probe direction count for the cosine-weighted aggregation.
  // context.giRadianceDirections is written by RadianceCascadesPass::Execute
  // (atlas depth of cascade level 0); fall back to 1 when the radiance pass is
  // absent so the shader always samples a valid single direction.
  const uint32_t raysPerProbe =
      (context.giRadianceDirections != 0u)
          ? context.giRadianceDirections
          : ((m_radianceCascadesPass != nullptr)
                 ? m_radianceCascadesPass->GetRadianceDirections()
                 : 1u);
  const int raysPerProbeInt = static_cast<int>(raysPerProbe);
  if (m_raysPerProbeLoc >= 0) {
    rlSetUniform(m_raysPerProbeLoc, &raysPerProbeInt, RL_SHADER_UNIFORM_INT, 1);
  }

  constexpr uint32_t kSceneInBinding = 0u;
  constexpr uint32_t kHistoryInBinding = 2u;
  constexpr uint32_t kSceneOutBinding = 3u;
  constexpr uint32_t kHistoryOutBinding = 4u;
  constexpr uint32_t kOccupancyCurrInBinding = 5u;
  constexpr uint32_t kOccupancyPrevInBinding = 6u;
  constexpr uint32_t kOccupancyOutBinding = 7u;
  NoMoreDay::utils::GPUUtils::BindImageTexture(kSceneInBinding,
                                               context.hdrSceneBuffer.colorTexture, 0,
                                               false, 0, kGLReadOnly, kGLRgba16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kHistoryInBinding,
                                               historyRead.colorTexture, 0, false, 0,
                                               kGLReadOnly, kGLRgba16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kSceneOutBinding,
                                               m_outputScene.colorTexture, 0, false, 0,
                                               kGLWriteOnly, kGLRgba16f);
  NoMoreDay::utils::GPUUtils::BindImageTexture(kHistoryOutBinding,
                                               historyWrite.colorTexture, 0, false, 0,
                                               kGLWriteOnly, kGLRgba16f);

  // M0-A R3: occupancy/depth disocclusion rejection. The current-frame
  // occupancy is the occluder mask texture (GL_R8, 0/1) owned by
  // OccluderExtractPass. Previous occupancy is read from the persistent R8
  // ping-pong history at the reprojected UV; any mismatch zeroes the history
  // weight. The current occupancy is always written to the outgoing history
  // slot for the next frame.
  const uint32_t occupancyFormat = RenderConstants::V5GI::kOccluderMaskFormat;
  const uint32_t currentOccupancyTexture =
      (m_occluderExtractPass != nullptr)
          ? m_occluderExtractPass->GetOccluderMaskTexture()
          : 0u;
  const auto &occupancyRead =
      m_readHistoryA ? m_occupancyHistoryA : m_occupancyHistoryB;
  auto &occupancyWrite = m_readHistoryA ? m_occupancyHistoryB : m_occupancyHistoryA;
  const bool occupancyAvailable = (currentOccupancyTexture != 0u) &&
                                  occupancyRead.IsValid() &&
                                  occupancyWrite.IsValid();
  if (occupancyAvailable) {
    NoMoreDay::utils::GPUUtils::BindImageTexture(kOccupancyCurrInBinding,
                                                 currentOccupancyTexture, 0, false, 0,
                                                 kGLReadOnly, occupancyFormat);
    NoMoreDay::utils::GPUUtils::BindImageTexture(kOccupancyPrevInBinding,
                                                 occupancyRead.colorTexture, 0, false,
                                                 0, kGLReadOnly, occupancyFormat);
    NoMoreDay::utils::GPUUtils::BindImageTexture(kOccupancyOutBinding,
                                                 occupancyWrite.colorTexture, 0, false,
                                                 0, kGLWriteOnly, occupancyFormat);
  }
  const int occupancyEnabledInt = occupancyAvailable ? 1 : 0;
  if (m_occupancyEnabledLoc >= 0) {
    rlSetUniform(m_occupancyEnabledLoc, &occupancyEnabledInt, RL_SHADER_UNIFORM_INT, 1);
  }

  {
    NoMoreDay::utils::GPUUtils::ScopedDebugGroup debugGroup("GIComposite");
    NoMoreDay::utils::GPUUtils::DispatchComputeNoBarrier(
        DivUp(static_cast<uint32_t>(width), kGLComputeGroupSize),
        DivUp(static_cast<uint32_t>(height), kGLComputeGroupSize), 1u);
  }
  rlDisableShader();
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture2);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, 0);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture1);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2DArray, 0);
  NoMoreDay::utils::GPUUtils::ActiveTexture(kGLTexture0);

  // Same-pass sync before the blit reads the composite output: emitted at this
  // exact execution point from the Setup AddPhaseBarrier(Compute, Fragment, ...)
  // declaration.
  context.EmitPhaseBarrier(graph::PipelineStage::Compute,
                           graph::PipelineStage::Fragment);

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLReadFramebuffer, m_outputScene.fbo);
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLDrawFramebuffer,
                                              context.hdrSceneBuffer.fbo);
  rlBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                    static_cast<int>(kGLColorBufferBit));
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer,
                                              context.hdrSceneBuffer.fbo);
  NoMoreDay::utils::GPUUtils::Viewport(0, 0, width, height);

  if (resetHistory) {
    ++m_historyResetCount;
    if (!m_historyValid || extentChanged) {
      m_lastResetReason = "extent";
    } else {
      m_lastResetReason = "initial";
    }
  }

  if (resetHistory && extentChanged) {
    LOG_INFO("GICompositePass: reset temporal history due resolution change ({}x{})", width, height);
  } else if (resetHistory) {
    LOG_INFO("GICompositePass: reset temporal history (initial or explicit invalidation)");
  }

  m_historyValid = true;
  m_readHistoryA = !m_readHistoryA;
  m_prevCameraValid = true;
  m_prevCameraTarget = context.camera->target;
  m_prevCameraZoom = context.camera->zoom;
  if (lightsChanged) {
    m_prevActiveLights = activeLights;
  }
  m_prevOccluderMaskVersion =
      (m_occluderExtractPass != nullptr) ? m_occluderExtractPass->GetMaskVersion() : 0u;
  m_prevVfxEmissionSnapshotVersion = vfxEmissionVersion;
  core::ApplyRlglFlushTemplate();
}

} // namespace NoMoreDay::render::passes

