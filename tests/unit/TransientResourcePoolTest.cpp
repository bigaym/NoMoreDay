#include "doctest.h"

#include "engine/render/resources/TransientResourcePool.hpp"

using namespace NoMoreDay::render::resources;

namespace {
// FramebufferHandle has no operator==; compare the GL object ids directly.
bool SameTarget(const FramebufferHandle &a, const FramebufferHandle &b) {
  return a.fbo == b.fbo && a.colorTexture == b.colorTexture;
}
} // namespace

TEST_CASE("[Unit] TransientResourcePool - Alias Group Attribution Is Not Stolen Across Groups (M2)") {
  TransientResourcePool pool;
  pool.SetAliasingEnabled(true);

  constexpr int kWidth = 64;
  constexpr int kHeight = 64;
  constexpr uint32_t kFormat = 0x8058; // GL_RGBA8

  // Frame 1: group 1 creates its entry.
  pool.BeginFrame();
  FramebufferHandle group1First = pool.AcquireAliasedColorTarget(kWidth, kHeight, kFormat, 1);
  REQUIRE(group1First.IsValid());
  pool.EndFrame();

  // Frame 2: group 2 acquires first. It must NOT steal group 1's idle entry
  // (the standard exact-match path used to overwrite entry.aliasGroupId,
  // corrupting attribution when two groups are active in the same frame).
  pool.BeginFrame();
  FramebufferHandle group2Own = pool.AcquireAliasedColorTarget(kWidth, kHeight, kFormat, 2);
  REQUIRE(group2Own.IsValid());
  CHECK_FALSE(SameTarget(group2Own, group1First)); // different group -> separate backing

  // Group 1 acquires later in the same frame and must still hit its own entry.
  FramebufferHandle group1Reuse = pool.AcquireAliasedColorTarget(kWidth, kHeight, kFormat, 1);
  CHECK(SameTarget(group1Reuse, group1First)); // same group -> reuse
  pool.EndFrame();

  // Frame 3: group 1 must still find its own entry (attribution preserved),
  // and group 2 keeps its own entry too.
  pool.BeginFrame();
  FramebufferHandle group1Third = pool.AcquireAliasedColorTarget(kWidth, kHeight, kFormat, 1);
  CHECK(SameTarget(group1Third, group1First));
  FramebufferHandle group2Reuse = pool.AcquireAliasedColorTarget(kWidth, kHeight, kFormat, 2);
  CHECK(SameTarget(group2Reuse, group2Own));
  pool.EndFrame();

  // Two independent groups -> exactly two backing entries (three before the
  // fix, because group 2 stole group 1's entry and group 1 had to re-create).
  CHECK(pool.GetPoolSize() == 2);

  pool.Shutdown();
}