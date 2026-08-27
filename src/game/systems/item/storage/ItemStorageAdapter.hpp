#pragma once

#include "game/foundation/Settings.hpp"
#include "game/foundation/SharedContext.hpp"
#include "game/systems/item/storage/IItemStorageAdapter.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include <memory>

namespace NoMoreDay {

/**
 * @brief Default adapter implementing IItemStorageAdapter.
 * Encapsulates settings->useItemStore decision to route operations either to
 * ItemStorageService (ItemStore track) or legacy ECS container entities.
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
    return m_settings && m_settings->useItemStore && m_service;
  }

  void setService(ItemStorageService *service) noexcept { m_service = service; }
  void setSettings(const GameSettings *settings) noexcept {
    m_settings = settings;
  }

  static IItemStorageAdapter *GetDefaultAdapter(SharedContext *ctx = nullptr);
  static void SetDefaultAdapter(std::unique_ptr<IItemStorageAdapter> adapter);

private:
  entt::entity *getEntitySlot(entt::registry &reg, const SlotRef &slot);
  const entt::entity *getEntitySlot(const entt::registry &reg,
                                    const SlotRef &slot) const;

  ItemStorageService *m_service = nullptr;
  const GameSettings *m_settings = nullptr;
};

} // namespace NoMoreDay
