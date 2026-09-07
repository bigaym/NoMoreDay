#include "TestCommon.hpp"
#include "game/foundation/components/CompactEntitySet.hpp"
#include <type_traits>
#include <vector>

namespace NoMoreDay {

TEST_CASE("[Unit] CompactEntitySet - POD and Standard Layout") {
  static_assert(std::is_standard_layout_v<CompactEntitySet<8>>);
  static_assert(std::is_trivially_destructible_v<CompactEntitySet<8>>);
  static_assert(std::is_standard_layout_v<CompactEntitySet<32>>);
  static_assert(std::is_trivially_destructible_v<CompactEntitySet<32>>);

  CompactEntitySet<8> set8;
  CHECK(set8.size() == 0);
  CHECK(set8.capacity() == 8);
  CHECK(set8.empty());
  CHECK_FALSE(set8.full());

  CompactEntitySet<32> set32;
  CHECK(set32.size() == 0);
  CHECK(set32.capacity() == 32);
}

TEST_CASE("[Unit] CompactEntitySet - Deduplication Semantics") {
  CompactEntitySet<8> set;
  const entt::entity e1{10};
  const entt::entity e2{20};

  CHECK(set.insert(e1));
  CHECK(set.size() == 1);
  CHECK(set.contains(e1));
  CHECK_FALSE(set.contains(e2));

  // Duplicate insertion should return false and not grow size
  CHECK_FALSE(set.insert(e1));
  CHECK(set.size() == 1);

  CHECK(set.insert(e2));
  CHECK(set.size() == 2);
  CHECK(set.contains(e2));

  CHECK_FALSE(set.insert(e2));
  CHECK(set.size() == 2);
}

TEST_CASE("[Unit] CompactEntitySet - Capacity and Overflow Policy") {
  CompactEntitySet<8> set;

  for (uint32_t i = 0; i < 8; ++i) {
    CHECK(set.insert(entt::entity{i + 1}));
  }

  CHECK(set.size() == 8);
  CHECK(set.full());

  // 9th insertion should fail gracefully without crashing or heap allocating
  const entt::entity overflow_entity{999};
  CHECK_FALSE(set.insert(overflow_entity));
  CHECK(set.size() == 8);
  CHECK_FALSE(set.contains(overflow_entity));

  // Clear should reset
  set.clear();
  CHECK(set.size() == 0);
  CHECK(set.empty());
  CHECK_FALSE(set.full());
  CHECK_FALSE(set.contains(entt::entity{1}));

  // Re-insert after clear
  CHECK(set.insert(overflow_entity));
  CHECK(set.size() == 1);
  CHECK(set.contains(overflow_entity));
}

TEST_CASE("[Unit] CompactEntitySet - Traversal and Equality") {
  CompactEntitySet<8> setA;
  CompactEntitySet<8> setB;

  std::vector<entt::entity> expected;
  for (uint32_t i = 1; i <= 5; ++i) {
    const entt::entity e{i * 10};
    setA.insert(e);
    setB.insert(e);
    expected.push_back(e);
  }

  CHECK(setA == setB);

  std::vector<entt::entity> iterated;
  for (auto e : setA) {
    iterated.push_back(e);
  }

  CHECK(iterated == expected);

  // Differing set
  CompactEntitySet<8> setC;
  setC.insert(entt::entity{10});
  CHECK_FALSE(setA == setC);
}

} // namespace NoMoreDay
