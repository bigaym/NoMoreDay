#include "TestCommon.hpp"
#include "game/systems/item/storage/ItemStorageConverter.hpp"
#include "game/systems/item/storage/ItemStore.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <type_traits>
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

  // Invalid handle tests
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

  // Remaining handles are unaffected
  CHECK(store.isValid(h1));
  CHECK(store.isValid(h3));
  CHECK(store.get(h1)->instanceId == 1);
  CHECK(store.get(h3)->instanceId == 3);

  // Re-allocating should reuse slot h2.index with gen incremented
  ItemInstance p4;
  p4.instanceId = 4;
  ItemHandle h4 = store.create(p4);

  CHECK(h4.index == h2.index);
  CHECK(h4.gen == h2.gen + 1);
  CHECK(store.isValid(h4));
  CHECK(store.get(h4)->instanceId == 4);

  // Stale handle h2 is still invalid
  CHECK_FALSE(store.isValid(h2));
  CHECK(store.get(h2) == nullptr);

  // Destroying invalid handle is a safe no-op
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

  // Mutating invalid handle fails and does not throw
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

  // Destroying item cleans up side table
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

  // Destroy items at index 1 (id=200) and index 4 (id=500)
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

TEST_CASE("[Unit] ItemStorageConverter - Component to Instance and Back (Round-Trip)") {
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemComponent comp;
  comp.id = 5566;
  comp.baseId = 1001; // Iron Shortsword
  comp.itemLevel = 45;
  comp.quantity = 1;
  comp.rarity = Rarity::Rare;
  comp.isLocked = true;
  comp.isTwoHanded = false;
  comp.forgingPotential = 35;
  comp.legendaryPotential = 2;
  comp.socketCount = 3;
  comp.attack = 22.5f;
  comp.defense = 0.0f;
  comp.value = 350.0f;
  comp.activeRunewordId = 12;

  Affix a1;
  a1.type = AffixType::FlatPhysicalDamage;
  a1.tier = 4;
  a1.isPrefix = true;
  a1.value = 20.0f;

  Affix a2;
  a2.type = AffixType::AttackSpeed;
  a2.tier = 3;
  a2.isPrefix = false;
  a2.value = 10.0f;

  Affix a3;
  a3.type = AffixType::CritChance;
  a3.tier = 2;
  a3.isPrefix = false;
  a3.value = 6.0f;

  comp.affixes = {a1, a2, a3};

  StatConversion conv;
  conv.source = StatType::PhysicalDamage;
  conv.target = StatType::ColdDamage;
  conv.ratio = 0.5f;
  comp.conversions.push_back(conv);

  DamageModifier dmgMod;
  dmgMod.value = 15.0f;
  dmgMod.type = ModifierType::Increased;
  comp.damage_modifiers.push_back(dmgMod);

  // Convert to ItemInstance
  ItemInstance inst;
  ItemSideTableData side;
  REQUIRE(ItemComponentToInstance(comp, inst, &side));

  CHECK(inst.instanceId == 5566);
  CHECK(inst.baseId == 1001);
  CHECK(inst.itemLevel == 45);
  CHECK(inst.quantity == 1);
  CHECK(inst.rarity == static_cast<uint8_t>(Rarity::Rare));
  CHECK(inst.isLocked());
  CHECK_FALSE(inst.isTwoHanded());
  CHECK(inst.forgingPotential == 35);
  CHECK(inst.legendaryPotential == 2);
  CHECK(inst.socketCount == 3);
  CHECK(inst.attack == 22.5f);
  CHECK(inst.value == 350.0f);
  CHECK(inst.activeRunewordId == 12);
  CHECK(inst.affixCount == 3);
  CHECK(inst.affixes[0].type == AffixType::FlatPhysicalDamage);
  CHECK(inst.affixes[0].value == 20.0f);
  CHECK(inst.affixes[0].tier == 4);
  CHECK(inst.affixes[0].isPrefix == true);
  CHECK(side.conversions.size() == 1);
  CHECK(side.damage_modifiers.size() == 1);

  // Convert back to ItemComponent
  ItemComponent restored;
  REQUIRE(InstanceToItemComponent(inst, restored, &side));

  // Verify static properties populated from template registry
  CHECK(restored.name == "锈蚀铁剑");
  CHECK(restored.type == ItemType::Weapon);
  CHECK(restored.slot == EquipmentSlot::MainHand);
  CHECK(restored.weaponSubtype == WeaponSubtype::Sword);

  // Verify dynamic fields
  CHECK(restored.id == 5566);
  CHECK(restored.baseId == 1001);
  CHECK(restored.itemLevel == 45);
  CHECK(restored.quantity == 1);
  CHECK(restored.rarity == Rarity::Rare);
  CHECK(restored.isLocked == true);
  CHECK(restored.isTwoHanded == false);
  CHECK(restored.forgingPotential == 35);
  CHECK(restored.legendaryPotential == 2);
  CHECK(restored.socketCount == 3);
  CHECK(restored.sockets.size() == 3);
  CHECK(restored.attack == 22.5f);
  CHECK(restored.value == 350.0f);
  CHECK(restored.activeRunewordId == 12);
  REQUIRE(restored.affixes.size() == 3);
  CHECK(restored.affixes[0].type == AffixType::FlatPhysicalDamage);
  CHECK(restored.affixes[0].value == 20.0f);
  CHECK(restored.affixes[0].tier == 4);
  CHECK(restored.affixes[0].isPrefix == true);
  CHECK(restored.affixes[1].type == AffixType::AttackSpeed);
  CHECK(restored.affixes[1].value == 10.0f);
  CHECK(restored.affixes[2].type == AffixType::CritChance);
  CHECK(restored.affixes[2].value == 6.0f);

  REQUIRE(restored.conversions.size() == 1);
  CHECK(restored.conversions[0].source == StatType::PhysicalDamage);
  CHECK(restored.conversions[0].target == StatType::ColdDamage);
  CHECK(restored.conversions[0].ratio == 0.5f);

  REQUIRE(restored.damage_modifiers.size() == 1);
  CHECK(restored.damage_modifiers[0].value == 15.0f);
}

TEST_CASE("[Unit] ItemStorageConverter - Max Affixes and Sockets Bounds") {
  ItemTemplateRegistry::Instance().initializeDefaults();

  ItemComponent comp;
  comp.id = 999;
  comp.baseId = 1001;
  comp.socketCount = 6;

  for (uint16_t i = 0; i < 15; ++i) {
    Affix aff;
    aff.type = static_cast<AffixType>(static_cast<uint16_t>(AffixType::Strength) + (i % 5));
    aff.tier = 1 + (i % 7);
    aff.isPrefix = (i % 2 == 0);
    aff.value = 10.0f + static_cast<float>(i);
    comp.affixes.push_back(aff);
  }

  ItemInstance inst;
  ItemComponentToInstance(comp, inst);

  // Clamped to 12
  CHECK(inst.affixCount == 12);
  CHECK(inst.socketCount == 6);

  ItemComponent restored;
  InstanceToItemComponent(inst, restored);
  CHECK(restored.affixes.size() == 12);
  CHECK(restored.socketCount == 6);
  CHECK(restored.sockets.size() == 6);
}
