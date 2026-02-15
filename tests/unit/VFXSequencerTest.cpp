#include "doctest.h"

#include "engine/render/MaterialManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "engine/vfx/VFXSequencerSystem.hpp"
#include "game/components/Common.hpp"

#include <entt/entt.hpp>

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
  CHECK(vfx::VFXSequencerSystem::GetActiveMaterialSwapCountForTesting() == 1);

  vfx::VFXSequencerSystem::Update(registry, 0.10f);
  CHECK(vfx::VFXSequencerSystem::GetActiveMaterialSwapCountForTesting() == 0);

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
  CHECK(vfx::VFXSequencerSystem::GetActiveMaterialSwapCountForTesting() == 0);

  manager.Shutdown();
  CleanupDir(dir);
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
