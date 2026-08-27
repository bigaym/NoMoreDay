#include "game/systems/item/storage/ItemMigration.hpp"
#include <atomic>

namespace NoMoreDay {

static std::atomic<uint64_t> s_nextMigrationId{1};

size_t ItemMigration::countPreservedItems(const ItemStorageService &service) noexcept {
  const auto &store = service.getStore();
  size_t count = 0;

  auto processHandle = [&](ItemHandle h) {
    if (h && store.isValid(h)) {
      ++count;
      const ItemInstance *inst = store.get(h);
      if (inst) {
        for (const auto &sockH : inst->sockets) {
          if (sockH && store.isValid(sockH)) {
            ++count;
          }
        }
      }
    }
  };

  for (ItemHandle h : service.getInventorySlots()) {
    processHandle(h);
  }
  for (ItemHandle h : service.getEquipmentSlots()) {
    processHandle(h);
  }
  for (ItemHandle h : service.getBagSlots()) {
    processHandle(h);
  }
  for (const auto &page : service.getPersonalStashPages()) {
    for (ItemHandle h : page) {
      processHandle(h);
    }
  }
  for (const auto &page : service.getSharedStashPages()) {
    for (ItemHandle h : page) {
      processHandle(h);
    }
  }
  for (ItemHandle h : service.getHeirloomVaultSlots()) {
    processHandle(h);
  }

  return count;
}

SceneMigrationToken
ItemMigration::beginSceneSwitch(ItemStorageService &service) noexcept {
  // 1. Batch-clean and recycle all GroundPending handles back to ItemStore free list
  service.clearGroundPending(true);

  // 2. Count active instances across all persistent containers and their sockets
  const size_t preserved = countPreservedItems(service);

  // 3. Issue verified migration token
  SceneMigrationToken token;
  token.migrationId = s_nextMigrationId.fetch_add(1, std::memory_order_relaxed);
  token.preservedItemCount = preserved;
  token.sourceVersion = service.version();

  return token;
}

bool ItemMigration::endSceneSwitch(ItemStorageService &service,
                                   const SceneMigrationToken &token) noexcept {
  if (!token.isValid()) {
    return false;
  }

  // 1. Ground pending container must be empty post-transition
  if (!service.getGroundPending().empty()) {
    return false;
  }

  const auto &store = service.getStore();

  // 2. Validate every persistent handle and nested socket handle
  size_t currentPreserved = 0;
  bool allValid = true;

  auto validateHandle = [&](ItemHandle h) {
    if (h) {
      if (!store.isValid(h)) {
        allValid = false;
        return;
      }
      ++currentPreserved;
      const ItemInstance *inst = store.get(h);
      if (!inst) {
        allValid = false;
        return;
      }
      for (const auto &sockH : inst->sockets) {
        if (sockH) {
          if (!store.isValid(sockH)) {
            allValid = false;
            return;
          }
          ++currentPreserved;
        }
      }
    }
  };

  for (ItemHandle h : service.getInventorySlots()) {
    validateHandle(h);
  }
  for (ItemHandle h : service.getEquipmentSlots()) {
    validateHandle(h);
  }
  for (ItemHandle h : service.getBagSlots()) {
    validateHandle(h);
  }
  for (const auto &page : service.getPersonalStashPages()) {
    for (ItemHandle h : page) {
      validateHandle(h);
    }
  }
  for (const auto &page : service.getSharedStashPages()) {
    for (ItemHandle h : page) {
      validateHandle(h);
    }
  }
  for (ItemHandle h : service.getHeirloomVaultSlots()) {
    validateHandle(h);
  }

  if (!allValid) {
    return false;
  }

  // 3. Verify total counted matches token
  if (currentPreserved != token.preservedItemCount) {
    return false;
  }

  // 4. Verify pool activeCount exactly equals preserved count (no leak, no unexpected destroy)
  if (store.activeCount() != token.preservedItemCount) {
    return false;
  }

  return true;
}

} // namespace NoMoreDay
