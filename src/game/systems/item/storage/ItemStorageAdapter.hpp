#pragma once

#include "game/foundation/Settings.hpp"
#include "game/foundation/SharedContext.hpp"
#include "game/systems/item/storage/IItemStorageAdapter.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include <memory>

namespace NoMoreDay {

/**
 * @brief Default adapter implementing IItemStorageAdapter.
 * 直接将所有容器操作委托给 ItemStorageService (T-P4-7 双轨期结束，恒定启用 ItemStore)。
 */
class ItemStorageAdapter : public IItemStorageAdapter {
public:
  ItemStorageAdapter(ItemStorageService *service = nullptr,
                     const GameSettings *settings = nullptr);
  ~ItemStorageAdapter() override = default;

  StorageError moveItem(entt::registry &reg, const SlotRef &from,
                        const SlotRef &to) override;
  StorageError swapItem(entt::registry &reg, const SlotRef &a,
                        const SlotRef &b) override;
  StorageError splitStack(entt::registry &reg, const SlotRef &from,
                          const SlotRef &to, uint32_t splitCount) override;
  StorageError mergeStack(entt::registry &reg, const SlotRef &from,
                          const SlotRef &to) override;
  StorageError transferItem(entt::registry &reg, const SlotRef &from,
                            const SlotRef &to) override;
  StorageError autoDeposit(entt::registry &reg, const SlotRef &from,
                           ContainerKind targetKind,
                           uint8_t targetContainer = 0) override;
  void sortContainer(entt::registry &reg, ContainerKind kind,
                     uint8_t container = 0, uint16_t page = 0) override;
  bool canStoreItem(ContainerKind kind, uint8_t container, uint16_t page,
                    ItemHandle handle) const override;
  ItemHandle getSlotHandle(ContainerKind kind, uint8_t container,
                           uint16_t page, uint16_t index) const override;

  [[nodiscard]] bool isItemStoreEnabled() const noexcept {
    return true; // T-P4-7: 恒定启用 ItemStore 轨道
  }

  void setService(ItemStorageService *service) noexcept { m_service = service; }
  void setSettings(const GameSettings *settings) noexcept {
    m_settings = settings;
  }

  static IItemStorageAdapter *GetDefaultAdapter(SharedContext *ctx = nullptr);
  static void SetDefaultAdapter(std::unique_ptr<IItemStorageAdapter> adapter);

private:
  ItemStorageService *m_service = nullptr;
  const GameSettings *m_settings = nullptr;
};

} // namespace NoMoreDay
