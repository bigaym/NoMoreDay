#pragma once

#include "game/systems/item/storage/ItemStorageTypes.hpp"
#include "game/systems/item/storage/ItemStore.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace NoMoreDay {

/**
 * @brief Central service managing runtime item instances and storage containers.
 * Owns ItemStore and container slot arrays across Inventory, Equipment, BagSlots,
 * PersonalStash, SharedStash, HeirloomVault, MaterialBank, and GroundPending.
 */
class ItemStorageService {
public:
  static constexpr size_t kInventoryCapacity = 56;
  static constexpr size_t kEquipmentCapacity = 13;
  static constexpr size_t kBagSlotsCapacity = 4;
  static constexpr size_t kPersonalStashMaxPages = 10;
  static constexpr size_t kSharedStashMaxPages = 10;
  static constexpr size_t kStashPageCapacity = 144;
  static constexpr size_t kHeirloomVaultCapacity = 20;

  ItemStorageService();
  ~ItemStorageService() = default;

  ItemStorageService(const ItemStorageService &) = default;
  ItemStorageService &operator=(const ItemStorageService &) = default;
  ItemStorageService(ItemStorageService &&) noexcept = default;
  ItemStorageService &operator=(ItemStorageService &&) noexcept = default;

  // --- Transactional Container Operations ---
  StorageError moveItem(const SlotRef &from, const SlotRef &to);
  StorageError swapItem(const SlotRef &a, const SlotRef &b);
  StorageError splitStack(const SlotRef &from, const SlotRef &to,
                          uint32_t splitCount);
  StorageError mergeStack(const SlotRef &from, const SlotRef &to);
  StorageError transferItem(const SlotRef &from, const SlotRef &to);
  StorageError autoDeposit(const SlotRef &from, ContainerKind targetKind,
                           uint8_t targetContainer = 0);
  void sortContainer(ContainerKind kind, uint8_t container = 0,
                     uint16_t page = 0);
  StorageError destroyItem(const SlotRef &slot, int quantity = -1);
  [[nodiscard]] bool canStoreItem(ContainerKind kind, uint8_t container,
                                  uint16_t page, ItemHandle handle) const;

  // --- Slot Accessors ---
  [[nodiscard]] ItemHandle getSlotHandle(const SlotRef &slot) const;
  bool setSlotHandle(const SlotRef &slot, ItemHandle handle);
  [[nodiscard]] std::vector<ItemHandle>
  getContainerSlots(ContainerKind kind, uint8_t container = 0,
                    uint16_t page = 0) const;

  [[nodiscard]] const std::vector<ItemHandle> &
  getInventorySlots() const noexcept {
    return m_inventorySlots;
  }
  [[nodiscard]] const std::vector<ItemHandle> &
  getEquipmentSlots() const noexcept {
    return m_equipmentSlots;
  }
  [[nodiscard]] const std::vector<ItemHandle> &getBagSlots() const noexcept {
    return m_bagSlots;
  }
  [[nodiscard]] const std::vector<std::vector<ItemHandle>> &
  getPersonalStashPages() const noexcept {
    return m_personalStash;
  }
  [[nodiscard]] const std::vector<std::vector<ItemHandle>> &
  getSharedStashPages() const noexcept {
    return m_sharedStash;
  }
  [[nodiscard]] const std::vector<ItemHandle> &
  getHeirloomVaultSlots() const noexcept {
    return m_heirloomVault;
  }

  // --- Material Bank Account Operations ---
  int32_t addMaterial(uint32_t id, int32_t amount);
  bool removeMaterial(uint32_t id, int32_t amount);
  [[nodiscard]] int32_t getMaterialCount(uint32_t id) const;
  [[nodiscard]] bool hasMaterial(uint32_t id, int32_t amount) const;
  [[nodiscard]] const std::vector<std::pair<uint32_t, int32_t>> &
  getMaterials() const noexcept {
    return m_materialBank;
  }
  void setMaterials(std::vector<std::pair<uint32_t, int32_t>> materials);

  // --- Currency Operations ---
  [[nodiscard]] int32_t getGold() const noexcept { return m_gold; }
  void setGold(int32_t gold) noexcept { m_gold = gold; }
  void addGold(int32_t amount) noexcept { m_gold += amount; }
  bool spendGold(int32_t amount) noexcept {
    if (amount <= 0)
      return true;
    if (m_gold >= amount) {
      m_gold -= amount;
      return true;
    }
    return false;
  }

  // --- Stash Page Management ---
  [[nodiscard]] uint16_t getUnlockedPages(ContainerKind kind) const noexcept;
  bool unlockPage(ContainerKind kind);
  void setUnlockedPages(ContainerKind kind, uint16_t pages) noexcept;

  // --- Ground Pending Management ---
  void addGroundPending(ItemHandle h);
  bool removeGroundPending(ItemHandle h);
  [[nodiscard]] const std::vector<ItemHandle> &
  getGroundPending() const noexcept {
    return m_groundPending;
  }
  void clearGroundPending(bool destroyHandles = true);

  // --- Store Management & Snapshots ---
  [[nodiscard]] const ItemStore &getStore() const noexcept { return m_store; }
  [[nodiscard]] ItemStore &getStoreMutable() noexcept { return m_store; }
  [[nodiscard]] uint64_t version() const noexcept { return m_store.version(); }
  [[nodiscard]] ItemStore freeze() const { return m_store; }

  void clearContainer(ContainerKind kind, uint8_t container = 0,
                      uint16_t page = 0);
  void clearAll();

private:
  ItemHandle *getSlotPointer(const SlotRef &slot);
  const ItemHandle *getSlotPointer(const SlotRef &slot) const;
  uint64_t generateInstanceId() noexcept;

  ItemStore m_store;

  std::vector<ItemHandle> m_inventorySlots;
  std::vector<ItemHandle> m_equipmentSlots;
  std::vector<ItemHandle> m_bagSlots;
  std::vector<std::vector<ItemHandle>> m_personalStash;
  std::vector<std::vector<ItemHandle>> m_sharedStash;
  std::vector<ItemHandle> m_heirloomVault;
  std::vector<std::pair<uint32_t, int32_t>> m_materialBank;
  std::vector<ItemHandle> m_groundPending;

  uint16_t m_personalStashUnlockedPages = 1;
  uint16_t m_sharedStashUnlockedPages = 1;
  int32_t m_gold = 0;
  uint64_t m_nextInstanceId = 1;
};

} // namespace NoMoreDay
