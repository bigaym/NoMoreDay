#pragma once

#include "game/systems/item/storage/ItemStorageTypes.hpp"
#include <cstdint>
#include <entt/entt.hpp>

namespace NoMoreDay {

/**
 * @brief Unified pure virtual interface for container and item operations across gameplay systems.
 * Isolates business systems from dual-track execution (ItemStore vs legacy ECS entities).
 */
class IItemStorageAdapter {
public:
  virtual StorageError moveItem(entt::registry &reg, const SlotRef &from,
                                const SlotRef &to) = 0;
  virtual StorageError swapItem(entt::registry &reg, const SlotRef &a,
                                const SlotRef &b) = 0;
  virtual StorageError splitStack(entt::registry &reg, const SlotRef &from,
                                  const SlotRef &to, uint32_t splitCount) = 0;
  virtual StorageError mergeStack(entt::registry &reg, const SlotRef &from,
                                  const SlotRef &to) = 0;
  virtual StorageError transferItem(entt::registry &reg, const SlotRef &from,
                                    const SlotRef &to) = 0;
  virtual StorageError autoDeposit(entt::registry &reg, const SlotRef &from,
                                   ContainerKind targetKind,
                                   uint8_t targetContainer = 0) = 0;
  virtual void sortContainer(entt::registry &reg, ContainerKind kind,
                             uint8_t container = 0, uint16_t page = 0) = 0;
  virtual bool canStoreItem(ContainerKind kind, uint8_t container,
                            uint16_t page, ItemHandle handle) const = 0;
  virtual ItemHandle getSlotHandle(ContainerKind kind, uint8_t container,
                                   uint16_t page, uint16_t index) const = 0;
  virtual ~IItemStorageAdapter() = default;
};

} // namespace NoMoreDay
