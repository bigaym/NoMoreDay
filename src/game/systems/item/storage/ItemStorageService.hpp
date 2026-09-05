#pragma once

#include "game/foundation/components/InventoryComponent.hpp"
#include "game/systems/item/storage/ItemStorageTypes.hpp"
#include "game/systems/item/storage/ItemStore.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace NoMoreDay {

/**
 * @brief 管理运行时物品实例与存储容器的核心服务。
 * 持有 ItemStore 以及覆盖 Inventory、Equipment、BagSlots、
 * PersonalStash、SharedStash、HeirloomVault 与 MaterialBank 的容器槽位数组。
 */
class ItemStorageService {
public:
  // 玩家背包容量的单一真实来源为 InventoryComponent::BASE_CAPACITY（传统 ECS 轨道权威；
  // SaveManager 快照同样读取该值）。下方 store-track 常量镜像该值而非自行声明，
  // 静态断言防止两个轨道在 T-P3-3 统一步伐前产生偏差。
  static constexpr size_t kInventoryCapacity =
      static_cast<size_t>(InventoryComponent::BASE_CAPACITY);
  static constexpr size_t kEquipmentCapacity = 13;
  static constexpr size_t kBagSlotsCapacity =
      static_cast<size_t>(InventoryComponent::MAX_BAG_SLOTS);
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

  using SlotFilterPredicate = std::function<bool(const SlotRef &, ItemHandle)>;

  // --- 事务性容器操作 ---
  StorageError moveItem(const SlotRef &from, const SlotRef &to);
  StorageError swapItem(const SlotRef &a, const SlotRef &b);
  StorageError splitStack(const SlotRef &from, const SlotRef &to,
                          uint32_t splitCount);
  StorageError mergeStack(const SlotRef &from, const SlotRef &to);
  StorageError transferItem(const SlotRef &from, const SlotRef &to);
  StorageError autoDeposit(const SlotRef &from, ContainerKind targetKind,
                           uint8_t targetContainer = 0);
  void sortContainer(ContainerKind kind, uint8_t container = 0,
                     uint16_t page = 0,
                     SlotFilterPredicate filter = nullptr);
  StorageError destroyItem(const SlotRef &slot, int quantity = -1);
  [[nodiscard]] bool canStoreItem(ContainerKind kind, uint8_t container,
                                  uint16_t page, ItemHandle handle) const;

  // --- 槽位访问器 ---
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

  // --- 材料银行账户操作 ---
  int32_t addMaterial(uint32_t id, int32_t amount);
  bool removeMaterial(uint32_t id, int32_t amount);
  [[nodiscard]] int32_t getMaterialCount(uint32_t id) const;
  [[nodiscard]] bool hasMaterial(uint32_t id, int32_t amount) const;
  [[nodiscard]] const std::vector<std::pair<uint32_t, int32_t>> &
  getMaterials() const noexcept {
    return m_materialBank;
  }
  void setMaterials(std::vector<std::pair<uint32_t, int32_t>> materials);

  // --- 货币操作 ---
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
  [[nodiscard]] uint64_t getNextInstanceId() const noexcept {
    return m_nextInstanceId;
  }
  void setNextInstanceId(uint64_t nextId) noexcept {
    m_nextInstanceId = nextId;
  }

  // --- 仓库分页管理与元数据 ---
  [[nodiscard]] uint16_t getUnlockedPages(ContainerKind kind) const noexcept;
  bool unlockPage(ContainerKind kind);
  void setUnlockedPages(ContainerKind kind, uint16_t pages) noexcept;

  [[nodiscard]] const std::vector<StashTabMeta> &
  getPersonalStashMeta() const noexcept {
    return m_personalStashMeta;
  }
  void setPersonalStashMeta(std::vector<StashTabMeta> meta) {
    m_personalStashMeta = std::move(meta);
  }
  [[nodiscard]] const std::vector<StashTabMeta> &
  getSharedStashMeta() const noexcept {
    return m_sharedStashMeta;
  }
  void setSharedStashMeta(std::vector<StashTabMeta> meta) {
    m_sharedStashMeta = std::move(meta);
  }

  // --- Store 管理与快照 ---
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
  std::vector<StashTabMeta> m_personalStashMeta;
  std::vector<StashTabMeta> m_sharedStashMeta;
  std::vector<ItemHandle> m_heirloomVault;
  std::vector<std::pair<uint32_t, int32_t>> m_materialBank;

  uint16_t m_personalStashUnlockedPages = 1;
  uint16_t m_sharedStashUnlockedPages = 1;
  int32_t m_gold = 0;
  uint64_t m_nextInstanceId = 1;
};

} // namespace NoMoreDay
