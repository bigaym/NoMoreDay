#include "doctest.h"

#include "engine/render/MaterialManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/vfx/VFXBudgetEstimator.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "engine/vfx/VFXSequencerSystem.hpp"
#include "game/components/Common.hpp"

#include <entt/entt.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

using namespace NoMoreDay;

namespace {

std::filesystem::path MakeTempVfxDir(const std::string &name) {
  const std::filesystem::path dir = std::filesystem::path("bin") / name;
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

void WriteTextFile(const std::filesystem::path &path, const std::string &text) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  REQUIRE(file.is_open());
  file << text;
}

void CleanupDir(const std::filesystem::path &path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
}

} // namespace

TEST_CASE("[Unit] VFXSequencer - Load Parse Query") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_parse");
  const std::filesystem::path file = dir / "sword_slash.json";
  WriteTextFile(file,
                R"({
  "vfx_schema_version": 1,
  "name": "SwordSlash",
  "duration": 0.6,
  "minTier": "Low",
  "events": [
    { "time": 0.30, "type": "Particle", "anchor": "Caster",
      "params": { "materialId": "FireGlow", "count": 8 } },
    { "time": 0.10, "type": "Light", "anchor": "Caster",
      "params": { "radius": 80.0, "intensity": 2.0, "color": [1.0, 0.8, 0.6] } }
  ]
})");

  render::MaterialManager::Get().Shutdown();
  render::MaterialManager::Get().Initialize();

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson(dir.string());
  CHECK(loaded == 1);
  CHECK(manager.GetSequenceCount() == 1);
  CHECK(manager.GetSequenceId("SwordSlash") == 0);

  const vfx::VFXSequenceAsset *seq = manager.GetSequence("SwordSlash");
  REQUIRE(seq != nullptr);
  CHECK(seq->duration == doctest::Approx(0.6f));
  REQUIRE(seq->events.size() == 2);
  CHECK(seq->events[0].time <= seq->events[1].time);
  CHECK(seq->events[0].type == vfx::EventType::Light);
  CHECK(seq->events[1].type == vfx::EventType::Particle);

  const auto *particle =
      std::get_if<vfx::ParticleEventParams>(&seq->events[1].params);
  REQUIRE(particle != nullptr);
  CHECK(particle->materialId == render::MaterialManager::Get().GetMaterialId("FireGlow"));

  manager.Shutdown();
  render::MaterialManager::Get().Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - Play And Stop") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_play");
  const std::filesystem::path file = dir / "play.json";
  WriteTextFile(file,
                R"({
  "vfx_schema_version": 1,
  "name": "PlayMe",
  "duration": 0.5,
  "events": []
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);

  entt::registry registry;
  const entt::entity entity = registry.create();

  manager.Play(registry, entity, "PlayMe", entt::null, true);
  REQUIRE(registry.all_of<vfx::VFXPlayerComponent>(entity));

  const auto &player = registry.get<vfx::VFXPlayerComponent>(entity);
  CHECK(player.sequenceId == 0);
  CHECK(player.nextEventIdx == 0);
  CHECK(player.elapsed == doctest::Approx(0.0f));
  CHECK(player.loop == true);
  CHECK(player.active == true);

  manager.Stop(registry, entity);
  CHECK(registry.all_of<vfx::VFXPlayerComponent>(entity) == false);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - Hot Reload") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_reload");
  const std::filesystem::path file = dir / "reload.json";

  WriteTextFile(file,
                R"({
  "vfx_schema_version": 1,
  "name": "ReloadSeq",
  "duration": 0.5,
  "events": []
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);
  auto &qualityManager = render::core::QualityTierManager::Get();
  qualityManager.Initialize("settings.json");
  auto &config =
      const_cast<render::core::RenderConfig &>(qualityManager.GetConfig());
  const bool oldHotReloadEnabled = config.hotReloadEnabled;
  config.hotReloadEnabled = true;

  const vfx::VFXSequenceAsset *before = manager.GetSequence("ReloadSeq");
  REQUIRE(before != nullptr);
  CHECK(before->duration == doctest::Approx(0.5f));
  const auto beforeWriteTime = std::filesystem::last_write_time(file);

  WriteTextFile(file,
                R"({
  "vfx_schema_version": 1,
  "name": "ReloadSeq",
  "duration": 1.25,
  "events": []
})");

  // Forcefully advance the write time to ensure it's detectable
  // Some file systems have low resolution for last_write_time
  const auto newWriteTime = beforeWriteTime + std::chrono::seconds(2);
  std::filesystem::last_write_time(file, newWriteTime);

  bool timestampChanged = false;
  for (int attempt = 0; attempt < 20; ++attempt) {
    const auto nowWriteTime = std::filesystem::last_write_time(file);
    if (nowWriteTime > beforeWriteTime) {
      timestampChanged = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  REQUIRE(timestampChanged);

  manager.TryHotReload();

  const vfx::VFXSequenceAsset *after = manager.GetSequence("ReloadSeq");
  REQUIRE(after != nullptr);
  CHECK(after->duration == doctest::Approx(1.25f));

  config.hotReloadEnabled = oldHotReloadEnabled;
  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - Invalid Schema Fallback") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_invalid");
  const std::filesystem::path file = dir / "invalid.json";
  WriteTextFile(file,
                R"({
  "vfx_schema_version": 99,
  "name": "Invalid",
  "duration": 1.0,
  "events": []
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson(dir.string());
  CHECK(loaded == 0);
  CHECK(manager.GetSequenceCount() == 0);
  CHECK(manager.GetSequence("Invalid") == nullptr);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - Missing Schema Version Is Rejected") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_missing_schema");
  const std::filesystem::path file = dir / "missing_schema.json";
  WriteTextFile(file,
                R"({
  "name": "MissingSchema",
  "duration": 1.0,
  "events": []
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson(dir.string());
  CHECK(loaded == 0);
  CHECK(manager.GetSequence("MissingSchema") == nullptr);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - SchemaV3 Parse TierPolicy And New Events") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_schema_v3");
  WriteTextFile(
      dir / "schema_v3.json",
      R"({
  "vfx_schema_version": 3,
  "name": "SchemaV3Seq",
  "duration": 0.6,
  "events": [
    {
      "time": 0.00,
      "type": "ShadowPulse",
      "tierPolicy": "degrade",
      "params": { "softnessScale": 1.4, "intensityScale": 1.2, "duration": 0.4 }
    },
    {
      "time": 0.10,
      "type": "LightProfileBlend",
      "tierPolicy": "strict",
      "params": { "profileA": 1, "profileB": 3, "blendTime": 0.3 }
    },
    {
      "time": 0.20,
      "type": "MaterialPhaseShift",
      "tierPolicy": "skip",
      "params": {
        "roughnessScale": 0.7,
        "specularScale": 1.5,
        "emissiveScale": 1.2,
        "duration": 0.5
      }
    }
  ]
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);

  const vfx::VFXSequenceAsset *sequence = manager.GetSequence("SchemaV3Seq");
  REQUIRE(sequence != nullptr);
  CHECK(sequence->version == 3);
  REQUIRE(sequence->events.size() == 3);

  CHECK(sequence->events[0].type == vfx::EventType::ShadowPulse);
  CHECK(sequence->events[0].tierPolicy == vfx::TierPolicy::Degrade);
  const auto *shadowPulse =
      std::get_if<vfx::ShadowPulseParams>(&sequence->events[0].params);
  REQUIRE(shadowPulse != nullptr);
  CHECK(shadowPulse->softnessScale == doctest::Approx(1.4f));

  CHECK(sequence->events[1].type == vfx::EventType::LightProfileBlend);
  CHECK(sequence->events[1].tierPolicy == vfx::TierPolicy::Strict);
  const auto *lightBlend =
      std::get_if<vfx::LightProfileBlendParams>(&sequence->events[1].params);
  REQUIRE(lightBlend != nullptr);
  CHECK(lightBlend->profileA == 1u);
  CHECK(lightBlend->profileB == 3u);

  CHECK(sequence->events[2].type == vfx::EventType::MaterialPhaseShift);
  CHECK(sequence->events[2].tierPolicy == vfx::TierPolicy::Skip);
  const auto *phaseShift =
      std::get_if<vfx::MaterialPhaseShiftParams>(&sequence->events[2].params);
  REQUIRE(phaseShift != nullptr);
  CHECK(phaseShift->specularScale == doctest::Approx(1.5f));

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - SchemaV2 Compatibility Defaults TierPolicy") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_schema_v2_compat");
  WriteTextFile(
      dir / "schema_v2.json",
      R"({
  "vfx_schema_version": 2,
  "name": "SchemaV2Compat",
  "duration": 0.2,
  "events": [
    { "time": 0.0, "type": "Light",
      "params": { "radius": 64.0, "intensity": 1.8, "duration": 0.2 } }
  ]
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);

  const vfx::VFXSequenceAsset *sequence = manager.GetSequence("SchemaV2Compat");
  REQUIRE(sequence != nullptr);
  CHECK(sequence->version == 2);
  REQUIRE(sequence->events.size() == 1);
  CHECK(sequence->events[0].tierPolicy == vfx::TierPolicy::Skip);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - Invalid TierPolicy And Payload Rejected") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_v3_invalid");

  WriteTextFile(
      dir / "invalid_policy.json",
      R"({
  "vfx_schema_version": 3,
  "name": "InvalidPolicy",
  "duration": 0.3,
  "events": [
    { "time": 0.0, "type": "ShadowPulse", "tierPolicy": "hard_fail",
      "params": { "softnessScale": 1.1, "intensityScale": 1.1, "duration": 0.2 } }
  ]
})");

  WriteTextFile(
      dir / "invalid_payload.json",
      R"({
  "vfx_schema_version": 3,
  "name": "InvalidPayload",
  "duration": 0.3,
  "events": [
    { "time": 0.0, "type": "MaterialPhaseShift", "tierPolicy": "degrade",
      "params": { "roughnessScale": 0.7, "specularScale": 1.1, "emissiveScale": 1.0, "duration": 0.0 } }
  ]
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  CHECK(manager.LoadFromJson(dir.string()) == 0);
  CHECK(manager.GetSequence("InvalidPolicy") == nullptr);
  CHECK(manager.GetSequence("InvalidPayload") == nullptr);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - Player Advance Loop And End") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_runtime");
  WriteTextFile(
      dir / "advance.json",
      R"({
  "vfx_schema_version": 1,
  "name": "AdvanceSeq",
  "duration": 0.2,
  "events": [
    { "time": 0.05, "type": "Shake", "params": { "intensity": 0.05 } },
    { "time": 0.10, "type": "Shake", "params": { "intensity": 0.05 } }
  ]
})");
  WriteTextFile(
      dir / "loop.json",
      R"({
  "vfx_schema_version": 1,
  "name": "LoopSeq",
  "duration": 0.1,
  "events": [
    { "time": 0.05, "type": "Shake", "params": { "intensity": 0.05 } }
  ]
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 2);

  entt::registry registry;

  const entt::entity oneShot = registry.create();
  registry.emplace<Position>(oneShot, 0.0f, 0.0f);
  manager.Play(registry, oneShot, "AdvanceSeq", entt::null, false);
  REQUIRE(registry.all_of<vfx::VFXPlayerComponent>(oneShot));

  vfx::VFXSequencerSystem::Update(registry, 0.06f);
  REQUIRE(registry.all_of<vfx::VFXPlayerComponent>(oneShot));
  CHECK(registry.get<vfx::VFXPlayerComponent>(oneShot).nextEventIdx == 1);

  vfx::VFXSequencerSystem::Update(registry, 0.06f);
  REQUIRE(registry.all_of<vfx::VFXPlayerComponent>(oneShot));
  CHECK(registry.get<vfx::VFXPlayerComponent>(oneShot).nextEventIdx == 2);

  vfx::VFXSequencerSystem::Update(registry, 0.10f);
  CHECK(registry.all_of<vfx::VFXPlayerComponent>(oneShot) == false);

  const entt::entity looped = registry.create();
  registry.emplace<Position>(looped, 0.0f, 0.0f);
  manager.Play(registry, looped, "LoopSeq", entt::null, true);
  REQUIRE(registry.all_of<vfx::VFXPlayerComponent>(looped));

  vfx::VFXSequencerSystem::Update(registry, 0.15f);
  REQUIRE(registry.all_of<vfx::VFXPlayerComponent>(looped));
  const auto &loopPlayer = registry.get<vfx::VFXPlayerComponent>(looped);
  CHECK(loopPlayer.nextEventIdx == 0);
  CHECK(loopPlayer.elapsed < 0.1f);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - QualityTier Filtering") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_quality");
  WriteTextFile(
      dir / "quality.json",
      R"({
  "vfx_schema_version": 1,
  "name": "TierSeq",
  "duration": 0.2,
  "events": [
    { "time": 0.00, "type": "Shake", "minTier": "High",
      "params": { "intensity": 0.20 } }
  ]
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);

  render::core::QualityTierManager::Get().Initialize("settings.json");
  render::core::QualityTierManager::Get().ForceTier(
      render::core::QualityTier::Low);

  entt::registry registry;
  const entt::entity entity = registry.create();
  registry.emplace<Position>(entity, 0.0f, 0.0f);
  manager.Play(registry, entity, "TierSeq");

  REQUIRE(registry.all_of<vfx::VFXPlayerComponent>(entity));
  vfx::VFXSequencerSystem::Update(registry, 0.01f);
  REQUIRE(registry.all_of<vfx::VFXPlayerComponent>(entity));
  CHECK(registry.get<vfx::VFXPlayerComponent>(entity).nextEventIdx == 1);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - MaterialSwap Runtime Lifetime") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_material_swap");
  WriteTextFile(
      dir / "material_swap.json",
      R"({
  "vfx_schema_version": 1,
  "name": "MaterialSwapSeq",
  "duration": 0.2,
  "events": [
    { "time": 0.00, "type": "MaterialSwap", "anchor": "Caster",
      "params": { "materialId": 7, "duration": 0.05 } }
  ]
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);

  render::core::QualityTierManager::Get().ForceTier(
      render::core::QualityTier::High);

  entt::registry registry;
  const entt::entity entity = registry.create();
  registry.emplace<Position>(entity, 0.0f, 0.0f);
  manager.Play(registry, entity, "MaterialSwapSeq");

  vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  vfx::VFXSequencerSystem::Update(registry, 0.01f);
  CHECK(registry.all_of<vfx::ActiveMaterialSwap>(entity));

  vfx::VFXSequencerSystem::Update(registry, 0.10f);
  CHECK(registry.all_of<vfx::ActiveMaterialSwap>(entity) == false);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - MaterialSwap Fallback On Low Detail") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_material_fallback");
  WriteTextFile(
      dir / "material_swap_low.json",
      R"({
  "vfx_schema_version": 1,
  "name": "MaterialSwapLow",
  "duration": 0.2,
  "events": [
    { "time": 0.00, "type": "MaterialSwap", "anchor": "Caster",
      "params": { "materialId": 7, "duration": 0.10 } }
  ]
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);

  render::core::QualityTierManager::Get().ForceTier(
      render::core::QualityTier::Low);

  entt::registry registry;
  const entt::entity entity = registry.create();
  registry.emplace<Position>(entity, 0.0f, 0.0f);
  manager.Play(registry, entity, "MaterialSwapLow");

  vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  vfx::VFXSequencerSystem::Update(registry, 0.01f);
  CHECK(registry.all_of<vfx::ActiveMaterialSwap>(entity) == false);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - TierPolicy Strict Degrade Skip Runtime") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_tier_policy_runtime");

  WriteTextFile(
      dir / "strict.json",
      R"({
  "vfx_schema_version": 3,
  "name": "StrictSeq",
  "duration": 0.4,
  "events": [
    { "time": 0.00, "type": "ShadowPulse", "minTier": "Ultra", "tierPolicy": "strict",
      "params": { "softnessScale": 1.5, "intensityScale": 1.3, "duration": 0.2 } },
    { "time": 0.05, "type": "Shake", "params": { "intensity": 0.1 } }
  ]
})");

  WriteTextFile(
      dir / "degrade.json",
      R"({
  "vfx_schema_version": 3,
  "name": "DegradeSeq",
  "duration": 0.4,
  "events": [
    { "time": 0.00, "type": "ShadowPulse", "minTier": "Ultra", "tierPolicy": "degrade",
      "params": { "softnessScale": 1.5, "intensityScale": 1.3, "duration": 0.2 } }
  ]
})");

  WriteTextFile(
      dir / "skip.json",
      R"({
  "vfx_schema_version": 3,
  "name": "SkipSeq",
  "duration": 0.4,
  "events": [
    { "time": 0.00, "type": "ShadowPulse", "minTier": "Ultra", "tierPolicy": "skip",
      "params": { "softnessScale": 1.5, "intensityScale": 1.3, "duration": 0.2 } }
  ]
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 3);

  auto &tierManager = render::core::QualityTierManager::Get();
  tierManager.ForceTier(render::core::QualityTier::Low);

  entt::registry registry;
  const entt::entity strictEntity = registry.create();
  registry.emplace<Position>(strictEntity, 0.0f, 0.0f);
  manager.Play(registry, strictEntity, "StrictSeq");

  vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  vfx::VFXSequencerSystem::Update(registry, 0.02f);
  CHECK(registry.all_of<vfx::VFXPlayerComponent>(strictEntity) == false);

  const entt::entity degradeEntity = registry.create();
  registry.emplace<Position>(degradeEntity, 0.0f, 0.0f);
  manager.Play(registry, degradeEntity, "DegradeSeq");
  vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  vfx::VFXSequencerSystem::Update(registry, 0.02f);
  CHECK(vfx::VFXSequencerSystem::GetActiveShadowPulseCountForTesting() == 1);

  const entt::entity skipEntity = registry.create();
  registry.emplace<Position>(skipEntity, 0.0f, 0.0f);
  manager.Play(registry, skipEntity, "SkipSeq");
  vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  vfx::VFXSequencerSystem::Update(registry, 0.02f);
  CHECK(vfx::VFXSequencerSystem::GetActiveShadowPulseCountForTesting() == 0);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - MaterialPhaseShift Runtime Lifecycle") {
  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_phase_shift");
  WriteTextFile(
      dir / "phase_shift.json",
      R"({
  "vfx_schema_version": 3,
  "name": "PhaseShiftSeq",
  "duration": 0.4,
  "events": [
    { "time": 0.00, "type": "MaterialPhaseShift", "tierPolicy": "degrade",
      "params": { "roughnessScale": 0.6, "specularScale": 1.4, "emissiveScale": 1.2, "duration": 0.1 } }
  ]
})");

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);
  render::core::QualityTierManager::Get().ForceTier(render::core::QualityTier::High);

  auto &materials = render::MaterialManager::Get();
  materials.Shutdown();
  materials.Initialize();
  const int fireGlowId = materials.GetMaterialId("FireGlow");
  REQUIRE(fireGlowId >= 0);
  const auto baselineGpu = materials.GetGpuMaterialForTesting(fireGlowId);

  entt::registry registry;
  const entt::entity entity = registry.create();
  registry.emplace<Position>(entity, 0.0f, 0.0f);
  manager.Play(registry, entity, "PhaseShiftSeq");

  vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  vfx::VFXSequencerSystem::Update(registry, 0.02f);
  CHECK(vfx::VFXSequencerSystem::GetActiveMaterialPhaseShiftCountForTesting() == 1);
  vfx::VFXSequencerSystem::Update(registry, 0.02f);
  const auto shiftedGpu = materials.GetGpuMaterialForTesting(fireGlowId);
  CHECK(shiftedGpu.pbrLite.x != doctest::Approx(baselineGpu.pbrLite.x));
  CHECK(shiftedGpu.pbrLite.y != doctest::Approx(baselineGpu.pbrLite.y));
  CHECK(shiftedGpu.emissiveAndIntensity.w !=
        doctest::Approx(baselineGpu.emissiveAndIntensity.w));

  vfx::VFXSequencerSystem::Update(registry, 0.20f);
  CHECK(vfx::VFXSequencerSystem::GetActiveMaterialPhaseShiftCountForTesting() == 0);
  const auto restoredGpu = materials.GetGpuMaterialForTesting(fireGlowId);
  CHECK(restoredGpu.pbrLite.x == doctest::Approx(baselineGpu.pbrLite.x));
  CHECK(restoredGpu.pbrLite.y == doctest::Approx(baselineGpu.pbrLite.y));
  CHECK(restoredGpu.emissiveAndIntensity.w ==
        doctest::Approx(baselineGpu.emissiveAndIntensity.w));

  manager.Shutdown();
  materials.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXBudgetEstimator - JSON And Summary") {
  vfx::VFXSequenceAsset sequence = {};
  sequence.name = "BudgetSeq";
  sequence.version = 3;

  vfx::VFXEvent particleEvent = {};
  particleEvent.type = vfx::EventType::Particle;
  vfx::ParticleEventParams particleParams = {};
  particleParams.materialId = 2;
  particleParams.count = 24;
  particleParams.speed = 160.0f;
  particleParams.speedVariance = 40.0f;
  particleParams.lifetime = 0.7f;
  particleParams.scale = 1.1f;
  particleEvent.params = particleParams;
  sequence.events.push_back(particleEvent);

  vfx::VFXEvent shadowEvent = {};
  shadowEvent.type = vfx::EventType::ShadowPulse;
  vfx::ShadowPulseParams shadowParams = {};
  shadowParams.softnessScale = 1.3f;
  shadowParams.intensityScale = 1.2f;
  shadowParams.duration = 0.3f;
  shadowEvent.params = shadowParams;
  sequence.events.push_back(shadowEvent);

  const vfx::VFXBudgetReport report =
      vfx::VFXBudgetEstimator::Analyze(sequence, 10.0f);
  CHECK(report.eventCount == 2);
  CHECK(report.totalCost > 0.0f);
  CHECK(report.overBudget == true);

  const nlohmann::json reportJson = vfx::VFXBudgetEstimator::ToJson(report);
  CHECK(reportJson["sequence"].get<std::string>() == "BudgetSeq");
  CHECK(reportJson["cost"]["total"].get<float>() ==
        doctest::Approx(report.totalCost));

  const std::string summary = vfx::VFXBudgetEstimator::BuildConsoleSummary(report);
  CHECK(summary.find("BudgetSeq") != std::string::npos);
  CHECK(summary.find("overBudget=true") != std::string::npos);
}

TEST_CASE("[Unit] VFXSequencer - Distortion Overflow Deterministic Cap") {
  using nlohmann::json;

  const std::filesystem::path dir = MakeTempVfxDir("tmp_vfx_seq_distortion_cap");
  constexpr int kEventCount =
      NoMoreDay::render::passes::DistortionPass::MAX_DISTORTION_SOURCES + 12;

  json events = json::array();
  for (int i = 0; i < kEventCount; ++i) {
    events.push_back({
        {"time", 0.0},
        {"type", "Distortion"},
        {"anchor", "Caster"},
        {"params",
         {{"radius", 140.0 + i}, {"strength", 0.2 + i * 0.01}, {"duration", 0.4}, {"speed", 300.0}}},
    });
  }

  const json document = {
      {"vfx_schema_version", 1},
      {"name", "DistortionOverflow"},
      {"duration", 0.5},
      {"events", events},
  };
  WriteTextFile((dir / "distortion_overflow.json"), document.dump(2));

  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();
  REQUIRE(manager.LoadFromJson(dir.string()) == 1);

  render::core::QualityTierManager::Get().ForceTier(
      render::core::QualityTier::Ultra);

  entt::registry registry;
  const entt::entity entity = registry.create();
  registry.emplace<Position>(entity, 0.0f, 0.0f);
  manager.Play(registry, entity, "DistortionOverflow");

  vfx::VFXSequencerSystem::ResetRuntimeStateForTesting();
  vfx::VFXSequencerSystem::Update(registry, 0.01f);

  CHECK(vfx::VFXSequencerSystem::GetActiveDistortionCountForTesting() ==
        static_cast<size_t>(
            NoMoreDay::render::passes::DistortionPass::MAX_DISTORTION_SOURCES));
  CHECK(vfx::VFXSequencerSystem::GetDistortionOverflowDropCountForTesting() +
            vfx::VFXSequencerSystem::GetDistortionOverflowEvictCountForTesting() >
        0);

  manager.Shutdown();
  CleanupDir(dir);
}

TEST_CASE("[Unit] VFXSequencer - Asset Stress Sequences Available") {
  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson("assets/vfx");
  REQUIRE(loaded > 0);

  const int materialSwapId = manager.GetSequenceId("MaterialSwapCombo");
  const int distortionStressId = manager.GetSequenceId("DistortionOverflowStress");
  REQUIRE(materialSwapId >= 0);
  REQUIRE(distortionStressId >= 0);

  const auto *materialSwap = manager.GetSequence(materialSwapId);
  const auto *distortionStress = manager.GetSequence(distortionStressId);
  REQUIRE(materialSwap != nullptr);
  REQUIRE(distortionStress != nullptr);
  CHECK(materialSwap->events.size() >= 3);
  CHECK(distortionStress->events.size() >
        static_cast<size_t>(
            NoMoreDay::render::passes::DistortionPass::MAX_DISTORTION_SOURCES));

  manager.Shutdown();
}

TEST_CASE("[Unit] VFXSequencer - V3 Templates Load And Budget Within Threshold") {
  auto &manager = vfx::VFXSequenceManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson("assets/vfx/templates/v3");
  REQUIRE(loaded == 12);
  CHECK(manager.GetSequenceCount() == 12);

  constexpr float kTemplateBudgetThreshold = 1200.0f;
  const std::array<const char *, 12> names = {
      "V3Template_MeleeSlashFlash",
      "V3Template_MeleeHeavyQuake",
      "V3Template_MeleeComboRhythm",
      "V3Template_SpellFireballBurst",
      "V3Template_SpellFrostSpread",
      "V3Template_SpellChainLightning",
      "V3Template_AoEPoisonMist",
      "V3Template_AoEHolyColumn",
      "V3Template_SummonShadowTeleport",
      "V3Template_SummonElementalFocus",
      "V3Template_EnvTorchIgnite",
      "V3Template_EnvWaterRippleBurst",
  };

  for (const char *name : names) {
    const auto *sequence = manager.GetSequence(name);
    REQUIRE(sequence != nullptr);
    CHECK(sequence->version == 3);
    REQUIRE(sequence->events.empty() == false);

    for (const auto &event : sequence->events) {
      const bool validTierPolicy =
          (event.tierPolicy == vfx::TierPolicy::Strict) ||
          (event.tierPolicy == vfx::TierPolicy::Degrade) ||
          (event.tierPolicy == vfx::TierPolicy::Skip);
      CHECK(validTierPolicy);
    }

    const auto report =
        vfx::VFXBudgetEstimator::Analyze(*sequence, kTemplateBudgetThreshold);
    CHECK(report.overBudget == false);
  }

  manager.Shutdown();
}
