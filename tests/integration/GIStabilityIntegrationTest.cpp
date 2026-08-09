#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "engine/vfx/VFXTypes.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/application/render/EmissiveStampAdapter.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace {

constexpr uint32_t kHdrRgba16f = 0x881A;
constexpr uint32_t kDistanceR16f = 0x822D;

void PopulateGiEntities(entt::registry &registry, const int materialId) {
  registry.clear();
  constexpr int kCount = 64;
  constexpr int kCols = 8;
  constexpr float kStartX = -420.0f;
  constexpr float kStartY = -320.0f;
  constexpr float kSpacing = 88.0f;

  for (int i = 0; i < kCount; ++i) {
    const int row = i / kCols;
    const int col = i % kCols;
    const float jitter = static_cast<float>((i * 13) % 9) - 4.0f;

    const entt::entity entity = registry.create();
    registry.emplace<Position>(entity, kStartX + static_cast<float>(col) * kSpacing + jitter,
                               kStartY + static_cast<float>(row) * kSpacing - jitter);
    registry.emplace<Radius>(entity, 18.0f + static_cast<float>(i % 3) * 6.0f);
    registry.emplace<NoMoreDay::vfx::ActiveMaterialSwap>(
        entity, NoMoreDay::vfx::ActiveMaterialSwap{materialId, 0.25f, 0.25f});
  }
}

} // namespace

TEST_CASE("[Integration] GI - Long-run Stability Proxy (Resize + Tier Switch)") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  auto &qm = render::core::QualityTierManager::Get();
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());

  auto &materialManager = render::MaterialManager::Get();
  materialManager.Shutdown();
  materialManager.Initialize();
  materialManager.LoadFromJson("assets/data/materials_vfx.json");
  int materialId = materialManager.GetMaterialId("FireGlow");
  if (materialId <= 0) {
    materialId = 2;
  }
  REQUIRE(materialId > 0);

  entt::registry registry;
  PopulateGiEntities(registry, materialId);

  ResourceManager resources;

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {960.0f, 540.0f};

  auto hdr = render::resources::FramebufferManager::Create(1920, 1080, kHdrRgba16f, false);
  auto distanceField =
      render::resources::FramebufferManager::Create(1920, 1080, kDistanceR16f, false);
  REQUIRE(hdr.IsValid());
  REQUIRE(distanceField.IsValid());

  render::graph::RenderContext context = {};
  context.registry = &registry;
  context.resources = &resources;
  context.qualityManager = &qm;
  context.camera = &camera;
  context.hdrSceneBuffer = hdr;
  context.giDistanceFieldTexture = distanceField.colorTexture;
  context.giDistanceFieldWidth = distanceField.width;
  context.giDistanceFieldHeight = distanceField.height;

  render::passes::RadianceCascadesPass pass;

  std::vector<NoMoreDay::components::EmissiveStampInput> emissiveStamps;

  constexpr std::array<int, 4> kWidths = {1920, 1600, 1280, 1024};
  constexpr std::array<int, 4> kHeights = {1080, 900, 720, 768};
  constexpr std::array<render::core::QualityTier, 4> kTiers = {
      render::core::QualityTier::Low, render::core::QualityTier::Medium,
      render::core::QualityTier::High, render::core::QualityTier::Ultra};

  std::unordered_set<uint32_t> emissiveIds;
  std::unordered_set<uint32_t> radianceIds;
  uint32_t previousEmissive = 0u;
  uint32_t previousRadiance = 0u;
  int unexpectedReallocs = 0;
  int resizeCount = 0;
  int giEnabledFrames = 0;
  bool resizedThisFrame = false;
  bool giEnabledPrevFrame = false;

  constexpr int kStressFrames = 1800;
  for (int frame = 0; frame < kStressFrames; ++frame) {
    resizedThisFrame = false;
    if ((frame % 60) == 0) {
      const auto tier = kTiers[static_cast<size_t>((frame / 60) % kTiers.size())];
      qm.ForceTier(tier);
      cfg.giEnabled = static_cast<int>(tier) >=
                      static_cast<int>(render::core::QualityTier::High);
      if (cfg.giEnabled) {
        cfg.giCascadeLevels =
            (tier == render::core::QualityTier::Ultra) ? 6u : 4u;
        cfg.giHalfResolution = (tier != render::core::QualityTier::Ultra);
      }
    }

    if ((frame % 120) == 0) {
      const size_t index = static_cast<size_t>((frame / 120) % kWidths.size());
      render::resources::FramebufferManager::Destroy(hdr);
      render::resources::FramebufferManager::Destroy(distanceField);
      hdr = render::resources::FramebufferManager::Create(kWidths[index], kHeights[index],
                                                          kHdrRgba16f, false);
      distanceField = render::resources::FramebufferManager::Create(
          kWidths[index], kHeights[index], kDistanceR16f, false);
      REQUIRE(hdr.IsValid());
      REQUIRE(distanceField.IsValid());
      context.hdrSceneBuffer = hdr;
      context.giDistanceFieldTexture = distanceField.colorTexture;
      context.giDistanceFieldWidth = distanceField.width;
      context.giDistanceFieldHeight = distanceField.height;
      pass.OnResize(kWidths[index], kHeights[index]);
      resizedThisFrame = true;
      ++resizeCount;
    }

    camera.target.x = std::sin(static_cast<float>(frame) * 0.02f) * 260.0f;
    camera.target.y = std::cos(static_cast<float>(frame) * 0.015f) * 220.0f;
    cfg.giHolographicEnabled = ((frame / 45) % 2) == 1;

    context.giEmissiveTexture = 0u;
    context.giRadianceTexture = 0u;
    pass.PrepareVfxEmissionSnapshot(context);
    NoMoreDay::EmissiveProjection emissiveProjection =
        NoMoreDay::EmissiveStampAdapter::BuildEmissiveStamps(registry);
    emissiveStamps = std::move(emissiveProjection.stamps);
    context.emissiveStamps =
        emissiveStamps.empty() ? nullptr : emissiveStamps.data();
    context.emissiveStampCount = static_cast<uint32_t>(emissiveStamps.size());
    pass.Execute(context);

    if (cfg.giEnabled) {
      ++giEnabledFrames;
      CHECK(context.giEmissiveTexture != 0u);
      CHECK(context.giRadianceTexture != 0u);

      emissiveIds.insert(context.giEmissiveTexture);
      radianceIds.insert(context.giRadianceTexture);

      if (!resizedThisFrame && giEnabledPrevFrame) {
        if (previousEmissive != 0u && context.giEmissiveTexture != previousEmissive) {
          ++unexpectedReallocs;
        }
        if (previousRadiance != 0u && context.giRadianceTexture != previousRadiance) {
          ++unexpectedReallocs;
        }
      }

      previousEmissive = context.giEmissiveTexture;
      previousRadiance = context.giRadianceTexture;
      giEnabledPrevFrame = true;
    } else {
      giEnabledPrevFrame = false;
      previousEmissive = 0u;
      previousRadiance = 0u;
    }
  }

  CHECK(giEnabledFrames > 300);
  CHECK(resizeCount >= 10);
  CHECK(unexpectedReallocs == 0);
  CHECK(static_cast<int>(emissiveIds.size()) <= (resizeCount + 4));
  CHECK(static_cast<int>(radianceIds.size()) <= (resizeCount + 4));

  pass.Shutdown();
  render::resources::FramebufferManager::Destroy(distanceField);
  render::resources::FramebufferManager::Destroy(hdr);
  materialManager.Shutdown();
}
