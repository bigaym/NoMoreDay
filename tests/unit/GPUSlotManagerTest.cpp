#include "doctest.h"
#include "engine/render/GPUEntitySync.hpp"
#include "game/components/Common.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay::render;
using namespace NoMoreDay::components;

TEST_CASE("GPUSlotManager - Allocation and Recycling") {
  entt::registry registry;
  GPUSlotManager manager;

  bool callbackCalled = false;
  int recycledSlot = -1;

  manager.Init(10, &registry, [&](int slot) {
    callbackCalled = true;
    recycledSlot = slot;
  });

  CHECK(manager.GetMaxEntities() == 10);

  SUBCASE("Allocate single entity") {
    auto e1 = registry.create();
    registry.emplace<GPUIndex>(e1, -1);
    registry.emplace<Position>(e1);
    registry.emplace<Radius>(e1);

    manager.Process(registry);

    int index1 = registry.get<GPUIndex>(e1).index;
    CHECK(index1 >= 0);
    CHECK(index1 < 10);

    SUBCASE("Recycle entity") {
      registry.destroy(e1);

      CHECK(callbackCalled);
      CHECK(recycledSlot == index1);

      // Reuse
      auto e2 = registry.create();
      registry.emplace<GPUIndex>(e2, -1);
      registry.emplace<Position>(e2);
      registry.emplace<Radius>(e2);

      manager.Process(registry);
      int index2 = registry.get<GPUIndex>(e2).index;

      CHECK(index2 == index1); // LIFO behavior
    }
  }

  SUBCASE("Allocate until full") {
    for (int i = 0; i < 10; ++i) {
      auto e = registry.create();
      registry.emplace<GPUIndex>(e, -1);
      registry.emplace<Position>(e);
      registry.emplace<Radius>(e);
    }

    manager.Process(registry);

    // Next allocation should fail (stay -1)
    auto eFull = registry.create();
    registry.emplace<GPUIndex>(eFull, -1);
    registry.emplace<Position>(eFull);
    registry.emplace<Radius>(eFull);

    manager.Process(registry);
    CHECK(registry.get<GPUIndex>(eFull).index == -1);
  }
}
