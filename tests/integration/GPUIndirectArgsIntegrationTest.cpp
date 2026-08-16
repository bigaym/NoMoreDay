#include "doctest.h"

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPULootSystem.hpp"
#include "engine/render/GPUTextSystem.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <vector>

namespace {

struct DrawArraysIndirectCommand {
  uint32_t count = 0;
  uint32_t instanceCount = 0;
  uint32_t first = 0;
  uint32_t baseInstance = 0;
};

bool EnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "GPUIndirectArgsIntegrationTest Window");
  if (!IsWindowReady()) {
    return false;
  }
  NoMoreDay::utils::GPUUtils::Initialize();
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return false;
  }
  return NoMoreDay::utils::GPUUtils::CheckSupport().computeShaderSupported;
}

} // namespace

namespace NoMoreDay::tests {

TEST_CASE("[Integration] GPUTextSystem - Zero Synchronous Readback on Render Path & GPU Indirect Command") {
  if (!EnsureGpuContext()) {
    WARN("OpenGL 4.3 compute shader context unavailable; skipping GL execution test");
    return;
  }

  ResourceManager resources;
  auto &textSystem = render::GPUTextSystem::Get();
  textSystem.Init(resources, 1024, 4096);
  textSystem.SetReadbackEnabledForTesting(true);
  REQUIRE(textSystem.IsInitialized());

  // Setup minimal 1x1 font atlas texture
  Image dummyImage = GenImageColor(16, 16, WHITE);
  Texture2D atlasTex = LoadTextureFromImage(dummyImage);
  UnloadImage(dummyImage);
  textSystem.SetAtlasTexture(atlasTex, true);

  // Setup glyph metrics and string table
  components::GPUGlyphMetrics gm = {};
  gm.advance = 10.0f;
  gm.sizeX = 8.0f;
  gm.sizeY = 12.0f;
  gm.uvMaxX = 1.0f;
  gm.uvMaxY = 1.0f;
  textSystem.UploadGlyphMetrics({gm});

  render::GPUTextStringMeta meta = {};
  meta.glyphOffset = 0;
  meta.glyphCount = 3;
  meta.animStyle = 0;
  textSystem.UploadStringTable({0u, 0u, 0u}, {meta});

  // Test 1: Zero commands layout and render
  textSystem.BeginFrame();
  core::ComputeBuffer::ResetTestReadCount();
  REQUIRE_EQ(core::ComputeBuffer::GetTestReadCount(), 0u);

  textSystem.DispatchLayout(0.0f, 1.0f);
  Matrix viewProj = MatrixIdentity();
  textSystem.Render(viewProj);

  // Assert ZERO synchronous readbacks on the main render path
  CHECK_EQ(core::ComputeBuffer::GetTestReadCount(), 0u);

  // Read back indirect command buffer to verify GPU generated zero command
  DrawArraysIndirectCommand cmd = {};
  rlReadShaderBuffer(textSystem.GetIndirectBuffer().GetId(), &cmd, sizeof(cmd), 0);
  CHECK_EQ(cmd.count, 6u);
  CHECK_EQ(cmd.instanceCount, 0u);
  CHECK_EQ(cmd.first, 0u);
  CHECK_EQ(cmd.baseInstance, 0u);

  // Test 2: Active commands layout and render
  textSystem.BeginFrame();
  components::GPUTextCommand textCmd = {};
  textCmd.worldPosX = 100.0f;
  textCmd.worldPosY = 200.0f;
  textCmd.stringId = 0;
  textCmd.colorAndFlags = 0xFFFFFFFFu;
  CHECK(textSystem.EnqueueCommand(textCmd));

  core::ComputeBuffer::ResetTestReadCount();
  REQUIRE_EQ(core::ComputeBuffer::GetTestReadCount(), 0u);

  textSystem.DispatchLayout(0.0f, 1.0f);
  textSystem.Render(viewProj);

  // Assert ZERO synchronous readbacks on the main render path with active text
  CHECK_EQ(core::ComputeBuffer::GetTestReadCount(), 0u);

  // Read back indirect command buffer to verify GPU generated valid draw arguments
  rlReadShaderBuffer(textSystem.GetIndirectBuffer().GetId(), &cmd, sizeof(cmd), 0);
  CHECK_EQ(cmd.count, 6u);
  CHECK_EQ(cmd.instanceCount, 3u); // 1 command x 3 glyphs = 3 quads
  CHECK_EQ(cmd.first, 0u);
  CHECK_EQ(cmd.baseInstance, 0u);

  textSystem.Shutdown();
  CHECK_FALSE(textSystem.IsInitialized());
}

TEST_CASE("[Integration] GPULootSystem - Zero Synchronous Readback on Render Path & GPU Indirect Command") {
  if (!EnsureGpuContext()) {
    WARN("OpenGL 4.3 compute shader context unavailable; skipping GL execution test");
    return;
  }

  auto &lootSystem = render::GPULootSystem::Get();
  lootSystem.Init(1024);
  lootSystem.SetReadbackEnabledForTesting(true);
  REQUIRE(lootSystem.IsInitialized());

  Camera2D camera = {};
  camera.target = {0.0f, 0.0f};
  camera.offset = {400.0f, 300.0f};
  camera.zoom = 1.0f;
  Matrix viewProj = MatrixIdentity();

  // Test 1: Zero instances dispatch and render
  lootSystem.UploadInstances({});
  core::ComputeBuffer::ResetTestReadCount();
  REQUIRE_EQ(core::ComputeBuffer::GetTestReadCount(), 0u);

  lootSystem.Dispatch(camera, 800, 600, true);
  lootSystem.Render(viewProj, false);

  // Assert ZERO synchronous readbacks on render path
  CHECK_EQ(core::ComputeBuffer::GetTestReadCount(), 0u);

  DrawArraysIndirectCommand cmd = {};
  rlReadShaderBuffer(lootSystem.GetIndirectBuffer().GetId(), &cmd, sizeof(cmd), 0);
  CHECK_EQ(cmd.count, 6u);
  CHECK_EQ(cmd.instanceCount, 0u);
  CHECK_EQ(cmd.first, 0u);
  CHECK_EQ(cmd.baseInstance, 0u);

  // Test 2: Active instances inside frustum with force-directed layout enabled
  std::vector<components::GPULootInstance> lootList(8);
  for (size_t i = 0; i < lootList.size(); ++i) {
    lootList[i].worldPosX = static_cast<float>(i * 10.0f);
    lootList[i].worldPosY = static_cast<float>(i * 10.0f);
    lootList[i].labelOffsetX = 0.0f;
    lootList[i].labelOffsetY = 0.0f;
    lootList[i].itemId = static_cast<uint32_t>(100 + i);
    lootList[i].rarityColor = 0xFF00FFFF;
    lootList[i].glowIntensity = 1.0f;
    lootList[i].flags = 0u;
  }

  lootSystem.UploadInstances(lootList);
  CHECK_EQ(lootSystem.GetSyncedInstanceCount(), 8u);

  core::ComputeBuffer::ResetTestReadCount();
  REQUIRE_EQ(core::ComputeBuffer::GetTestReadCount(), 0u);

  lootSystem.Dispatch(camera, 800, 600, true);
  lootSystem.Render(viewProj, true);

  // Assert ZERO synchronous readbacks during full cull + indirect args + force directed + render pipeline
  CHECK_EQ(core::ComputeBuffer::GetTestReadCount(), 0u);

  rlReadShaderBuffer(lootSystem.GetIndirectBuffer().GetId(), &cmd, sizeof(cmd), 0);
  CHECK_EQ(cmd.count, 6u);
  CHECK_EQ(cmd.instanceCount, 8u); // all 8 are inside 800x600 centered frustum
  CHECK_EQ(cmd.first, 0u);
  CHECK_EQ(cmd.baseInstance, 0u);

  lootSystem.Shutdown();
  CHECK_FALSE(lootSystem.IsInitialized());
}

} // namespace NoMoreDay::tests
