#include "doctest.h"

#include "engine/render/resources/GPUTexturePool.hpp"
#include "engine/render/resources/FramebufferManager.hpp"

using namespace NoMoreDay::render::resources;
using namespace NoMoreDay::render::core;

namespace {
// Test stubs for GPUTexturePool::SetSyncPollForTesting (B3/H5): simulate a
// retire fence that always signals immediately vs. one that never signals,
// without touching a real GL context. The pool polls through the stub instead
// of GPUUtils::ClientWaitSync, so the retire-queue logic is deterministic.
uint32_t AlwaysSignaledSyncPoll(void *, uint32_t, uint64_t) {
  return 0x911A; // GL_ALREADY_SIGNALED
}
uint32_t NeverSignaledSyncPoll(void *, uint32_t, uint64_t) {
  return 0x911B; // GL_TIMEOUT_EXPIRED
}
} // namespace

TEST_SUITE("GPU Texture Pool & Resize Debouncer (AD-8)") {

TEST_CASE("TextureSizeClass classification and standard extent") {
  SUBCASE("1080p standard classifications") {
    CHECK(ClassifySize(1920, 1080) == TextureSizeClass::FullHD);
    CHECK(ClassifySize(960, 540) == TextureSizeClass::HalfRes);
    CHECK(ClassifySize(480, 270) == TextureSizeClass::QuarterRes);
    CHECK(ClassifySize(2048, 2048) == TextureSizeClass::Atlas2048);
    CHECK(ClassifySize(1024, 1024) == TextureSizeClass::Atlas1024);
    CHECK(ClassifySize(1280, 720) == TextureSizeClass::Custom);
  }

  SUBCASE("Standard Extent retrieval") {
    StandardExtent extFull = GetStandardExtent(TextureSizeClass::FullHD, 1920, 1080);
    CHECK(extFull.width == 1920);
    CHECK(extFull.height == 1080);

    StandardExtent extHalf = GetStandardExtent(TextureSizeClass::HalfRes, 1920, 1080);
    CHECK(extHalf.width == 960);
    CHECK(extHalf.height == 540);

    StandardExtent extQuarter = GetStandardExtent(TextureSizeClass::QuarterRes, 1920, 1080);
    CHECK(extQuarter.width == 480);
    CHECK(extQuarter.height == 270);

    StandardExtent extAtlas2k = GetStandardExtent(TextureSizeClass::Atlas2048);
    CHECK(extAtlas2k.width == 2048);
    CHECK(extAtlas2k.height == 2048);
  }
}

TEST_CASE("ResizeDebouncer 200ms debounce window behavior") {
  ResizeDebouncer debouncer;
  debouncer.Reset(1920, 1080);

  CHECK_FALSE(debouncer.IsDebouncing());
  CHECK(debouncer.GetEffectiveWidth() == 1920);
  CHECK(debouncer.GetEffectiveHeight() == 1080);

  int appliedW = 0;
  int appliedH = 0;

  SUBCASE("Resize request triggers debouncing state") {
    debouncer.RequestResize(1280, 720, 1.0);
    CHECK(debouncer.IsDebouncing());
    CHECK(debouncer.GetPendingWidth() == 1280);
    CHECK(debouncer.GetPendingHeight() == 720);
    CHECK(debouncer.GetEffectiveWidth() == 1920); // Not applied yet

    // Advance 100ms (< 200ms window)
    bool applied = debouncer.Update(1.10, appliedW, appliedH);
    CHECK_FALSE(applied);
    CHECK(debouncer.IsDebouncing());

    // Advance to 250ms (> 200ms window)
    applied = debouncer.Update(1.25, appliedW, appliedH);
    CHECK(applied);
    CHECK_FALSE(debouncer.IsDebouncing());
    CHECK(appliedW == 1280);
    CHECK(appliedH == 720);
    CHECK(debouncer.GetEffectiveWidth() == 1280);
    CHECK(debouncer.GetEffectiveHeight() == 720);
  }

  SUBCASE("Continuous resize dragging extends debounce window") {
    debouncer.RequestResize(1600, 900, 1.0);
    CHECK(debouncer.IsDebouncing());

    // Another event at 1.1s (100ms later)
    debouncer.RequestResize(1440, 810, 1.1);
    CHECK(debouncer.GetPendingWidth() == 1440);

    // At 1.25s (150ms after second event, < 200ms)
    bool applied = debouncer.Update(1.25, appliedW, appliedH);
    CHECK_FALSE(applied);

    // At 1.35s (250ms after second event, > 200ms)
    applied = debouncer.Update(1.35, appliedW, appliedH);
    CHECK(applied);
    CHECK(appliedW == 1440);
    CHECK(appliedH == 810);
  }

  SUBCASE("Flush immediately applies pending resize") {
    debouncer.RequestResize(800, 600, 1.0);
    CHECK(debouncer.IsDebouncing());

    debouncer.Flush(appliedW, appliedH);
    CHECK_FALSE(debouncer.IsDebouncing());
    CHECK(appliedW == 800);
    CHECK(appliedH == 600);
    CHECK(debouncer.GetEffectiveWidth() == 800);
    CHECK(debouncer.GetEffectiveHeight() == 600);
  }
}

TEST_CASE("GPUTexturePool lifecycle and delayed retirement") {
  auto &pool = GPUTexturePool::Get();
  pool.Shutdown();
  pool.ResetStats();
  pool.SetMinRetireFrames(3);
  // Deterministic fence polling for every subcase below (see stub namespace).
  // Restored to nullptr by the B3/H5 subcases at the end of this TEST_CASE.
  pool.SetSyncPollForTesting(AlwaysSignaledSyncPoll);

  SUBCASE("Pool stats and hit rate tracking") {
    pool.BeginFrame(1);
    
    // Key-based standard acquire
    TexturePoolKey key{};
    key.width = 1920;
    key.height = 1080;
    key.internalFormat = 0x8058;
    key.sizeClass = TextureSizeClass::FullHD;
    key.tier = QualityTier::Ultra;

    FramebufferHandle handle1 = pool.Acquire(key);
    if (!handle1.IsValid()) {
      handle1.fbo = 101;
      handle1.colorTexture = 201;
      handle1.width = 1920;
      handle1.height = 1080;
      handle1.internalFormat = 0x8058;
      // H5 (P2 AD-8): the fabricated handle must carry the same tier the key
      // was acquired with, otherwise Release() rebuilds a mismatched bucket key.
      handle1.tier = QualityTier::Ultra;
    }
    auto stats = pool.GetStats();
    CHECK(stats.totalAcquires == 1);
    CHECK(stats.poolMisses == 1);

    // Release back to pool (enters pending retire queue)
    pool.Release(handle1);
    pool.EndFrame();

    // Advance 3 frames so handle1 retires back into m_availablePool
    pool.AdvanceFrameForTesting(3);

    // Re-acquire should now hit the recycled pool
    pool.BeginFrame(5);
    FramebufferHandle handle2 = pool.Acquire(key);
    CHECK(handle2.IsValid());
    stats = pool.GetStats();
    CHECK(stats.totalAcquires == 2);
    CHECK(stats.poolHits == 1);
    CHECK(stats.GetHitRate() >= 0.5f);

    pool.Release(handle2);
    pool.EndFrame();
  }

  SUBCASE("3-frame retirement queue processing") {
    pool.Shutdown();
    pool.ResetStats();

    pool.BeginFrame(10);
    
    FramebufferHandle res = pool.Acquire(960, 540, 0x881A);
    if (!res.IsValid()) {
      res.fbo = 102;
      res.colorTexture = 202;
      res.width = 960;
      res.height = 540;
      res.internalFormat = 0x881A;
    }
    CHECK(res.IsValid());

    // Request delayed retirement
    pool.RetireOldResource(res, nullptr);
    CHECK_FALSE(res.IsValid()); // Handle invalidated immediately

    auto stats = pool.GetStats();
    CHECK(stats.pendingRetireCount == 1);

    pool.EndFrame();

    // Frame 11 (1 frame elapsed < 3 min frames)
    pool.BeginFrame(11);
    pool.EndFrame();
    CHECK(pool.GetStats().pendingRetireCount == 1);

    // Frame 12 (2 frames elapsed < 3 min frames)
    pool.BeginFrame(12);
    pool.EndFrame();
    CHECK(pool.GetStats().pendingRetireCount == 1);
// Frame 13 (3 frames elapsed >= 3 min frames) -> Retired / Recycled
    pool.BeginFrame(13);
    pool.EndFrame();
    CHECK(pool.GetStats().pendingRetireCount == 0);
  }

  SUBCASE("Fence never signals keeps resource pending (B3)") {
    // Override the TEST_CASE default stub: the retire fence never signals.
    pool.SetSyncPollForTesting(NeverSignaledSyncPoll);

    pool.BeginFrame(20);
    FramebufferHandle res = pool.Acquire(960, 540, 0x881A);
    if (!res.IsValid()) {
      res.fbo = 104;
      res.colorTexture = 204;
      res.width = 960;
      res.height = 540;
      res.internalFormat = 0x881A;
    }
    // Pass a non-null fence so the entry actually polls; the stub reports
    // GL_TIMEOUT_EXPIRED forever (simulating a fence the GPU never signals).
    pool.RetireOldResource(res, reinterpret_cast<void *>(0x1));
    CHECK_FALSE(res.IsValid()); // Handle invalidated immediately

    auto stats = pool.GetStats();
    CHECK(stats.pendingRetireCount == 1);
    pool.EndFrame();

    // Advance far past the 3-frame minimum AND the 60-frame warn cap: while the
    // fence is unsignaled the resource must NEVER be destroyed or recycled
    // (B3: a timeout is NOT permission to retire — the GPU may still reference it).
    for (uint64_t f = 21; f <= 90; ++f) {
      pool.BeginFrame(f);
      pool.EndFrame();
    }
    CHECK(pool.GetStats().pendingRetireCount == 1);

    // Restore real polling for any test that runs after this one.
    pool.SetSyncPollForTesting(nullptr);
  }

  SUBCASE("Different tier / sizeClass keys do not cross-hit (H5)") {
    // TEST_CASE preamble installed AlwaysSignaledSyncPoll for deterministic
    // recycle into the pool.

    pool.BeginFrame(50);
    // Retire an Ultra/FullHD target back into the pool.
    TexturePoolKey keyUltra{};
    keyUltra.width = 1920;
    keyUltra.height = 1080;
    keyUltra.internalFormat = 0x8058;
    keyUltra.sizeClass = TextureSizeClass::FullHD;
    keyUltra.tier = QualityTier::Ultra;
    FramebufferHandle ultra = pool.Acquire(keyUltra);
    if (!ultra.IsValid()) {
      ultra.fbo = 105;
      ultra.colorTexture = 205;
      ultra.width = 1920;
      ultra.height = 1080;
      ultra.internalFormat = 0x8058;
      ultra.tier = QualityTier::Ultra;
    }
    pool.Release(ultra);
    pool.EndFrame();
    pool.AdvanceFrameForTesting(3); // retire back into the Ultra/FullHD bucket

    // Same format/dims but a different tier must NOT hit the Ultra bucket.
    pool.BeginFrame(60);
    TexturePoolKey keyMedium = keyUltra;
    keyMedium.tier = QualityTier::Medium;
    FramebufferHandle medium = pool.Acquire(keyMedium);
    if (!medium.IsValid()) {
      medium.fbo = 106;
      medium.colorTexture = 206;
      medium.width = 1920;
      medium.height = 1080;
      medium.internalFormat = 0x8058;
      medium.tier = QualityTier::Medium;
    }
    auto stats = pool.GetStats();
    CHECK(stats.poolHits == 0);   // no cross-tier reuse
    CHECK(stats.poolMisses == 2); // ultra + medium both missed

    // Same dims but a different sizeClass must not hit either.
    TexturePoolKey keyCustom = keyUltra;
    keyCustom.sizeClass = TextureSizeClass::Custom;
    FramebufferHandle custom = pool.Acquire(keyCustom);
    if (!custom.IsValid()) {
      custom.fbo = 107;
      custom.colorTexture = 207;
      custom.width = 1920;
      custom.height = 1080;
      custom.internalFormat = 0x8058;
      custom.tier = QualityTier::Ultra;
    }
    stats = pool.GetStats();
    CHECK(stats.poolHits == 0);   // no cross-sizeClass reuse
    CHECK(stats.poolMisses == 3);

    // Re-acquiring with the exact same key now hits the pool.
    FramebufferHandle ultra2 = pool.Acquire(keyUltra);
    CHECK(ultra2.IsValid());
    stats = pool.GetStats();
    CHECK(stats.poolHits == 1);
    CHECK(stats.poolMisses == 3);
    CHECK(stats.GetHitRate() >= 0.25f);

    pool.Release(medium);
    pool.Release(custom);
    pool.Release(ultra2);
    pool.EndFrame();

    // Restore real polling for any test that runs after this one.
    pool.SetSyncPollForTesting(nullptr);
  }

}

}
