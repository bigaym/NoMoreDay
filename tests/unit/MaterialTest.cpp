#include "doctest.h"

#include "engine/render/GPUData.hpp"
#include "engine/render/MaterialDefs.hpp"
#include "engine/render/MaterialManager.hpp"

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>

using namespace NoMoreDay;

namespace {

std::filesystem::path MakeTempPath(const char *name) {
  return std::filesystem::path("bin") / name;
}

void WriteTextFile(const std::filesystem::path &path, const std::string &text) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  REQUIRE(file.is_open());
  file << text;
}

void CleanupPath(const std::filesystem::path &path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

} // namespace

TEST_CASE("[Unit] Material - GPUMaterialDataV2 ABI Layout") {
  CHECK(sizeof(components::GPUMaterialDataV2) == 128);
  CHECK(alignof(components::GPUMaterialDataV2) == 16);
  CHECK(offsetof(components::GPUMaterialDataV2, baseColor) == 0);
  CHECK(offsetof(components::GPUMaterialDataV2, emissiveAndIntensity) == 16);
  CHECK(offsetof(components::GPUMaterialDataV2, pbrLite) == 32);
  CHECK(offsetof(components::GPUMaterialDataV2, textureSlots) == 48);
  CHECK(offsetof(components::GPUMaterialDataV2, detailParams) == 64);
  CHECK(offsetof(components::GPUMaterialDataV2, reserved2) == 112);
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
  CHECK(fire.roughness == doctest::Approx(0.6f));
  CHECK(fire.specular == doctest::Approx(0.2f));
  CHECK(fire.ao == doctest::Approx(1.0f));

  manager.Shutdown();
}

TEST_CASE("[Unit] Material - Schema v1 Compatibility Loading") {
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
  CHECK(fire.normalMapSlot == -1);
  CHECK(fire.roughness == doctest::Approx(0.6f));
  CHECK(fire.specular == doctest::Approx(0.2f));
  CHECK(fire.ao == doctest::Approx(1.0f));
  CHECK(fire.detailNormalScale == doctest::Approx(1.0f));

  manager.Shutdown();
}

TEST_CASE("[Unit] Material - BRDF-lite same-light different-material divergence") {
  // Keep this formula aligned with particle/entity BRDF-lite shader path.
  const auto evaluateColor = [](const render::MaterialInstance &mat) {
    constexpr float lx = 0.35f;
    constexpr float ly = 0.45f;
    constexpr float lz = 0.82f;
    const float lLen = std::sqrt(lx * lx + ly * ly + lz * lz);
    const float lnx = lx / lLen;
    const float lny = ly / lLen;
    const float lnz = lz / lLen;

    constexpr float vx = 0.0f;
    constexpr float vy = 0.0f;
    constexpr float vz = 1.0f;
    const float hx = lnx + vx;
    const float hy = lny + vy;
    const float hz = lnz + vz;
    const float hLen = std::sqrt(hx * hx + hy * hy + hz * hz);
    const float hnx = hx / hLen;
    const float hny = hy / hLen;
    const float hnz = hz / hLen;

    constexpr float nx = 0.0f;
    constexpr float ny = 0.0f;
    constexpr float nz = 1.0f;
    const float diffuse = std::max(nx * lnx + ny * lny + nz * lnz, 0.0f);
    const float ndoth = std::max(nx * hnx + ny * hny + nz * hnz, 0.0f);
    const float roughness = std::clamp(mat.roughness, 0.0f, 1.0f);
    const float specularStrength = std::clamp(mat.specular, 0.0f, 1.0f);
    const float shininess = 64.0f + (4.0f - 64.0f) * roughness;
    const float specular = std::pow(ndoth, shininess) * specularStrength;
    const float brdf = std::clamp(diffuse + specular, 0.0f, 2.0f);
    const float ao = std::clamp(mat.ao, 0.0f, 1.0f);
    const float factor = (0.25f + 0.75f * brdf) * ao;

    return std::array<float, 3>{mat.baseColorR * factor, mat.baseColorG * factor,
                                mat.baseColorB * factor};
  };

  render::MaterialInstance glossy = render::MaterialPresets::Default();
  glossy.baseColorR = 0.95f;
  glossy.baseColorG = 0.25f;
  glossy.baseColorB = 0.20f;
  glossy.roughness = 0.10f;
  glossy.specular = 1.00f;
  glossy.ao = 1.00f;

  render::MaterialInstance matte = render::MaterialPresets::Default();
  matte.baseColorR = 0.25f;
  matte.baseColorG = 0.35f;
  matte.baseColorB = 0.95f;
  matte.roughness = 1.00f;
  matte.specular = 0.00f;
  matte.ao = 0.55f;

  const auto glossyColor = evaluateColor(glossy);
  const auto matteColor = evaluateColor(matte);
  const float diff = std::fabs(glossyColor[0] - matteColor[0]) +
                     std::fabs(glossyColor[1] - matteColor[1]) +
                     std::fabs(glossyColor[2] - matteColor[2]);

  CHECK(diff > 0.05f);
}

TEST_CASE("[Unit] Material - Schema v2 Strict Validation and Parsing") {
  auto &manager = render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const auto path = MakeTempPath("tmp_materials_schema_v2_ok.json");
  WriteTextFile(path, R"json(
{
  "material_schema_version": 2,
  "materials": [
    {
      "name": "Schema2Material",
      "baseColor": [0.3, 0.4, 0.5, 0.6],
      "emissive": [0.7, 0.8, 0.9],
      "emissiveIntensity": 1.1,
      "distortion": 0.2,
      "blendMode": "Additive",
      "shader": "Hologram",
      "textureSlots": [12, -1, 8, -1],
      "normalMapSlot": 7,
      "roughness": 0.45,
      "specular": 0.35,
      "ao": 0.92,
      "heightBias": 0.03,
      "detailNormalScale": 1.25
    }
  ]
}
)json");

  const int loaded = manager.LoadFromJson(path.string());
  CHECK(loaded == 1);

  const int id = manager.GetMaterialId("Schema2Material");
  REQUIRE(id >= render::MaterialManager::PRESET_RESERVE);
  const auto &mat = manager.GetMaterial(id);
  CHECK(mat.baseColorR == doctest::Approx(0.3f));
  CHECK(mat.baseColorA == doctest::Approx(0.6f));
  CHECK(mat.normalMapSlot == 7);
  CHECK(mat.textureSlots[0] == 12);
  CHECK(mat.textureSlots[2] == 8);
  CHECK(mat.roughness == doctest::Approx(0.45f));
  CHECK(mat.specular == doctest::Approx(0.35f));
  CHECK(mat.ao == doctest::Approx(0.92f));
  CHECK(mat.heightBias == doctest::Approx(0.03f));
  CHECK(mat.detailNormalScale == doctest::Approx(1.25f));

  CleanupPath(path);
  manager.Shutdown();
}

TEST_CASE("[Unit] Material - Flags Pack Unpack") {
  uint32_t flags = 0xABu;
  components::GPUFlags::PackMaterialId(flags, 1234);

  CHECK(components::GPUFlags::UnpackMaterialId(flags) == 1234);
  CHECK((flags & 0xFFu) == 0xABu);
}

TEST_CASE("[Unit] Material - Invalid Schema Does Not Override Existing State") {
  auto &manager = render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson("assets/data/materials_vfx.json");
  REQUIRE(loaded >= 5);

  const int fireId = manager.GetMaterialId("FireExplosion");
  REQUIRE(fireId >= render::MaterialManager::PRESET_RESERVE);
  const auto before = manager.GetMaterial(fireId);
  const int countBefore = manager.GetMaterialCount();

  const auto invalidPath = MakeTempPath("tmp_materials_invalid_schema.json");
  WriteTextFile(invalidPath,
                "{ \"material_schema_version\": 99, \"materials\": [] }");

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

  CleanupPath(invalidPath);
  manager.Shutdown();
}

TEST_CASE("[Unit] Material - Missing Schema Version Is Rejected") {
  auto &manager = render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson("assets/data/materials_vfx.json");
  REQUIRE(loaded >= 5);
  const int countBefore = manager.GetMaterialCount();

  const auto invalidPath = MakeTempPath("tmp_materials_missing_schema.json");
  WriteTextFile(invalidPath, "{ \"materials\": [] }");

  const int invalidLoaded = manager.LoadFromJson(invalidPath.string());
  CHECK(invalidLoaded == 0);
  CHECK(manager.GetMaterialCount() == countBefore);

  CleanupPath(invalidPath);
  manager.Shutdown();
}

TEST_CASE("[Unit] Material - Schema v2 Rejects Missing and Unknown Fields") {
  auto &manager = render::MaterialManager::Get();
  manager.Shutdown();
  manager.Initialize();

  const int loaded = manager.LoadFromJson("assets/data/materials_vfx.json");
  REQUIRE(loaded >= 5);
  const int countBefore = manager.GetMaterialCount();

  const auto missingFieldPath = MakeTempPath("tmp_materials_schema_v2_missing.json");
  WriteTextFile(missingFieldPath, R"json(
{
  "material_schema_version": 2,
  "materials": [
    {
      "name": "MissingFieldMaterial",
      "baseColor": [1, 1, 1, 1],
      "emissive": [0, 0, 0],
      "emissiveIntensity": 0,
      "distortion": 0,
      "blendMode": "Alpha",
      "shader": "Default",
      "textureSlots": [-1, -1, -1, -1],
      "normalMapSlot": -1,
      "roughness": 0.6,
      "specular": 0.2,
      "ao": 1.0,
      "heightBias": 0.0
    }
  ]
}
)json");

  CHECK(manager.LoadFromJson(missingFieldPath.string()) == 0);
  CHECK(manager.GetMaterialCount() == countBefore);

  const auto unknownFieldPath = MakeTempPath("tmp_materials_schema_v2_unknown.json");
  WriteTextFile(unknownFieldPath, R"json(
{
  "material_schema_version": 2,
  "materials": [
    {
      "name": "UnknownFieldMaterial",
      "baseColor": [1, 1, 1, 1],
      "emissive": [0, 0, 0],
      "emissiveIntensity": 0,
      "distortion": 0,
      "blendMode": "Alpha",
      "shader": "Default",
      "textureSlots": [-1, -1, -1, -1],
      "normalMapSlot": -1,
      "roughness": 0.6,
      "specular": 0.2,
      "ao": 1.0,
      "heightBias": 0.0,
      "detailNormalScale": 1.0,
      "unsupportedField": 123
    }
  ]
}
)json");

  CHECK(manager.LoadFromJson(unknownFieldPath.string()) == 0);
  CHECK(manager.GetMaterialCount() == countBefore);

  CleanupPath(missingFieldPath);
  CleanupPath(unknownFieldPath);
  manager.Shutdown();
}
