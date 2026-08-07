#include "doctest.h"

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <cstdint>
#include <vector>

// BUG-20260807-001 regression: ComputeBuffer::OrphanAndUpload used to reallocate
// the GL backing store to exactly the per-frame payload size, shrinking its
// capacity. Under battle load the caller's `sz > GetSize()` growth check then
// fired on the next frame's +1 instance count, forcing a full
// Release()+Create (glDeleteBuffers+glGenBuffers+glBufferData) cycle on the
// render hot path, which crashed the NVIDIA driver inside glBufferData
// (nvoglv64!DrvPresentBuffers). This test pins the grow-only contract: capacity
// must never shrink below the largest allocation seen.
namespace {

bool EnsureGpuContext() {
  if (NoMoreDay::utils::GPUUtils::IsInitialized()) {
    return true;
  }
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  InitWindow(1, 1, "ComputeBuffer Orphan Capacity Test Window");
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

} // namespace

TEST_CASE("[Integration] ComputeBuffer orphan capacity is grow-only and never shrinks") {
  if (!EnsureGpuContext()) {
    FAIL("Cannot create GPU context; skipping ComputeBuffer orphan capacity test");
  }
  (void)DrainGlErrors();

  NoMoreDay::render::resources::GPUResourceRegistry::Get().Reset();

  constexpr size_t kInitialCapacity = 64 * 48; // e.g. 64 beam instances
  NoMoreDay::core::ComputeBuffer buffer;
  buffer.Create(kInitialCapacity, nullptr, RL_DYNAMIC_DRAW);
  REQUIRE(buffer.GetId() != 0u);
  INFO("after Create: GetSize=" << buffer.GetSize() << " expected=" << kInitialCapacity);
  CHECK(buffer.GetSize() == kInitialCapacity);

  // Small payloads below capacity must NOT shrink the allocation.
  {
    std::vector<uint8_t> small(8 * 48, 0);
    buffer.OrphanAndUpload(small.data(), small.size(), RL_DYNAMIC_DRAW);
    CHECK(buffer.GetSize() == kInitialCapacity);
  }
  {
    std::vector<uint8_t> tiny(1 * 48, 0);
    buffer.OrphanAndUpload(tiny.data(), tiny.size(), RL_DYNAMIC_DRAW);
    CHECK(buffer.GetSize() == kInitialCapacity);
  }

  // A payload larger than capacity must grow the allocation (doubling headroom).
  {
    const size_t over = kInitialCapacity + 48;
    std::vector<uint8_t> payload(over, 1);
    buffer.OrphanAndUpload(payload.data(), payload.size(), RL_DYNAMIC_DRAW);
    CHECK(buffer.GetSize() >= over);
    const size_t grown = buffer.GetSize();
    // Subsequent small uploads must preserve the grown capacity.
    std::vector<uint8_t> small(4 * 48, 0);
    buffer.OrphanAndUpload(small.data(), small.size(), RL_DYNAMIC_DRAW);
    CHECK(buffer.GetSize() == grown);
  }

  // Repeated fluctuating payload sizes (battle-like) must keep the buffer alive
  // with the peak capacity: no delete/recreate churn is observable via GetSize.
  {
    const size_t peak = buffer.GetSize();
    for (int i = 0; i < 60; ++i) {
      const size_t count = 1 + (i % 40);
      std::vector<uint8_t> payload(count * 48, static_cast<uint8_t>(i));
      buffer.OrphanAndUpload(payload.data(), payload.size(), RL_DYNAMIC_DRAW);
      CHECK(buffer.GetSize() >= peak);
      CHECK(buffer.GetId() != 0u);
    }
  }

  // No GL errors must be reported across the whole lifecycle.
  const auto errors = DrainGlErrors();
  CHECK(errors.empty());

  buffer.Release();
  CHECK(buffer.GetId() == 0u);
  CHECK(buffer.GetSize() == 0u);
}
