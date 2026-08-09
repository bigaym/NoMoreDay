#include "doctest.h"

#include "engine/render/shadow/StableLightIdTracker.hpp"

namespace {

using NoMoreDay::render::shadow::StableLightIdTracker;

} // namespace

TEST_CASE("[Unit] Stable Light Id - same fingerprint reuses the same id") {
  StableLightIdTracker tracker;
  const uint32_t first = tracker.Resolve(0x1111ull, 1u);
  const uint32_t second = tracker.Resolve(0x2222ull, 1u);
  REQUIRE(first != second);

  CHECK(tracker.Resolve(0x1111ull, 2u) == first);
  CHECK(tracker.Resolve(0x2222ull, 2u) == second);
  CHECK(tracker.GetEntryCount() == 2u);
}

TEST_CASE("[Unit] Stable Light Id - distinct fingerprints get distinct ids") {
  StableLightIdTracker tracker;
  const uint32_t a = tracker.Resolve(0xA0A0ull, 1u);
  const uint32_t b = tracker.Resolve(0xB0B0ull, 1u);
  const uint32_t c = tracker.Resolve(0xC0C0ull, 1u);
  CHECK(a != b);
  CHECK(b != c);
  CHECK(a != c);
}

TEST_CASE("[Unit] Stable Light Id - entries last seen inside the window survive") {
  StableLightIdTracker tracker;
  const uint32_t oldLight = tracker.Resolve(0xAAAAull, 1u);
  const uint32_t freshLight = tracker.Resolve(0xBBBBull, 50u);

  tracker.Prune(61u, 60u);
  CHECK(tracker.GetEntryCount() == 1u);
  CHECK(tracker.Resolve(0xBBBBull, 62u) == freshLight);

  tracker.Prune(130u, 60u);
  CHECK(tracker.GetEntryCount() == 0u);
  CHECK(tracker.Resolve(0xAAAAull, 131u) != oldLight);
}

TEST_CASE("[Unit] Stable Light Id - Clear resets id space") {
  StableLightIdTracker tracker;
  (void)tracker.Resolve(0x1111ull, 1u);
  tracker.Clear();
  CHECK(tracker.GetEntryCount() == 0u);
  const uint32_t id = tracker.Resolve(0x1111ull, 2u);
  CHECK(id != 0u);
}
