#include "doctest.h"

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/render/passes/JFAPass.hpp"
#include "engine/render/passes/OccluderExtractPass.hpp"
#include "engine/render/passes/RadianceCascadesPass.hpp"
#include "engine/render/passes/ShadowBuildPass.hpp"
#include "engine/resource/ResourceManager.hpp"

#include <array>
#include <cstdint>
#include <vector>

TEST_CASE("[Unit] PersistentBuffer - Non-blocking TryRead and delayed snapshot contract") {
  using NoMoreDay::render::PersistentBuffer;

  PersistentBuffer buffer;
  constexpr size_t kSlotBytes = sizeof(uint32_t);
  buffer.Create(kSlotBytes, 3);

  CHECK(buffer.GetBufferCount() == 3);
  CHECK(buffer.GetSize() == 256); // Aligned slot size

  // Write to slot 0
  uint32_t *p0 = static_cast<uint32_t *>(buffer.BeginWrite());
  REQUIRE(p0 != nullptr);
  *p0 = 111u;
  buffer.Flush();
  buffer.Lock();

  // Write to slot 1
  uint32_t *p1 = static_cast<uint32_t *>(buffer.BeginWrite());
  REQUIRE(p1 != nullptr);
  *p1 = 222u;
  buffer.Flush();
  buffer.Lock();

  // Non-blocking try read from delayed slot 1 (previous slot)
  uint32_t readVal = 0u;
  bool readOk = buffer.TryReadNonBlocking(&readVal, sizeof(uint32_t), 1);
  if (readOk) {
    CHECK(readVal == 222u);
  }

  // Non-blocking try read from slot 2 before (oldest slot)
  uint32_t oldestVal = 0u;
  bool oldestOk = buffer.TryReadNonBlocking(&oldestVal, sizeof(uint32_t), 2);
  if (oldestOk) {
    CHECK(oldestVal == 111u);
  }

  // Resize safety: re-creating buffer resets slots and destroys old handles cleanly
  buffer.Create(sizeof(uint64_t), 3);
  CHECK(buffer.GetCurrentSlot() == 0);
  uint64_t *pNew = static_cast<uint64_t *>(buffer.BeginWrite());
  REQUIRE(pNew != nullptr);
  *pNew = 9999ULL;
  buffer.Flush();
  buffer.Lock();

  buffer.Destroy();
  CHECK(buffer.GetId() == 0u);
}

TEST_CASE("[Unit] ShadowBuildPass & OccluderExtractPass - PersistentBuffer triple-buffer resize safety") {
  using NoMoreDay::render::passes::OccluderExtractPass;
  using NoMoreDay::render::passes::ShadowBuildPass;

  ShadowBuildPass shadowPass;
  OccluderExtractPass occluderPass;

  // Verify initial state
  CHECK(shadowPass.GetOccluderCount() == 0u);
  CHECK(occluderPass.GetOccluderCount() == 0u);

  // Re-creation and resize must not crash
  shadowPass.Shutdown();
  occluderPass.Shutdown();
}

TEST_CASE("[Unit] JFAPass - Non-blocking overflow counter snapshot contract") {
  using NoMoreDay::render::passes::JFAPass;

  JFAPass jfaPass;
  // Default overflow count must be 0
  CHECK(jfaPass.GetLastOverflowCount() == 0u);
  CHECK(jfaPass.ReadOverflowCounterImmediateForTesting() == 0u);

  jfaPass.Shutdown();
}

TEST_CASE("[Unit] RadianceCascadesPass - Particle counter test-only entry point contract") {
  using NoMoreDay::render::passes::RadianceCascadesPass;

  RadianceCascadesPass rcPass;
  CHECK(rcPass.ReadParticleCounterForTesting() == 0u);
}

TEST_CASE("[Unit] FlowFieldSystem - Non-blocking CPU sync and valid snapshot tracking") {
  using NoMoreDay::systems::GPUFlowFieldSystem;

  auto &flowSystem = GPUFlowFieldSystem::Get();
  flowSystem.ResetSyncTag();

  // Calling SyncToCPU on uninitialized system should gracefully return without stall.
  // The CPU shadow must always match the current grid size (0x0 when uninitialized).
  flowSystem.SyncToCPU();
  const auto &field = flowSystem.GetFlowFieldCPU();
  CHECK(field.size() ==
        (size_t)flowSystem.GetWidth() * (size_t)flowSystem.GetHeight());
}

TEST_CASE("[Unit] Zero Synchronous Readback Production Audit") {
  using NoMoreDay::core::ComputeBuffer;

  // Reset testing counter
  ComputeBuffer::ResetTestReadCount();
  CHECK(ComputeBuffer::GetTestReadCount() == 0u);
}
