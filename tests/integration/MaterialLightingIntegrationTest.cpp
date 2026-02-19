#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/core/QualityTierManager.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace {

std::string ReadText(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return {};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void WriteText(const std::filesystem::path &path, const std::string &text) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out << text;
}

} // namespace

TEST_CASE("[Integration] Material Lighting - Cross-tier particle shader ABI path") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  const std::string materialAbi = ReadText("assets/shaders/generated/material_abi.glslinc");
  REQUIRE(!materialAbi.empty());
  CHECK(materialAbi.find("vec4 pbrParams;") != std::string::npos);
  CHECK(materialAbi.find("vec4 fresnelControl;") != std::string::npos);

  auto &qm = render::core::QualityTierManager::Get();
  qm.Initialize("settings.json");
  const auto originalTier = qm.GetTier();

  constexpr render::core::QualityTier kTiers[4] = {
      render::core::QualityTier::Low, render::core::QualityTier::Medium,
      render::core::QualityTier::High, render::core::QualityTier::Ultra};

  auto &particleSystem = systems::GPUParticleSystem::Get();
  for (const auto tier : kTiers) {
    qm.ForceTier(tier);
    particleSystem.Shutdown();
    particleSystem.Init(4096);
    CHECK(particleSystem.IsInitialized());

    Camera2D camera = {};
    camera.zoom = 1.0f;
    camera.target = {0.0f, 0.0f};
    camera.offset = {0.0f, 0.0f};
    CHECK_NOTHROW(particleSystem.Render(camera));
  }

  particleSystem.Shutdown();
  qm.ForceTier(originalTier);
}

TEST_CASE("[Integration] Material Lighting - Schema v2 hot reload updates runtime data") {
  using namespace NoMoreDay;

  auto &qm = render::core::QualityTierManager::Get();
  qm.Initialize("settings.json");
  qm.ForceTier(render::core::QualityTier::Ultra);
  auto &cfg = const_cast<render::core::RenderConfig &>(qm.GetConfig());
  cfg.hotReloadEnabled = true;

  auto &manager = render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const auto path =
      std::filesystem::path("bin") / "tmp_material_hot_reload_schema_v2.json";
  WriteText(path, R"json(
{
  "material_schema_version": 3,
  "materials": [
    {
      "name": "HotReloadMaterial",
      "baseColor": [1.0, 1.0, 1.0, 1.0],
      "emissive": [0.0, 0.0, 0.0],
      "emissiveIntensity": 0.0,
      "distortion": 0.0,
      "blendMode": "Alpha",
      "shader": "Default",
      "textureSlots": [-1, -1, -1, -1],
      "normalMapSlot": -1,
      "roughness": 0.65,
      "metallic": 0.1,
      "specular": 0.15,
      "ao": 1.0,
      "heightBias": 0.0,
      "detailNormalScale": 1.0,
      "fresnelControl": [0.04, 0.3, 0.1, 0.0],
      "uvParams": [1.0, 1.0, 0.0, 0.0]
    }
  ]
}
)json");

  REQUIRE(manager.LoadFromJson(path.string()) == 1);
  int materialId = manager.GetMaterialId("HotReloadMaterial");
  REQUIRE(materialId >= render::MaterialManager::PRESET_RESERVE);
  CHECK(manager.GetMaterial(materialId).roughness == doctest::Approx(0.65f));

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  WriteText(path, R"json(
{
  "material_schema_version": 3,
  "materials": [
    {
      "name": "HotReloadMaterial",
      "baseColor": [1.0, 1.0, 1.0, 1.0],
      "emissive": [0.0, 0.0, 0.0],
      "emissiveIntensity": 0.0,
      "distortion": 0.0,
      "blendMode": "Alpha",
      "shader": "Default",
      "textureSlots": [-1, -1, -1, -1],
      "normalMapSlot": -1,
      "roughness": 0.25,
      "metallic": 0.25,
      "specular": 0.35,
      "ao": 0.9,
      "heightBias": 0.02,
      "detailNormalScale": 1.1,
      "fresnelControl": [0.06, 0.24, 0.06, 0.0],
      "uvParams": [1.0, 1.0, 0.0, 0.0]
    }
  ]
}
)json");

  manager.TryHotReload();
  materialId = manager.GetMaterialId("HotReloadMaterial");
  REQUIRE(materialId >= render::MaterialManager::PRESET_RESERVE);
  const auto &updated = manager.GetMaterial(materialId);
  CHECK(updated.roughness == doctest::Approx(0.25f));
  CHECK(updated.specular == doctest::Approx(0.35f));
  CHECK(updated.ao == doctest::Approx(0.9f));
  CHECK(updated.heightBias == doctest::Approx(0.02f));

  manager.Shutdown();
  std::error_code ec;
  std::filesystem::remove(path, ec);
}
