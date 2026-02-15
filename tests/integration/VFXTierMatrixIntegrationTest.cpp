#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "engine/vfx/VFXSequencerSystem.hpp"
#include "game/components/Common.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace {

std::filesystem::path MakeTempVfxDir(const std::string &name) {
  const std::filesystem::path dir = std::filesystem::path("bin") / name;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

void WriteMatrixSequence(const std::filesystem::path &filePath, float duration) {
  using nlohmann::json;
  const json doc = {
      {"vfx_schema_version", 1},
      {"name", "VFXTierMatrix"},
      {"duration", duration},
      {"events",
       json::array(
           {{{"time", 0.0},
             {"type", "MaterialSwap"},
             {"anchor", "Caster"},
             {"minTier", "Medium"},
             {"params", {{"materialId", 7}, {"duration", 0.12}}}},
            {{"time", 0.0},
             {"type", "Distortion"},
             {"anchor", "Caster"},
             {"minTier", "Low"},
             {"params",
              {{"radius", 140.0}, {"strength", 0.35}, {"duration", 0.3}, {"speed", 300.0}}}}})},
  };

  std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out << doc.dump(2);
}

void CleanupDir(const std::filesystem::path &path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
}

} // namespace

TEST_CASE("[Integration] VFX - Tier Matrix Load/Reload/Resize/Restore") {
  using namespace NoMoreDay;
  using Tier = render::core::QualityTier;

  const auto dir = MakeTempVfxDir("tmp_vfx_tier_matrix");
  const auto file = dir / "matrix.json";
  WriteMatrixSequence(file, 0.25f);

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);

  auto &qm = render::core::QualityTierManager::Get();
  const Tier originalTier = qm.GetTier();
  const bool originalHotReload = qm.GetConfig().hotReloadEnabled;

  entt::registry registry;
  const entt::entity entity = registry.create();
  registry.emplace<Position>(entity, 0.0f, 0.0f);

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  Camera2D camera = {};
  camera.zoom = 1.0f;
  camera.target = {0.0f, 0.0f};
  camera.offset = {0.0f, 0.0f};

  constexpr Tier kTiers[4] = {Tier::Low, Tier::Medium, Tier::High, Tier::Ultra};
  for (const Tier tier : kTiers) {
    qm.ForceTier(tier);
    render::core::RenderConfig &tierCfg =
        const_cast<render::core::RenderConfig &>(qm.GetConfig());
    tierCfg.hotReloadEnabled = true;
    vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();

    manager.Play(registry, entity, "VFXTierMatrix");
    vfx::VFXSequencerSystem::Update(registry, 1.0f / 60.0f);

    const bool expectMaterialSwap = static_cast<int>(tier) >= static_cast<int>(Tier::Medium);
    if (expectMaterialSwap) {
      CHECK(vfx::VFXSequencerSystem::GetActiveMaterialSwapCountForTesting() > 0);
    } else {
      CHECK(vfx::VFXSequencerSystem::GetActiveMaterialSwapCountForTesting() == 0);
    }
    CHECK(vfx::VFXSequencerSystem::GetActiveDistortionCountForTesting() <=
          static_cast<size_t>(
              render::passes::DistortionPass::MAX_DISTORTION_SOURCES));

    auto *sequence = manager.GetSequence("VFXTierMatrix");
    REQUIRE(sequence != nullptr);
    const float nextDuration = sequence->duration + 0.05f;
    WriteMatrixSequence(file, nextDuration);
    const auto previousTimestamp = std::filesystem::last_write_time(file);
    std::filesystem::last_write_time(file,
                                     previousTimestamp + std::chrono::seconds(2));
    manager.TryHotReload();
    const auto *reloaded = manager.GetSequence("VFXTierMatrix");
    REQUIRE(reloaded != nullptr);
    CHECK(reloaded->duration == doctest::Approx(nextDuration));

    if (utils::GPUUtils::IsInitialized()) {
      render::passes::DistortionPass pass;
      REQUIRE(pass.Initialize());
      auto input = render::resources::FramebufferManager::Create(1280, 720, 0x8058, false);
      REQUIRE(input.IsValid());

      render::graph::RenderContext context = {};
      context.qualityManager = &qm;
      context.camera = &camera;
      pass.SetInputBuffer(&input);
      pass.AddDistortionSource(0.0f, 0.0f, 120.0f, 0.35f);
      pass.Execute(context);

      pass.OnResize(1600, 900);
      pass.AddDistortionSource(20.0f, 10.0f, 90.0f, 0.30f);
      pass.Execute(context);

      pass.Shutdown();
      CHECK(pass.Initialize()); // context restore equivalent
      pass.Shutdown();
      render::resources::FramebufferManager::Destroy(input);
    }
  }

  qm.ForceTier(originalTier);
  render::core::RenderConfig &restoredCfg =
      const_cast<render::core::RenderConfig &>(qm.GetConfig());
  restoredCfg.hotReloadEnabled = originalHotReload;
  manager.Shutdown();
  CleanupDir(dir);
}
