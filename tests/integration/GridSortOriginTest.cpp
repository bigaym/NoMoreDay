#include "doctest.h"

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/resource/ResourceManager.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

bool EnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "GridSort Origin Test Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  return NoMoreDay::utils::GPUUtils::IsInitialized();
}

std::vector<GLenum> DrainGlErrors() {
  std::vector<GLenum> errors;
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    errors.push_back(err);
  }
  return errors;
}

// CPU reference implementation of the shader mapping contract:
// vec2 relativePos = entities[id].position - gridOrigin;
// int cx = int(relativePos.x / cellSize);
// int cy = int(relativePos.y / cellSize);
// if (cx < 0 || cx >= gridCols || cy < 0 || cy >= gridRows) return -1;
// uint cellIdx = cy * gridCols + cx;
int ComputeCellIndexContract(Vector2 position, Vector2 gridOrigin, float cellSize,
                             int gridCols, int gridRows, float radius) {
  if (radius <= 0.0f) {
    return -1;
  }
  Vector2 relativePos = {position.x - gridOrigin.x, position.y - gridOrigin.y};
  int cx = static_cast<int>(relativePos.x / cellSize);
  int cy = static_cast<int>(relativePos.y / cellSize);
  if (cx < 0 || cx >= gridCols || cy < 0 || cy >= gridRows) {
    return -1;
  }
  return cy * gridCols + cx;
}

} // namespace

TEST_CASE("[Unit] GridSort - Coordinate contract with non-zero origin") {
  constexpr float kCellSize = 32.0f;
  constexpr int kGridCols = 16;
  constexpr int kGridRows = 16;

  SUBCASE("Negative grid origin mapping") {
    const Vector2 origin = {-500.0f, -300.0f};

    // Exactly on origin -> cell (0, 0)
    CHECK(ComputeCellIndexContract({-500.0f, -300.0f}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == 0);

    // Near origin inside cell (0, 0)
    CHECK(ComputeCellIndexContract({-500.0f + 15.0f, -300.0f + 15.0f}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == 0);

    // Cell (1, 0)
    CHECK(ComputeCellIndexContract({-500.0f + 35.0f, -300.0f + 10.0f}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == 1);

    // Cell (0, 1) -> index 16
    CHECK(ComputeCellIndexContract({-500.0f + 10.0f, -300.0f + 35.0f}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == 16);

    // Cell (15, 15) -> index 255
    CHECK(ComputeCellIndexContract({-500.0f + 15.0f * kCellSize + 1.0f, -300.0f + 15.0f * kCellSize + 1.0f},
                                  origin, kCellSize, kGridCols, kGridRows, 1.0f) == 255);

    // Far negative relative to origin -> out-of-bounds (-1)
    CHECK(ComputeCellIndexContract({-600.0f, -300.0f}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == -1);
    CHECK(ComputeCellIndexContract({-500.0f, -400.0f}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == -1);

    // Exceeding grid width/height -> out-of-bounds (-1)
    CHECK(ComputeCellIndexContract({-500.0f + 16.0f * kCellSize, -300.0f}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == -1);
    CHECK(ComputeCellIndexContract({-500.0f, -300.0f + 16.0f * kCellSize}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == -1);

    // Inactive entity (radius <= 0) -> dropped (-1)
    CHECK(ComputeCellIndexContract({-500.0f + 10.0f, -300.0f + 10.0f}, origin, kCellSize, kGridCols, kGridRows, 0.0f) == -1);
    CHECK(ComputeCellIndexContract({-500.0f + 10.0f, -300.0f + 10.0f}, origin, kCellSize, kGridCols, kGridRows, -5.0f) == -1);
  }

  SUBCASE("Positive grid origin mapping") {
    const Vector2 origin = {1000.0f, 2000.0f};

    // Exactly on origin -> cell (0, 0)
    CHECK(ComputeCellIndexContract({1000.0f, 2000.0f}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == 0);

    // Cell (2, 3) -> index 3 * 16 + 2 = 50
    CHECK(ComputeCellIndexContract({1000.0f + 2.5f * kCellSize, 2000.0f + 3.5f * kCellSize},
                                  origin, kCellSize, kGridCols, kGridRows, 1.0f) == 50);

    // Less than origin -> out-of-bounds (-1)
    CHECK(ComputeCellIndexContract({900.0f, 2000.0f}, origin, kCellSize, kGridCols, kGridRows, 1.0f) == -1);
  }
}

TEST_CASE("[Integration] GridSort - Real GPU compute non-zero origin parity with grid_count") {
  if (!EnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping GridSort non-zero origin GPU test");
  }
  (void)DrainGlErrors();

  ResourceManager resources;
  Shader countShader = resources.loadComputeShader(
      entt::hashed_string{"grid_count_parity_test"}, "assets/shaders/grid_count.compute");
  Shader sortShader = resources.loadComputeShader(
      entt::hashed_string{"grid_sort_parity_test"}, "assets/shaders/grid_sort.compute");

  REQUIRE(countShader.id != 0u);
  REQUIRE(sortShader.id != 0u);

  constexpr int kGridCols = 16;
  constexpr int kGridRows = 16;
  constexpr int kNumCells = kGridCols * kGridRows; // 256 cells
  constexpr float kCellSize = 32.0f;
  const Vector2 kGridOrigin = {-500.0f, -300.0f};
  constexpr int kMaxEntities = 32;

  // Prepare test entities
  std::vector<NoMoreDay::components::GPUEntity> entities(kMaxEntities);
  for (auto &e : entities) {
    e.radius = 0.0f;
    e.position = {0.0f, 0.0f};
  }

  // Entity 0: in cell (0, 0), pos = {-500.0f + 5.0f, -300.0f + 5.0f}
  entities[0].position = {kGridOrigin.x + 5.0f, kGridOrigin.y + 5.0f};
  entities[0].radius = 4.0f;

  // Entity 1: in cell (0, 0), pos = {-500.0f + 25.0f, -300.0f + 15.0f}
  entities[1].position = {kGridOrigin.x + 25.0f, kGridOrigin.y + 15.0f};
  entities[1].radius = 4.0f;

  // Entity 2: in cell (1, 0), pos = {-500.0f + 40.0f, -300.0f + 10.0f} -> cellIdx = 1
  entities[2].position = {kGridOrigin.x + 40.0f, kGridOrigin.y + 10.0f};
  entities[2].radius = 4.0f;

  // Entity 3: in cell (0, 2), pos = {-500.0f + 10.0f, -300.0f + 70.0f} -> cellIdx = 2 * 16 + 0 = 32
  entities[3].position = {kGridOrigin.x + 10.0f, kGridOrigin.y + 70.0f};
  entities[3].radius = 4.0f;

  // Entity 4: in cell (5, 8), pos = {-500.0f + 5 * 32.0f + 10.0f, -300.0f + 8 * 32.0f + 10.0f} -> cellIdx = 8 * 16 + 5 = 133
  entities[4].position = {kGridOrigin.x + 5.0f * kCellSize + 10.0f, kGridOrigin.y + 8.0f * kCellSize + 10.0f};
  entities[4].radius = 4.0f;

  // Entity 5: in cell (15, 15), pos = {-500.0f + 15 * 32.0f + 16.0f, -300.0f + 15 * 32.0f + 16.0f} -> cellIdx = 255
  entities[5].position = {kGridOrigin.x + 15.0f * kCellSize + 16.0f, kGridOrigin.y + 15.0f * kCellSize + 16.0f};
  entities[5].radius = 4.0f;

  // Entity 6: Out-of-bounds (far negative X)
  entities[6].position = {kGridOrigin.x - 100.0f, kGridOrigin.y + 10.0f};
  entities[6].radius = 4.0f;

  // Entity 7: Out-of-bounds (far negative Y)
  entities[7].position = {kGridOrigin.x + 10.0f, kGridOrigin.y - 100.0f};
  entities[7].radius = 4.0f;

  // Entity 8: Out-of-bounds (exceeds grid width in X)
  entities[8].position = {kGridOrigin.x + kGridCols * kCellSize + 10.0f, kGridOrigin.y + 10.0f};
  entities[8].radius = 4.0f;

  // Entity 9: Out-of-bounds (exceeds grid height in Y)
  entities[9].position = {kGridOrigin.x + 10.0f, kGridOrigin.y + kGridRows * kCellSize + 10.0f};
  entities[9].radius = 4.0f;

  // Entity 10: In cell (0, 0) but inactive (radius == 0.0f)
  entities[10].position = {kGridOrigin.x + 10.0f, kGridOrigin.y + 10.0f};
  entities[10].radius = 0.0f;

  // Entity 11: In cell (1, 0) but inactive (radius < 0.0f)
  entities[11].position = {kGridOrigin.x + 40.0f, kGridOrigin.y + 10.0f};
  entities[11].radius = -2.0f;

  // Create SSBOs
  NoMoreDay::core::ComputeBuffer entityBuffer;
  entityBuffer.Create(entities.size() * sizeof(NoMoreDay::components::GPUEntity),
                      entities.data(), RL_DYNAMIC_DRAW);
  REQUIRE(entityBuffer.GetId() != 0u);

  NoMoreDay::core::ComputeBuffer cellCountBuffer;
  std::vector<uint32_t> initialCounts(kNumCells, 0u);
  cellCountBuffer.Create(kNumCells * sizeof(uint32_t), initialCounts.data(), RL_DYNAMIC_DRAW);
  REQUIRE(cellCountBuffer.GetId() != 0u);

  NoMoreDay::core::ComputeBuffer cellOffsetBuffer;
  cellOffsetBuffer.Create(kNumCells * sizeof(uint32_t), nullptr, RL_DYNAMIC_DRAW);
  REQUIRE(cellOffsetBuffer.GetId() != 0u);

  constexpr uint32_t kSentinel = 0xDEADBEEFu;
  NoMoreDay::core::ComputeBuffer entityIndicesBuffer;
  std::vector<uint32_t> initialIndices(kMaxEntities, kSentinel);
  entityIndicesBuffer.Create(kMaxEntities * sizeof(uint32_t), initialIndices.data(), RL_DYNAMIC_DRAW);
  REQUIRE(entityIndicesBuffer.GetId() != 0u);

  NoMoreDay::core::ComputeBuffer tempCountBuffer;
  tempCountBuffer.Create(kNumCells * sizeof(uint32_t), initialCounts.data(), RL_DYNAMIC_DRAW);
  REQUIRE(tempCountBuffer.GetId() != 0u);

  // 1. Dispatch grid_count
  rlEnableShader(countShader.id);
  int locCountMaxEntities = rlGetLocationUniform(countShader.id, "maxEntities");
  int locCountCellSize = rlGetLocationUniform(countShader.id, "cellSize");
  int locCountGridCols = rlGetLocationUniform(countShader.id, "gridCols");
  int locCountGridRows = rlGetLocationUniform(countShader.id, "gridRows");
  int locCountGridOrigin = rlGetLocationUniform(countShader.id, "gridOrigin");

  rlSetUniform(locCountMaxEntities, &kMaxEntities, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locCountCellSize, &kCellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(locCountGridCols, &kGridCols, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locCountGridRows, &kGridRows, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locCountGridOrigin, &kGridOrigin, RL_SHADER_UNIFORM_VEC2, 1);

  entityBuffer.BindBase(1);
  cellCountBuffer.BindBase(2);

  rlComputeShaderDispatch((kMaxEntities + 255) / 256, 1, 1);
  NoMoreDay::utils::GPUUtils::MemoryBarrier(NoMoreDay::RenderConstants::Barrier::SSBO);
  rlDisableShader();

  // Read back cell counts and verify
  std::vector<uint32_t> gpuCellCounts(kNumCells, 0u);
  cellCountBuffer.Read(gpuCellCounts.data(), kNumCells * sizeof(uint32_t));

  CHECK(gpuCellCounts[0] == 2u);   // Entities 0, 1
  CHECK(gpuCellCounts[1] == 1u);   // Entity 2
  CHECK(gpuCellCounts[32] == 1u);  // Entity 3
  CHECK(gpuCellCounts[133] == 1u); // Entity 4
  CHECK(gpuCellCounts[255] == 1u); // Entity 5

  uint32_t totalCounted = 0;
  for (uint32_t c : gpuCellCounts) {
    totalCounted += c;
  }
  CHECK(totalCounted == 6u);

  // 2. Compute prefix sum (offsets) and upload
  std::vector<uint32_t> cellOffsets(kNumCells, 0u);
  uint32_t runningSum = 0;
  for (int i = 0; i < kNumCells; ++i) {
    cellOffsets[i] = runningSum;
    runningSum += gpuCellCounts[i];
  }
  cellOffsetBuffer.Update(cellOffsets.data(), kNumCells * sizeof(uint32_t));

  // Reset tempCounts to 0
  std::vector<uint32_t> zeroCounts(kNumCells, 0u);
  tempCountBuffer.Update(zeroCounts.data(), kNumCells * sizeof(uint32_t));

  // 3. Dispatch grid_sort
  rlEnableShader(sortShader.id);
  int locSortMaxEntities = rlGetLocationUniform(sortShader.id, "maxEntities");
  int locSortCellSize = rlGetLocationUniform(sortShader.id, "cellSize");
  int locSortGridCols = rlGetLocationUniform(sortShader.id, "gridCols");
  int locSortGridRows = rlGetLocationUniform(sortShader.id, "gridRows");
  int locSortGridOrigin = rlGetLocationUniform(sortShader.id, "gridOrigin");

  rlSetUniform(locSortMaxEntities, &kMaxEntities, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locSortCellSize, &kCellSize, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(locSortGridCols, &kGridCols, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locSortGridRows, &kGridRows, RL_SHADER_UNIFORM_INT, 1);
  rlSetUniform(locSortGridOrigin, &kGridOrigin, RL_SHADER_UNIFORM_VEC2, 1);

  entityBuffer.BindBase(1);
  cellOffsetBuffer.BindBase(3);
  entityIndicesBuffer.BindBase(4);
  tempCountBuffer.BindBase(5);

  rlComputeShaderDispatch((kMaxEntities + 255) / 256, 1, 1);
  NoMoreDay::utils::GPUUtils::MemoryBarrier(NoMoreDay::RenderConstants::Barrier::SSBO);
  rlDisableShader();

  // Read back temp counts and entity indices
  std::vector<uint32_t> gpuTempCounts(kNumCells, 0u);
  tempCountBuffer.Read(gpuTempCounts.data(), kNumCells * sizeof(uint32_t));

  std::vector<uint32_t> gpuEntityIndices(kMaxEntities, 0u);
  entityIndicesBuffer.Read(gpuEntityIndices.data(), kMaxEntities * sizeof(uint32_t));

  // 4. Parity and bounds validation
  for (int i = 0; i < kNumCells; ++i) {
    CHECK_EQ(gpuTempCounts[i], gpuCellCounts[i]);
  }

  // Cell 0 slice (offset 0, count 2): contains {0, 1}
  std::vector<uint32_t> cell0Entities = {gpuEntityIndices[0], gpuEntityIndices[1]};
  std::sort(cell0Entities.begin(), cell0Entities.end());
  CHECK(cell0Entities[0] == 0u);
  CHECK(cell0Entities[1] == 1u);

  // Cell 1 slice (offset 2, count 1): contains {2}
  CHECK(gpuEntityIndices[2] == 2u);

  // Cell 32 slice (offset 3, count 1): contains {3}
  CHECK(gpuEntityIndices[3] == 3u);

  // Cell 133 slice (offset 4, count 1): contains {4}
  CHECK(gpuEntityIndices[4] == 4u);

  // Cell 255 slice (offset 5, count 1): contains {5}
  CHECK(gpuEntityIndices[5] == 5u);

  // Elements beyond the 6 valid sorted entities must remain untouched sentinel values
  for (int i = 6; i < kMaxEntities; ++i) {
    CHECK_EQ(gpuEntityIndices[i], kSentinel);
  }

  // No OpenGL errors
  const auto errors = DrainGlErrors();
  CHECK(errors.empty());
}
