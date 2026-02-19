#include "doctest.h"

#include "engine/render/MaterialManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "engine/vfx/VFXSequencerSystem.hpp"
#include "game/components/Common.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace {

std::filesystem::path MakeTempDirVfxLightingIntegration(const std::string &name) {
  const std::filesystem::path dir = std::filesystem::path("bin") / name;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

void CleanupDirVfxLightingIntegration(const std::filesystem::path &path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
}

void WriteJsonVfxLightingIntegration(const std::filesystem::path &path,
                                     const nlohmann::json &doc) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out << doc.dump(2);
}

} // namespace

TEST_CASE("[Integration] VFX Lighting - V3 Templates Tier Matrix Execution") {
  using Tier = NoMoreDay::render::core::QualityTier;
  constexpr std::array<const char *, 12> kTemplateNames = {
      "V3Template_MeleeSlashFlash",     "V3Template_MeleeHeavyQuake",
      "V3Template_MeleeComboRhythm",    "V3Template_SpellFireballBurst",
      "V3Template_SpellFrostSpread",    "V3Template_SpellChainLightning",
      "V3Template_AoEPoisonMist",       "V3Template_AoEHolyColumn",
      "V3Template_SummonShadowTeleport","V3Template_SummonElementalFocus",
      "V3Template_EnvTorchIgnite",      "V3Template_EnvWaterRippleBurst",
  };

  auto &manager = NoMoreDay::vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson("assets/vfx/templates/v3") == 12);

  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();
  constexpr std::array<Tier, 4> kTiers = {Tier::Low, Tier::Medium, Tier::High, Tier::Ultra};
  constexpr float kDt = 1.0f / 60.0f;

  for (const Tier tier : kTiers) {
    qm.ForceTier(tier);
    NoMoreDay::vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();

    entt::registry registry;
    std::vector<entt::entity> entities;
    entities.reserve(kTemplateNames.size());

    for (size_t i = 0; i < kTemplateNames.size(); ++i) {
      const char *name = kTemplateNames[i];
      REQUIRE(manager.GetSequenceId(name) >= 0);
      const entt::entity entity = registry.create();
      registry.emplace<Position>(entity, static_cast<float>(i) * 8.0f,
                                 static_cast<float>(i) * 4.0f);
      manager.Play(registry, entity, name, entt::null, false);
      entities.push_back(entity);
    }

    NoMoreDay::vfx::VFXSequencerSystem::Update(registry, kDt);
    CHECK(NoMoreDay::vfx::VFXSequencerSystem::GetActiveShadowPulseCountForTesting() > 0);
    CHECK(NoMoreDay::vfx::VFXSequencerSystem::GetActiveMaterialPhaseShiftCountForTesting() > 0);
    CHECK(NoMoreDay::vfx::VFXSequencerSystem::GetActiveDistortionCountForTesting() <=
          static_cast<size_t>(
              NoMoreDay::render::passes::DistortionPass::MAX_DISTORTION_SOURCES));

    for (int frame = 0; frame < 220; ++frame) {
      NoMoreDay::vfx::VFXSequencerSystem::Update(registry, kDt);
    }

    for (entt::entity entity : entities) {
      CHECK(registry.all_of<NoMoreDay::vfx::VFXPlayerComponent>(entity) == false);
    }
  }

  manager.Shutdown();
}

TEST_CASE("[Integration] VFX Lighting - Schema V2 Compatibility Regression 10 Sequences") {
  constexpr std::array<const char *, 10> kLegacyAssets = {
      "sword_slash.json",  "shadow_nova.json", "fire_explosion.json", "ice_shatter.json",
      "lightning_strike.json", "heal_pulse.json", "critical_hit.json",
      "item_drop_legendary.json", "death_dissolve.json", "blade_formation.json",
  };

  const std::filesystem::path dir =
      MakeTempDirVfxLightingIntegration("tmp_vfx_v2_compat_matrix");
  for (const char *asset : kLegacyAssets) {
    const auto sourcePath = std::filesystem::path("assets/vfx") / asset;
    REQUIRE(std::filesystem::exists(sourcePath));

    std::ifstream in(sourcePath, std::ios::binary);
    REQUIRE(in.is_open());
    nlohmann::json doc = nlohmann::json::parse(in);
    doc["vfx_schema_version"] = 2;
    WriteJsonVfxLightingIntegration(dir / asset, doc);
  }

  auto &manager = NoMoreDay::vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == static_cast<int>(kLegacyAssets.size()));

  auto &qm = NoMoreDay::render::core::QualityTierManager::Get();
  qm.ForceTier(NoMoreDay::render::core::QualityTier::Low);

  entt::registry registry;
  std::vector<entt::entity> entities;
  entities.reserve(kLegacyAssets.size());
  for (size_t i = 0; i < kLegacyAssets.size(); ++i) {
    const auto sourcePath = std::filesystem::path("assets/vfx") / kLegacyAssets[i];
    std::ifstream sourceIn(sourcePath, std::ios::binary);
    REQUIRE(sourceIn.is_open());
    const nlohmann::json original = nlohmann::json::parse(sourceIn);

    const entt::entity entity = registry.create();
    registry.emplace<Position>(entity, static_cast<float>(i) * 3.0f,
                               static_cast<float>(i) * 2.0f);
    const std::string seqName = original.value("name", "");
    REQUIRE(seqName.empty() == false);
    const auto *sequence = manager.GetSequence(seqName);
    REQUIRE(sequence != nullptr);
    CHECK(sequence->version == 2);
    manager.Play(registry, entity, seqName, entt::null, false);
    entities.push_back(entity);
  }

  constexpr float kDt = 1.0f / 60.0f;
  NoMoreDay::vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  for (int frame = 0; frame < 240; ++frame) {
    NoMoreDay::vfx::VFXSequencerSystem::Update(registry, kDt);
  }

  for (entt::entity entity : entities) {
    CHECK(registry.all_of<NoMoreDay::vfx::VFXPlayerComponent>(entity) == false);
  }

  manager.Shutdown();
  CleanupDirVfxLightingIntegration(dir);
}

TEST_CASE("[Integration] VFX Lighting - Preview HotReload Diff Hook") {
  const std::filesystem::path dir =
      MakeTempDirVfxLightingIntegration("tmp_vfx_preview_diff_test");
  const auto beforePath = dir / "before.json";
  const auto afterPath = dir / "after.json";
  const auto diffPath = dir / "preview.diff.txt";

  WriteJsonVfxLightingIntegration(beforePath,
            {{"vfx_schema_version", 3},
             {"name", "PreviewBefore"},
             {"duration", 0.4},
             {"events",
              nlohmann::json::array(
                  {{{"time", 0.0},
                    {"type", "Light"},
                    {"tierPolicy", "skip"},
                    {"params", {{"radius", 80.0}, {"intensity", 1.2}, {"duration", 0.2}}}}})}});

  WriteJsonVfxLightingIntegration(afterPath,
            {{"vfx_schema_version", 3},
             {"name", "PreviewBefore"},
             {"duration", 0.6},
             {"events",
              nlohmann::json::array(
                  {{{"time", 0.0},
                    {"type", "Light"},
                    {"tierPolicy", "skip"},
                    {"params", {{"radius", 100.0}, {"intensity", 1.6}, {"duration", 0.3}}}}})}});

  const std::string command =
      "python tools/vfx_preview/preview_v3.py \"" + beforePath.generic_string() +
      "\" --hot-reload-check \"" + afterPath.generic_string() + "\" --diff-out \"" +
      diffPath.generic_string() + "\"";
  const int rc = std::system(command.c_str());
  CHECK(rc == 0);
  REQUIRE(std::filesystem::exists(diffPath));

  std::ifstream diffIn(diffPath, std::ios::binary);
  REQUIRE(diffIn.is_open());
  const std::string diffText((std::istreambuf_iterator<char>(diffIn)),
                             std::istreambuf_iterator<char>());
  CHECK(diffText.find("--- before") != std::string::npos);
  CHECK(diffText.find("+++ after") != std::string::npos);

  CleanupDirVfxLightingIntegration(dir);
}

TEST_CASE("[Integration] VFX Lighting - MaterialPhaseShift GPU Payload Transition") {
  const std::filesystem::path dir =
      MakeTempDirVfxLightingIntegration("tmp_vfx_phase_shift_payload_transition");

  WriteJsonVfxLightingIntegration(
      dir / "phase_shift_payload.json",
      {{"vfx_schema_version", 3},
       {"name", "PhaseShiftPayload"},
       {"duration", 0.4},
       {"events",
        nlohmann::json::array(
            {{{"time", 0.0},
              {"type", "MaterialPhaseShift"},
              {"tierPolicy", "degrade"},
              {"params",
               {{"roughnessScale", 0.6},
                {"specularScale", 1.4},
                {"emissiveScale", 1.2},
                {"duration", 0.1}}}}})}});

  auto &materials = NoMoreDay::render::MaterialManager::Get();
  materials.Shutdown();
  materials.Initialize();
  const int fireGlowId = materials.GetMaterialId("FireGlow");
  REQUIRE(fireGlowId >= 0);
  const auto baselineGpu = materials.GetGpuMaterialForTesting(fireGlowId);

  auto &manager = NoMoreDay::vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);

  NoMoreDay::render::core::QualityTierManager::Get().ForceTier(
      NoMoreDay::render::core::QualityTier::High);

  entt::registry registry;
  const entt::entity entity = registry.create();
  registry.emplace<Position>(entity, 10.0f, 20.0f);
  manager.Play(registry, entity, "PhaseShiftPayload");

  NoMoreDay::vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  NoMoreDay::vfx::VFXSequencerSystem::Update(registry, 1.0f / 60.0f);
  CHECK(NoMoreDay::vfx::VFXSequencerSystem::GetActiveMaterialPhaseShiftCountForTesting() ==
        1);
  NoMoreDay::vfx::VFXSequencerSystem::Update(registry, 1.0f / 60.0f);
  const auto shiftedGpu = materials.GetGpuMaterialForTesting(fireGlowId);
  CHECK(shiftedGpu.pbrParams.x != doctest::Approx(baselineGpu.pbrParams.x));
  CHECK(shiftedGpu.fresnelControl.x !=
        doctest::Approx(baselineGpu.fresnelControl.x));
  CHECK(shiftedGpu.emissiveAndIntensity.w !=
        doctest::Approx(baselineGpu.emissiveAndIntensity.w));

  NoMoreDay::vfx::VFXSequencerSystem::Update(registry, 0.25f);
  CHECK(NoMoreDay::vfx::VFXSequencerSystem::GetActiveMaterialPhaseShiftCountForTesting() ==
        0);
  const auto restoredGpu = materials.GetGpuMaterialForTesting(fireGlowId);
  CHECK(restoredGpu.pbrParams.x == doctest::Approx(baselineGpu.pbrParams.x));
  CHECK(restoredGpu.fresnelControl.x ==
        doctest::Approx(baselineGpu.fresnelControl.x));
  CHECK(restoredGpu.emissiveAndIntensity.w ==
        doctest::Approx(baselineGpu.emissiveAndIntensity.w));

  manager.Shutdown();
  materials.Shutdown();
  CleanupDirVfxLightingIntegration(dir);
}
