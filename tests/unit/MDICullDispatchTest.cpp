#include "doctest.h"
#include "engine/render/MDIRenderer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string ReadFileText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return {};
  }
  std::stringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

struct TestEntity {
  float posX;
  float posY;
  float radius;
  bool isVisible(float minX, float maxX, float minY, float maxY) const {
    return (posX + radius >= minX && posX - radius <= maxX &&
            posY + radius >= minY && posY - radius <= maxY);
  }
};

} // namespace

TEST_CASE("[Unit] MDI Cull - Dispatch Group Calculation strictly (N + 255) / 256") {
  using NoMoreDay::render::MDIRenderer;

  // Verify compile-time constant
  static_assert(MDIRenderer::kCullLocalSize == 256,
                "kCullLocalSize must be 256 to match cull.compute layout(local_size_x = 256)");
  CHECK(MDIRenderer::kCullLocalSize == 256);

  // Spot checks for specific milestone item counts (0, 1, 64, 255, 256, 257, 1000)
  struct TestCase {
    uint32_t items;
    uint32_t expectedGroups;
  };

  const std::array<TestCase, 14> testCases = {{
      {0, 0},
      {1, 1},
      {64, 1},
      {128, 1},
      {255, 1},
      {256, 1},
      {257, 2},
      {511, 2},
      {512, 2},
      {513, 3},
      {1000, 4},
      {1024, 4},
      {1025, 5},
      {65536, 256},
  }};

  for (const auto &tc : testCases) {
    const uint32_t formulaResult = (tc.items + 255) / 256;
    const uint32_t helperResult = MDIRenderer::CalculateCullDispatchGroups(tc.items);

    CHECK(formulaResult == tc.expectedGroups);
    CHECK(helperResult == tc.expectedGroups);
  }

  // Comprehensive range test [0, 4096]
  for (uint32_t n = 0; n <= 4096; ++n) {
    const uint32_t expected = (n + 255) / 256;
    const uint32_t actual = MDIRenderer::CalculateCullDispatchGroups(n);
    CHECK(actual == expected);

    if (n > 0) {
      CHECK(actual * 256 >= n);
      CHECK((actual - 1) * 256 < n);
    } else {
      CHECK(actual == 0);
    }
  }
}

TEST_CASE("[Unit] MDI Cull - Result set parity between reference culling and simulated 256 workgroups") {
  using NoMoreDay::render::MDIRenderer;

  const std::vector<uint32_t> testEntityCounts = {0, 1, 64, 255, 256, 257, 511, 512, 513, 1000, 2048};
  constexpr float kFrustumMinX = 0.0f;
  constexpr float kFrustumMaxX = 1000.0f;
  constexpr float kFrustumMinY = 0.0f;
  constexpr float kFrustumMaxY = 1000.0f;

  for (uint32_t totalCount : testEntityCounts) {
    std::vector<TestEntity> entities(totalCount);
    for (uint32_t i = 0; i < totalCount; ++i) {
      // Alternate positions inside and outside the frustum
      entities[i].posX = (i % 2 == 0) ? 500.0f : -500.0f;
      entities[i].posY = 500.0f;
      entities[i].radius = 10.0f;
    }

    // 1. Reference serial CPU culling
    std::vector<uint32_t> referenceVisibleIndices;
    for (uint32_t i = 0; i < totalCount; ++i) {
      if (entities[i].isVisible(kFrustumMinX, kFrustumMaxX, kFrustumMinY, kFrustumMaxY)) {
        referenceVisibleIndices.push_back(i);
      }
    }

    // 2. Simulated GPU 256-wide workgroup culling (exact logic of cull.compute)
    const uint32_t groups = MDIRenderer::CalculateCullDispatchGroups(totalCount);
    std::vector<uint32_t> simulatedGpuVisibleIndices;
    uint32_t executedThreadCount = 0;
    uint32_t outOfBoundsGuardTriggerCount = 0;

    for (uint32_t g = 0; g < groups; ++g) {
      for (uint32_t localId = 0; localId < 256; ++localId) {
        const uint32_t globalIdx = g * 256 + localId;
        executedThreadCount++;

        // cull.compute guard: if (globalIdx >= totalCount) return;
        if (globalIdx >= totalCount) {
          outOfBoundsGuardTriggerCount++;
          continue;
        }

        if (entities[globalIdx].isVisible(kFrustumMinX, kFrustumMaxX, kFrustumMinY, kFrustumMaxY)) {
          simulatedGpuVisibleIndices.push_back(globalIdx);
        }
      }
    }

    // Assert total workgroup thread coverage encompasses the exact item range
    CHECK(executedThreadCount == groups * 256);
    if (totalCount > 0) {
      CHECK(outOfBoundsGuardTriggerCount == (groups * 256 - totalCount));
    }

    // Assert resulting visible index sets are identical
    REQUIRE(simulatedGpuVisibleIndices.size() == referenceVisibleIndices.size());
    CHECK(simulatedGpuVisibleIndices == referenceVisibleIndices);

    // Verify last tail item [totalCount - 1] is correctly processed and not omitted
    if (totalCount > 0) {
      const uint32_t lastIdx = totalCount - 1;
      const bool expectedLastVisible = (lastIdx % 2 == 0);
      const bool actualLastVisible =
          std::find(simulatedGpuVisibleIndices.begin(), simulatedGpuVisibleIndices.end(), lastIdx) !=
          simulatedGpuVisibleIndices.end();
      CHECK(actualLastVisible == expectedLastVisible);
    }
  }
}

TEST_CASE("[Unit] MDI Cull - Shader local_size_x contract matches C++ constant") {
  // 1. Verify cull.compute declares local_size_x = 256
  const std::string cullShaderSource = ReadFileText("assets/shaders/cull.compute");
  REQUIRE_FALSE(cullShaderSource.empty());
  CHECK(cullShaderSource.find("layout(local_size_x = 256)") != std::string::npos);

  // 2. Verify scatter_stats.compute declares local_size_x = 64
  const std::string scatterShaderSource = ReadFileText("assets/shaders/scatter_stats.compute");
  REQUIRE_FALSE(scatterShaderSource.empty());
  CHECK(scatterShaderSource.find("layout(local_size_x = 64)") != std::string::npos);
}

TEST_CASE("[Unit] MDI Cull - Source code dispatch contract alignment") {
  const std::string mdiSource = ReadFileText("src/engine/render/MDIRenderer.cpp");
  REQUIRE_FALSE(mdiSource.empty());

  // Cull dispatch must use 256 workgroup size
  CHECK(mdiSource.find("constexpr uint32_t kCullLocalSize = 256;") != std::string::npos);
  CHECK(mdiSource.find("(dispatchCount + kCullLocalSize - 1) / kCullLocalSize") != std::string::npos);
  CHECK(mdiSource.find("DispatchComputeNoBarrier(groups, 1, 1)") != std::string::npos);

  // StatsScatter dispatch must remain 64
  CHECK(mdiSource.find("DispatchCompute((count + 63) / 64, 1, 1)") != std::string::npos);
}
