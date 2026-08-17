#include "doctest.h"

#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/GPUTexturePool.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

namespace {

std::filesystem::path MakeTempSettingsPath(const std::string &name) {
  const auto dir = std::filesystem::path("bin") / "tmp_release_gate";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir / name;
}

void WriteJsonFile(const std::filesystem::path &path,
                   const nlohmann::json &value) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  REQUIRE(out.is_open());
  out << value.dump(2);
}

nlohmann::json ReadJsonFile(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.is_open());
  nlohmann::json value = nlohmann::json::object();
  in >> value;
  return value;
}

} // namespace

TEST_CASE("[Integration] ReleaseGate - Runtime render.v3.enabled toggle path") {
  using namespace NoMoreDay;

  const auto settingsPath = MakeTempSettingsPath("runtime_toggle_v3.json");
  WriteJsonFile(settingsPath,
                {{"renderQualityTier", "High"},
                 {"render", {{"v3", {{"enabled", false}}}}},
                 {"render.v3.enabled", false}});

  auto &manager = render::core::QualityTierManager::Get();
  manager.Initialize(settingsPath.string(), true);
  CHECK(manager.GetConfig().v3Enabled == false);

  int callbackCount = 0;
  bool callbackState = false;
  manager.SetV3ToggleCallback([&callbackCount, &callbackState](bool enabled) {
    ++callbackCount;
    callbackState = enabled;
  });

  CHECK(manager.SetV3Enabled(true, settingsPath.string()));
  CHECK(manager.GetConfig().v3Enabled == true);
  CHECK(callbackState == true);

  CHECK(manager.SetV3Enabled(false, settingsPath.string()));
  CHECK(manager.GetConfig().v3Enabled == false);
  CHECK(callbackState == false);
  CHECK_FALSE(manager.SetV3Enabled(false, settingsPath.string()));
  CHECK(callbackCount >= 2);

  const nlohmann::json persisted = ReadJsonFile(settingsPath);
  REQUIRE(persisted.contains("render.v3.enabled"));
  CHECK(persisted["render.v3.enabled"].is_boolean());
}

TEST_CASE("[Integration] ReleaseGate - Framebuffer tracked bytes stable under resize stress") {
  using namespace NoMoreDay;

  if (!utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::Initialize();
  }

  render::resources::FramebufferManager::ResetTrackedBytesForTesting();
  const uint64_t startBytes = render::resources::FramebufferManager::GetTrackedBytes();
  uint64_t peakBytes = startBytes;

  constexpr uint32_t kRgba16f = 0x881A;
  for (int i = 0; i < 240; ++i) {
    const int width = 640 + ((i % 4) * 320);
    const int height = 360 + ((i % 4) * 180);
    auto handle =
        render::resources::FramebufferManager::Create(width, height, kRgba16f, true);
    REQUIRE(handle.IsValid());
    peakBytes = std::max(peakBytes,
                         render::resources::FramebufferManager::GetTrackedBytes());

    const int resizedWidth = width + 64;
    const int resizedHeight = height + 32;
    render::resources::FramebufferManager::Resize(handle, resizedWidth,
                                                  resizedHeight);
    REQUIRE(handle.IsValid());
    peakBytes = std::max(peakBytes,
                         render::resources::FramebufferManager::GetTrackedBytes());

    render::resources::FramebufferManager::Destroy(handle);
  }

  // B3 (P2 AD-8): retire is now gated on the fence ACTUALLY signaling (a
  // timeout keeps the entry pending instead of releasing it), so the drain
  // loop must allow the real GL fences time to complete. 64 frames is a
  // generous polling budget (~1s @ 60fps) with zero GPU work in between;
  // the previous 4 frames only sufficed under the old timeout-as-ready bug.
  render::resources::GPUTexturePool::Get().AdvanceFrameForTesting(64);

  const uint64_t endBytes = render::resources::FramebufferManager::GetTrackedBytes();
  const double delta =
      static_cast<double>(endBytes) - static_cast<double>(startBytes);

  CHECK(peakBytes > startBytes);
  CHECK(endBytes == startBytes);
  std::cout << "RELEASE_GATE_METRIC vram_proxy_delta_bytes=" << delta << "\n";
}
