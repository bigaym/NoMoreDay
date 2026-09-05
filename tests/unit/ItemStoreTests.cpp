#include "TestCommon.hpp"
#include "game/systems/item/storage/ItemStore.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <type_traits>
#include <unordered_map>
#include <vector>

using namespace NoMoreDay;

TEST_CASE("[Unit] ItemStorageTypes - POD Layout and Constraints") {
  static_assert(sizeof(ItemHandle) == 8, "ItemHandle size must be 8 bytes");
  static_assert(sizeof(CompactAffix) == 8, "CompactAffix size must be 8 bytes");
  static_assert(alignof(ItemInstance) == 8, "ItemInstance alignof must be 8 bytes");
  static_assert(std::is_trivially_copyable_v<ItemHandle>,
                "ItemHandle must be trivially copyable");
  static_assert(std::is_trivially_copyable_v<CompactAffix>,
                "CompactAffix must be trivially copyable");
  static_assert(std::is_trivially_copyable_v<ItemInstance>,
                "ItemInstance must be trivially copyable");

  ItemHandle nullHandle{0, 0};
  CHECK_FALSE(static_cast<bool>(nullHandle));

  ItemHandle validHandle1{1, 0};
  CHECK(static_cast<bool>(validHandle1));

  ItemHandle validHandle2{0, 1};
  CHECK(static_cast<bool>(validHandle2));

  ItemHandle validHandle3{5, 2};
  CHECK(static_cast<bool>(validHandle3));
  CHECK(validHandle3 == ItemHandle{5, 2});
  CHECK(validHandle3 != ItemHandle{5, 3});

  ItemInstance inst;
  CHECK_FALSE(inst.isLocked());
  CHECK_FALSE(inst.isTwoHanded());

  inst.setLocked(true);
  CHECK(inst.isLocked());
  inst.setLocked(false);
  CHECK_FALSE(inst.isLocked());

  inst.setTwoHanded(true);
  CHECK(inst.isTwoHanded());
  inst.setTwoHanded(false);
  CHECK_FALSE(inst.isTwoHanded());

  SlotRef slotA{ContainerKind::Inventory, 0, 1, 10};
  SlotRef slotB{ContainerKind::Inventory, 0, 1, 10};
  SlotRef slotC{ContainerKind::PersonalStash, 1, 0, 5};
  CHECK(slotA == slotB);
  CHECK(slotA != slotC);
}

TEST_CASE("[Unit] ItemStore - Create, Get, and IsValid") {
  ItemStore store;
  CHECK(store.activeCount() == 0);
  CHECK(store.empty());
  CHECK(store.version() == 1);

  ItemInstance proto;
  proto.instanceId = 10001;
  proto.baseId = 1001;
  proto.quantity = 5;
  proto.itemLevel = 25;
  proto.rarity = static_cast<uint8_t>(Rarity::Magic);
  proto.attack = 42.0f;
  proto.setLocked(true);

  ItemHandle h1 = store.create(proto);
  CHECK(h1.index > 0);
  CHECK(h1.gen > 0);
  CHECK(store.isValid(h1));
  CHECK(store.activeCount() == 1);
  CHECK_FALSE(store.empty());
  CHECK(store.version() > 1);

  const ItemInstance *view = store.get(h1);
  REQUIRE(view != nullptr);
  CHECK(view->instanceId == 10001);
  CHECK(view->baseId == 1001);
  CHECK(view->quantity == 5);
  CHECK(view->itemLevel == 25);
  CHECK(view->attack == 42.0f);
  CHECK(view->isLocked());

  ItemInstance *mutView = store.getMutable(h1);
  REQUIRE(mutView != nullptr);
  mutView->quantity = 8;
  CHECK(store.get(h1)->quantity == 8);

  // 无效句柄测试
  CHECK_FALSE(store.isValid(ItemHandle{0, 0}));
  CHECK(store.get(ItemHandle{0, 0}) == nullptr);

  CHECK_FALSE(store.isValid(ItemHandle{9999, 1}));
  CHECK(store.get(ItemHandle{9999, 1}) == nullptr);

  CHECK_FALSE(store.isValid(ItemHandle{h1.index, h1.gen + 1}));
  CHECK(store.get(ItemHandle{h1.index, h1.gen + 1}) == nullptr);
}

TEST_CASE("[Unit] ItemStore - Destroy and Generation Invalidation") {
  ItemStore store;

  ItemInstance p1;
  p1.instanceId = 1;
  ItemInstance p2;
  p2.instanceId = 2;
  ItemInstance p3;
  p3.instanceId = 3;

  ItemHandle h1 = store.create(p1);
  ItemHandle h2 = store.create(p2);
  ItemHandle h3 = store.create(p3);

  CHECK(store.activeCount() == 3);
  CHECK(store.isValid(h1));
  CHECK(store.isValid(h2));
  CHECK(store.isValid(h3));

  const uint64_t vBefore = store.version();
  store.destroy(h2);
  CHECK(store.version() > vBefore);
  CHECK(store.activeCount() == 2);
  CHECK_FALSE(store.isValid(h2));
  CHECK(store.get(h2) == nullptr);
  CHECK(store.getMutable(h2) == nullptr);

  // 剩余句柄不受影响
  CHECK(store.isValid(h1));
  CHECK(store.isValid(h3));
  CHECK(store.get(h1)->instanceId == 1);
  CHECK(store.get(h3)->instanceId == 3);

  // 重新分配应复用槽位 h2.index 且代际自增
  ItemInstance p4;
  p4.instanceId = 4;
  ItemHandle h4 = store.create(p4);

  CHECK(h4.index == h2.index);
  CHECK(h4.gen == h2.gen + 1);
  CHECK(store.isValid(h4));
  CHECK(store.get(h4)->instanceId == 4);

  // 过期的句柄 h2 依然无效
  CHECK_FALSE(store.isValid(h2));
  CHECK(store.get(h2) == nullptr);

  // 销毁无效句柄是安全的空操作
  const size_t cnt = store.activeCount();
  store.destroy(h2);
  CHECK(store.activeCount() == cnt);
}

TEST_CASE("[Unit] ItemStore - Mutate and Version Tracking") {
  ItemStore store;

  ItemInstance p;
  p.instanceId = 77;
  p.quantity = 1;
  p.value = 100.0f;

  ItemHandle h = store.create(p);
  const uint64_t v0 = store.version();

  bool mutated = store.mutate(h, [](ItemInstance &inst) {
    inst.quantity += 9;
    inst.value = 550.0f;
  });

  CHECK(mutated);
  CHECK(store.get(h)->quantity == 10);
  CHECK(store.get(h)->value == 550.0f);
  CHECK(store.version() > v0);

  // 修改无效句柄失败且不抛出异常
  const uint64_t v1 = store.version();
  bool mutatedBad =
      store.mutate(ItemHandle{999, 1}, [](ItemInstance &inst) { inst.quantity = 0; });
  CHECK_FALSE(mutatedBad);
  CHECK(store.version() == v1);
}

TEST_CASE("[Unit] ItemStore - Side-Table Sparse Storage") {
  ItemStore store;

  ItemInstance p;
  p.instanceId = 888;
  ItemHandle h = store.create(p);

  CHECK(store.getSideTable(h) == nullptr);
  CHECK(store.getSideTableMutable(h) == nullptr);

  ItemSideTableData side;
  StatConversion conv;
  conv.source = StatType::PhysicalDamage;
  conv.target = StatType::FireDamage;
  conv.ratio = 0.6f;
  side.conversions.push_back(conv);

  DamageModifier mod;
  mod.value = 30.0f;
  mod.type = ModifierType::More;
  side.damage_modifiers.push_back(mod);

  store.setSideTable(h, side);

  const ItemSideTableData *fetched = store.getSideTable(h);
  REQUIRE(fetched != nullptr);
  REQUIRE(fetched->conversions.size() == 1);
  CHECK(fetched->conversions[0].source == StatType::PhysicalDamage);
  CHECK(fetched->conversions[0].target == StatType::FireDamage);
  CHECK(fetched->conversions[0].ratio == 0.6f);
  REQUIRE(fetched->damage_modifiers.size() == 1);
  CHECK(fetched->damage_modifiers[0].value == 30.0f);

  // 销毁物品会清理旁表
  store.destroy(h);
  CHECK(store.getSideTable(h) == nullptr);
}

TEST_CASE("[Unit] ItemStore - Visit Iteration") {
  ItemStore store;

  std::vector<ItemHandle> created;
  for (uint32_t i = 1; i <= 10; ++i) {
    ItemInstance p;
    p.instanceId = i * 100;
    created.push_back(store.create(p));
  }

  // 销毁索引 1 (id=200) 和索引 4 (id=500) 处的物品
  store.destroy(created[1]);
  store.destroy(created[4]);
  CHECK(store.activeCount() == 8);

  std::vector<uint64_t> visitedIds;
  store.visit([&](ItemHandle handle, const ItemInstance &inst) {
    CHECK(store.isValid(handle));
    visitedIds.push_back(inst.instanceId);
  });

  CHECK(visitedIds.size() == 8);
  for (uint64_t id : visitedIds) {
    CHECK(id != 200);
    CHECK(id != 500);
  }
}

TEST_CASE("[Unit] ItemStore - Clear and Reserve") {
  ItemStore store;
  for (int i = 0; i < 5; ++i) {
    ItemInstance p;
    p.instanceId = i;
    store.create(p);
  }
  CHECK(store.activeCount() == 5);

  const uint64_t vBefore = store.version();
  store.clear();
  CHECK(store.activeCount() == 0);
  CHECK(store.empty());
  CHECK(store.version() > vBefore);

  store.reserve(500);
  CHECK(store.capacity() >= 500);
}

