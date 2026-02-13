#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/MaterialDefs.hpp"
#include "engine/render/MaterialManager.hpp"

#include <cstddef>
#include <cstdint>

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
