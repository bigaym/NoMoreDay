#include "game/systems/item/storage/ItemStorageAdapter.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>

namespace NoMoreDay {

namespace {
std::unique_ptr<IItemStorageAdapter> s_customAdapter = nullptr;
std::unique_ptr<ItemStorageAdapter> s_fallbackAdapter = nullptr;
} // namespace

ItemStorageAdapter::ItemStorageAdapter(ItemStorageService *service,
                                       const GameSettings *settings)
    : m_service(service), m_settings(settings) {}

IItemStorageAdapter *ItemStorageAdapter::GetDefaultAdapter(SharedContext *ctx) {
  if (s_customAdapter) {
    return s_customAdapter.get();
  }

  if (!s_fallbackAdapter) {
    s_fallbackAdapter = std::make_unique<ItemStorageAdapter>();
  }

  if (ctx) {
    s_fallbackAdapter->setService(ctx->itemStorage);
    s_fallbackAdapter->setSettings(ctx->settings);
  } else {
    s_fallbackAdapter->setService(nullptr);
    s_fallbackAdapter->setSettings(nullptr);
  }

  return s_fallbackAdapter.get();
}

void ItemStorageAdapter::SetDefaultAdapter(
    std::unique_ptr<IItemStorageAdapter> adapter) {
  s_customAdapter = std::move(adapter);
}

StorageError ItemStorageAdapter::moveItem(entt::registry &reg,
                                          const SlotRef &from,
                                          const SlotRef &to) {
  (void)reg;
  if (!m_service) {
    return StorageError::NotFound;
  }
  return m_service->moveItem(from, to);
}

StorageError ItemStorageAdapter::swapItem(entt::registry &reg,
                                          const SlotRef &a, const SlotRef &b) {
  (void)reg;
  if (!m_service) {
    return StorageError::NotFound;
  }
  return m_service->swapItem(a, b);
}

StorageError ItemStorageAdapter::splitStack(entt::registry &reg,
                                            const SlotRef &from,
                                            const SlotRef &to,
                                            uint32_t splitCount) {
  (void)reg;
  if (!m_service) {
    return StorageError::NotFound;
  }
  return m_service->splitStack(from, to, splitCount);
}

StorageError ItemStorageAdapter::mergeStack(entt::registry &reg,
                                            const SlotRef &from,
                                            const SlotRef &to) {
  (void)reg;
  if (!m_service) {
    return StorageError::NotFound;
  }
  return m_service->mergeStack(from, to);
}

StorageError ItemStorageAdapter::transferItem(entt::registry &reg,
                                              const SlotRef &from,
                                              const SlotRef &to) {
  (void)reg;
  if (!m_service) {
    return StorageError::NotFound;
  }
  return m_service->transferItem(from, to);
}

StorageError ItemStorageAdapter::autoDeposit(entt::registry &reg,
                                             const SlotRef &from,
                                             ContainerKind targetKind,
                                             uint8_t targetContainer) {
  (void)reg;
  if (!m_service) {
    return StorageError::NotFound;
  }
  return m_service->autoDeposit(from, targetKind, targetContainer);
}

void ItemStorageAdapter::sortContainer(entt::registry &reg, ContainerKind kind,
                                       uint8_t container, uint16_t page) {
  (void)reg;
  if (m_service) {
    m_service->sortContainer(kind, container, page);
  }
}

bool ItemStorageAdapter::canStoreItem(ContainerKind kind, uint8_t container,
                                      uint16_t page,
                                      ItemHandle handle) const {
  if (m_service) {
    return m_service->canStoreItem(kind, container, page, handle);
  }
  LOG_ERROR("ItemStorageAdapter::canStoreItem: m_service is null, rejecting canStoreItem");
  return false;
}

ItemHandle ItemStorageAdapter::getSlotHandle(ContainerKind kind,
                                             uint8_t container,
                                             uint16_t page,
                                             uint16_t index) const {
  if (m_service) {
    return m_service->getSlotHandle(
        SlotRef{kind, container, page, index});
  }
  return ItemHandle{0, 0};
}

} // namespace NoMoreDay
