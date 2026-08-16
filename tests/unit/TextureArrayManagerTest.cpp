#include "doctest.h"

#include "engine/render/resource/TextureArrayManager.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

std::filesystem::path MakeTexturePath(const char *name) {
  const std::filesystem::path dir = std::filesystem::path("bin") / "tmp_texture_arrays";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / name;
}

void WriteTinyBmp(const std::filesystem::path &path, uint8_t r, uint8_t g,
                  uint8_t b) {
  const uint32_t fileSize = 54u + 4u;
  const uint32_t dataOffset = 54u;
  const uint32_t width = 1u;
  const uint32_t height = 1u;
  const uint16_t bitsPerPixel = 24u;
  const uint32_t imageSize = 4u;

  std::vector<uint8_t> header(54u, 0u);
  header[0] = 'B';
  header[1] = 'M';
  header[2] = static_cast<uint8_t>(fileSize & 0xFFu);
  header[3] = static_cast<uint8_t>((fileSize >> 8u) & 0xFFu);
  header[4] = static_cast<uint8_t>((fileSize >> 16u) & 0xFFu);
  header[5] = static_cast<uint8_t>((fileSize >> 24u) & 0xFFu);
  header[10] = static_cast<uint8_t>(dataOffset);
  header[14] = 40u;
  header[18] = static_cast<uint8_t>(width);
  header[22] = static_cast<uint8_t>(height);
  header[26] = 1u;
  header[28] = static_cast<uint8_t>(bitsPerPixel);
  header[34] = static_cast<uint8_t>(imageSize);

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out.write(reinterpret_cast<const char *>(header.data()),
            static_cast<std::streamsize>(header.size()));

  const uint8_t pixel[4] = {b, g, r, 0u};
  out.write(reinterpret_cast<const char *>(pixel), sizeof(pixel));
}

void Cleanup(const std::filesystem::path &path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

} // namespace

TEST_CASE("[Unit] TextureArrayManager - Defaults and Resolve Fallback") {
  using NoMoreDay::render::TextureArrayManager;
  using NoMoreDay::render::TextureArraySemantic;

  auto &manager = TextureArrayManager::Get();
  manager.Shutdown();
  manager.Initialize(8, 16);

  CHECK(manager.IsInitialized());
  CHECK(manager.GetDefaultLayer(TextureArraySemantic::Normal) >= 0);
  CHECK(manager.GetDefaultLayer(TextureArraySemantic::Roughness) >= 0);

  const int normalDefault = manager.GetDefaultLayer(TextureArraySemantic::Normal);
  CHECK(manager.ResolveLayerOrDefault(TextureArraySemantic::Normal, -1) ==
        normalDefault);
  CHECK(manager.ResolveLayerOrDefault(TextureArraySemantic::Normal, 9999) ==
        normalDefault);

  manager.Shutdown();
}

TEST_CASE("[Unit] TextureArrayManager - Layer Allocate Release and Fallback") {
  using NoMoreDay::render::TextureArrayManager;
  using NoMoreDay::render::TextureArraySemantic;

  auto &manager = TextureArrayManager::Get();
  manager.Shutdown();
  manager.Initialize(16, 16);

  const auto texPath = MakeTexturePath("layer_alloc_release.bmp");
  WriteTinyBmp(texPath, 120, 180, 220);

  const int defaultLayer = manager.GetDefaultLayer(TextureArraySemantic::Normal);
  const int layer = manager.LoadLayer(TextureArraySemantic::Normal, texPath.string());
  REQUIRE(layer >= 0);
  CHECK(layer != defaultLayer);
  CHECK(manager.GetLayerCount(TextureArraySemantic::Normal) >= 2);

  manager.ReleaseLayer(TextureArraySemantic::Normal, layer);
  CHECK(manager.ResolveLayerOrDefault(TextureArraySemantic::Normal, layer) ==
        defaultLayer);

  Cleanup(texPath);
  manager.Shutdown();
}

TEST_CASE("[Unit] TextureArrayManager - HotReload Atomic Swap and Rollback") {
  using NoMoreDay::render::TextureArrayManager;
  using NoMoreDay::render::TextureArraySemantic;

  auto &manager = TextureArrayManager::Get();
  manager.Shutdown();
  manager.Initialize(16, 16);

  const auto texA = MakeTexturePath("hot_reload_a.bmp");
  const auto texB = MakeTexturePath("hot_reload_b.bmp");
  WriteTinyBmp(texA, 255, 0, 0);
  WriteTinyBmp(texB, 0, 255, 0);

  CHECK(manager.HotReloadLayers(TextureArraySemantic::Roughness,
                                {texA.string(), texB.string()}));
  const int countAfterReload =
      manager.GetLayerCount(TextureArraySemantic::Roughness);
  CHECK(countAfterReload >= 3);

  const auto missing = MakeTexturePath("hot_reload_missing.bmp");
  CHECK_FALSE(
      manager.HotReloadLayers(TextureArraySemantic::Roughness, {missing.string()}));
  CHECK(manager.GetLayerCount(TextureArraySemantic::Roughness) == countAfterReload);

  Cleanup(texA);
  Cleanup(texB);
  Cleanup(missing);
  manager.Shutdown();
}

TEST_CASE("[Unit] TextureArrayManager - Rebuild On Resize Keeps Valid Defaults") {
  using NoMoreDay::render::TextureArrayManager;
  using NoMoreDay::render::TextureArraySemantic;

  auto &manager = TextureArrayManager::Get();
  manager.Shutdown();
  manager.Initialize(16, 16);

  const auto texPath = MakeTexturePath("resize_rebuild_layer.bmp");
  WriteTinyBmp(texPath, 60, 80, 140);
  CHECK(manager.LoadLayer(TextureArraySemantic::Normal, texPath.string()) >= 0);
  CHECK(manager.LoadLayer(TextureArraySemantic::Roughness, texPath.string()) >= 0);

  CHECK(manager.RebuildForResize(1600, 900));
  CHECK(manager.GetDefaultLayer(TextureArraySemantic::Normal) >= 0);
  CHECK(manager.GetDefaultLayer(TextureArraySemantic::Roughness) >= 0);
  CHECK(manager.GetLayerCount(TextureArraySemantic::Normal) >= 1);
  CHECK(manager.GetLayerCount(TextureArraySemantic::Roughness) >= 1);

  Cleanup(texPath);
  manager.Shutdown();
}

TEST_CASE("[Unit] TextureArrayManager - Color Space Linear Metadata (T8.1)") {
  using NoMoreDay::render::TextureArrayManager;
  using NoMoreDay::render::TextureArraySemantic;

  auto &manager = TextureArrayManager::Get();
  manager.Shutdown();
  manager.Initialize(16, 16);

  // Semantic-level defaults:
  // Albedo holds authored sRGB color data -> needs linearizing (default linear = false)
  CHECK_FALSE(TextureArrayManager::GetDefaultLinearForSemantic(
      TextureArraySemantic::Albedo));
  CHECK_FALSE(manager.IsSemanticLinear(TextureArraySemantic::Albedo));

  // Normal, Mask, Detail hold mathematical/physical data -> already linear (default linear = true)
  CHECK(TextureArrayManager::GetDefaultLinearForSemantic(
      TextureArraySemantic::Normal));
  CHECK(manager.IsSemanticLinear(TextureArraySemantic::Normal));

  CHECK(TextureArrayManager::GetDefaultLinearForSemantic(
      TextureArraySemantic::Mask));
  CHECK(manager.IsSemanticLinear(TextureArraySemantic::Mask));

  CHECK(TextureArrayManager::GetDefaultLinearForSemantic(
      TextureArraySemantic::Detail));
  CHECK(manager.IsSemanticLinear(TextureArraySemantic::Detail));

  // Default layer linear state matches semantic default
  const int albedoDefault =
      manager.GetDefaultLayer(TextureArraySemantic::Albedo);
  CHECK_FALSE(manager.IsLayerLinear(TextureArraySemantic::Albedo, albedoDefault));

  const int normalDefault =
      manager.GetDefaultLayer(TextureArraySemantic::Normal);
  CHECK(manager.IsLayerLinear(TextureArraySemantic::Normal, normalDefault));

  // Loading custom layer with explicit linear flag override
  const auto texLinear = MakeTexturePath("linear_override.bmp");
  const auto texSrgb = MakeTexturePath("srgb_override.bmp");
  WriteTinyBmp(texLinear, 128, 128, 128);
  WriteTinyBmp(texSrgb, 200, 100, 50);

  const int layerLinear = manager.LoadLayer(
      TextureArraySemantic::Albedo, texLinear.string(), /*isLinear=*/true);
  REQUIRE(layerLinear >= 0);
  CHECK(manager.IsLayerLinear(TextureArraySemantic::Albedo, layerLinear));

  const int layerSrgb = manager.LoadLayer(
      TextureArraySemantic::Normal, texSrgb.string(), /*isLinear=*/false);
  REQUIRE(layerSrgb >= 0);
  CHECK_FALSE(manager.IsLayerLinear(TextureArraySemantic::Normal, layerSrgb));

  // Hot-reload with per-layer linear flags
  const auto reloadA = MakeTexturePath("reload_linear_a.bmp");
  const auto reloadB = MakeTexturePath("reload_linear_b.bmp");
  WriteTinyBmp(reloadA, 255, 0, 0);
  WriteTinyBmp(reloadB, 0, 255, 0);

  CHECK(manager.HotReloadLayers(TextureArraySemantic::Albedo,
                                {reloadA.string(), reloadB.string()},
                                {true, false}));
  // Layer lookup
  const int reloadALayer = manager.LoadLayer(TextureArraySemantic::Albedo,
                                             reloadA.string());
  const int reloadBLayer = manager.LoadLayer(TextureArraySemantic::Albedo,
                                             reloadB.string());
  CHECK(manager.IsLayerLinear(TextureArraySemantic::Albedo, reloadALayer) == true);
  CHECK(manager.IsLayerLinear(TextureArraySemantic::Albedo, reloadBLayer) == false);

  Cleanup(texLinear);
  Cleanup(texSrgb);
  Cleanup(reloadA);
  Cleanup(reloadB);
  manager.Shutdown();
}
