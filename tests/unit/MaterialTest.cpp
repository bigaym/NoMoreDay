#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/MaterialDefs.hpp"
#include "engine/render/MaterialManager.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <filesystem>

using namespace NoMoreDay;

TEST_CASE("[Unit] Material - GPUMaterialData ABI Layout") {
  CHECK(sizeof(components::GPUMaterialData) == 64);
  CHECK(offsetof(components::GPUMaterialData, baseColorR) == 0);
  CHECK(offsetof(components::GPUMaterialData, baseColorA) == 12);
  CHECK(offsetof(components::GPUMaterialData, emissiveR) == 16);
  CHECK(offsetof(components::GPUMaterialData, emissiveIntensity) == 28);
  CHECK(offsetof(components::GPUMaterialData, distortion) == 32);
  CHECK(offsetof(components::GPUMaterialData, blendMode) == 36);
  CHECK(offsetof(components::GPUMaterialData, shaderVariant) == 40);
  CHECK(offsetof(components::GPUMaterialData, flags) == 44);
  CHECK(offsetof(components::GPUMaterialData, textureSlot0) == 48);
  CHECK(offsetof(components::GPUMaterialData, textureSlot3) == 60);
}

TEST_CASE("[Unit] Material - Preset Registration") {
  auto &manager = render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();

  CHECK(manager.GetMaterialCount() == 8);
  CHECK(manager.GetMaterialId("Default") == 0);
  CHECK(manager.GetMaterialId("InkSplash") == 1);
  CHECK(manager.GetMaterialId("FireGlow") == 2);
  CHECK(manager.GetMaterialId("DistortionShockwave") == 7);

  const auto &fire = manager.GetMaterial(manager.GetMaterialId("FireGlow"));
  CHECK(fire.blendMode == render::BlendMode::Additive);
  CHECK(fire.shader == render::ShaderVariant::Fire);

  manager.Shutdown();
}

TEST_CASE("[Unit] Material - Json Loading") {
  auto &manager = render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson("assets/data/materials_vfx.json");
  CHECK(loaded >= 5);
  CHECK(manager.GetMaterialCount() >= 13);

  const int fireId = manager.GetMaterialId("FireExplosion");
  REQUIRE(fireId >= render::MaterialManager::PRESET_RESERVE);
  const auto &fire = manager.GetMaterial(fireId);
  CHECK(fire.shader == render::ShaderVariant::Fire);
  CHECK(fire.blendMode == render::BlendMode::Additive);
  CHECK(fire.emissiveIntensity == doctest::Approx(3.0f));

  manager.Shutdown();
}

TEST_CASE("[Unit] Material - Flags Pack Unpack") {
  uint32_t flags = 0xABu;
  components::GPUFlags::PackMaterialId(flags, 1234);

  CHECK(components::GPUFlags::UnpackMaterialId(flags) == 1234);
  CHECK((flags & 0xFFu) == 0xABu);
}

TEST_CASE("[Unit] Material - Invalid Json Does Not Override Existing State") {
  auto &manager = render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson("assets/data/materials_vfx.json");
  REQUIRE(loaded >= 5);

  const int fireId = manager.GetMaterialId("FireExplosion");
  REQUIRE(fireId >= render::MaterialManager::PRESET_RESERVE);
  const auto before = manager.GetMaterial(fireId);
  const int countBefore = manager.GetMaterialCount();

  const std::filesystem::path invalidPath =
      std::filesystem::path("bin") / "tmp_materials_invalid.json";
  {
    std::ofstream file(invalidPath, std::ios::binary);
    REQUIRE(file.is_open());
    file << "{ \"material_schema_version\": 99, \"materials\": [] }";
  }

  const int invalidLoaded = manager.LoadFromJson(invalidPath.string());
  CHECK(invalidLoaded == 0);
  CHECK(manager.GetMaterialCount() == countBefore);

  const int fireIdAfter = manager.GetMaterialId("FireExplosion");
  CHECK(fireIdAfter == fireId);
  const auto after = manager.GetMaterial(fireIdAfter);
  CHECK(after.baseColorR == doctest::Approx(before.baseColorR));
  CHECK(after.baseColorG == doctest::Approx(before.baseColorG));
  CHECK(after.baseColorB == doctest::Approx(before.baseColorB));
  CHECK(after.baseColorA == doctest::Approx(before.baseColorA));
  CHECK(after.emissiveIntensity == doctest::Approx(before.emissiveIntensity));
  CHECK(after.shader == before.shader);
  CHECK(after.blendMode == before.blendMode);

  std::error_code ec;
  std::filesystem::remove(invalidPath, ec);
  manager.Shutdown();
}

TEST_CASE("[Unit] Material - Missing Schema Version Is Rejected") {
  auto &manager = render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson("assets/data/materials_vfx.json");
  REQUIRE(loaded >= 5);
  const int countBefore = manager.GetMaterialCount();

  const std::filesystem::path invalidPath =
      std::filesystem::path("bin") / "tmp_materials_missing_schema.json";
  {
    std::ofstream file(invalidPath, std::ios::binary);
    REQUIRE(file.is_open());
    file << "{ \"materials\": [] }";
  }

  const int invalidLoaded = manager.LoadFromJson(invalidPath.string());
  CHECK(invalidLoaded == 0);
  CHECK(manager.GetMaterialCount() == countBefore);

  std::error_code ec;
  std::filesystem::remove(invalidPath, ec);
  manager.Shutdown();
}
