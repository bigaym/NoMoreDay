#include "game/systems/item/storage/ItemStorageAdapter.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/systems/item/SharedStash.hpp"
#include "game/systems/item/storage/ItemStorageConverter.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
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
  }

  return s_fallbackAdapter.get();
}

void ItemStorageAdapter::SetDefaultAdapter(
    std::unique_ptr<IItemStorageAdapter> adapter) {
  s_customAdapter = std::move(adapter);
}

entt::entity *ItemStorageAdapter::getEntitySlot(entt::registry &reg,
                                                const SlotRef &slot) {
  entt::entity player = entt::null;
  const auto playerView = reg.view<PlayerTag>();
  for (auto e : playerView) {
    player = e;
    break;
  }

  switch (slot.kind) {
  case ContainerKind::Inventory: {
    if (player == entt::null)
      return nullptr;
    auto *inv = reg.try_get<InventoryComponent>(player);
    if (inv && slot.index < inv->items.size()) {
      return &inv->items[slot.index];
    }
    return nullptr;
  }
  case ContainerKind::Equipment: {
    if (player == entt::null)
      return nullptr;
    auto *equip = reg.try_get<EquipmentComponent>(player);
    const size_t idx = (slot.index < equip->slots.size())
                           ? static_cast<size_t>(slot.index)
                           : static_cast<size_t>(slot.container);
    if (equip && idx < equip->slots.size()) {
      return &equip->slots[idx];
    }
    return nullptr;
  }
  case ContainerKind::BagSlots: {
    if (player == entt::null)
      return nullptr;
    auto *inv = reg.try_get<InventoryComponent>(player);
    const size_t idx = (slot.index < inv->bag_slots.size())
                           ? static_cast<size_t>(slot.index)
                           : static_cast<size_t>(slot.container);
    if (inv && idx < inv->bag_slots.size()) {
      return &inv->bag_slots[idx];
    }
    return nullptr;
  }
  case ContainerKind::PersonalStash: {
    if (player == entt::null)
      return nullptr;
    auto *stash = reg.try_get<PersonalStashComponent>(player);
    if (stash && slot.page < stash->tabs.size() &&
        slot.index < stash->tabs[slot.page].items.size()) {
      return &stash->tabs[slot.page].items[slot.index];
    }
    return nullptr;
  }
  case ContainerKind::SharedStash: {
    StashTab *tab = SharedStash::Get().getTab(slot.page);
    if (tab && slot.index < StashTab::CAPACITY) {
      return &tab->items[slot.index];
    }
    return nullptr;
  }
  case ContainerKind::HeirloomVault:
  case ContainerKind::MaterialBank:
  case ContainerKind::GroundPending:
    return nullptr;
  }
  return nullptr;
}

const entt::entity *
ItemStorageAdapter::getEntitySlot(const entt::registry &reg,
                                  const SlotRef &slot) const {
  entt::entity player = entt::null;
  const auto playerView = reg.view<const PlayerTag>();
  for (auto e : playerView) {
    player = e;
    break;
  }

  switch (slot.kind) {
  case ContainerKind::Inventory: {
    if (player == entt::null)
      return nullptr;
    const auto *inv = reg.try_get<InventoryComponent>(player);
    if (inv && slot.index < inv->items.size()) {
      return &inv->items[slot.index];
    }
    return nullptr;
  }
  case ContainerKind::Equipment: {
    if (player == entt::null)
      return nullptr;
    const auto *equip = reg.try_get<EquipmentComponent>(player);
    const size_t idx = (slot.index < equip->slots.size())
                           ? static_cast<size_t>(slot.index)
                           : static_cast<size_t>(slot.container);
    if (equip && idx < equip->slots.size()) {
      return &equip->slots[idx];
    }
    return nullptr;
  }
  case ContainerKind::BagSlots: {
    if (player == entt::null)
      return nullptr;
    const auto *inv = reg.try_get<InventoryComponent>(player);
    const size_t idx = (slot.index < inv->bag_slots.size())
                           ? static_cast<size_t>(slot.index)
                           : static_cast<size_t>(slot.container);
    if (inv && idx < inv->bag_slots.size()) {
      return &inv->bag_slots[idx];
    }
    return nullptr;
  }
  case ContainerKind::PersonalStash: {
    if (player == entt::null)
      return nullptr;
    const auto *stash = reg.try_get<PersonalStashComponent>(player);
    if (stash && slot.page < stash->tabs.size() &&
        slot.index < stash->tabs[slot.page].items.size()) {
      return &stash->tabs[slot.page].items[slot.index];
    }
    return nullptr;
  }
  case ContainerKind::SharedStash: {
    const StashTab *tab = SharedStash::Get().getTab(slot.page);
    if (tab && slot.index < StashTab::CAPACITY) {
      return &tab->items[slot.index];
    }
    return nullptr;
  }
  case ContainerKind::HeirloomVault:
  case ContainerKind::MaterialBank:
  case ContainerKind::GroundPending:
    return nullptr;
  }
  return nullptr;
}

StorageError ItemStorageAdapter::moveItem(entt::registry &reg,
                                          const SlotRef &from,
                                          const SlotRef &to) {
  if (isItemStoreEnabled()) {
    return m_service->moveItem(from, to);
  }

  entt::entity *pFrom = getEntitySlot(reg, from);
  entt::entity *pTo = getEntitySlot(reg, to);
  if (!pFrom || !pTo) {
    return StorageError::InvalidSlot;
  }
  if (from == to) {
    return StorageError::Ok;
  }

  if (*pFrom == entt::null || !reg.valid(*pFrom)) {
    return StorageError::NotFound;
  }
  if (*pTo != entt::null && reg.valid(*pTo)) {
    return StorageError::ContainerFull;
  }

  *pTo = *pFrom;
  *pFrom = entt::null;
  return StorageError::Ok;
}

StorageError ItemStorageAdapter::swapItem(entt::registry &reg,
                                          const SlotRef &a, const SlotRef &b) {
  if (isItemStoreEnabled()) {
    return m_service->swapItem(a, b);
  }

  entt::entity *pA = getEntitySlot(reg, a);
  entt::entity *pB = getEntitySlot(reg, b);
  if (!pA || !pB) {
    return StorageError::InvalidSlot;
  }
  if (a == b) {
    return StorageError::Ok;
  }

  std::swap(*pA, *pB);
  return StorageError::Ok;
}

StorageError ItemStorageAdapter::splitStack(entt::registry &reg,
                                            const SlotRef &from,
                                            const SlotRef &to,
                                            uint32_t splitCount) {
  if (isItemStoreEnabled()) {
    return m_service->splitStack(from, to, splitCount);
  }

  entt::entity *pFrom = getEntitySlot(reg, from);
  entt::entity *pTo = getEntitySlot(reg, to);
  if (!pFrom || !pTo || from == to || splitCount == 0) {
    return StorageError::InvalidSlot;
  }

  if (*pFrom == entt::null || !reg.valid(*pFrom)) {
    return StorageError::NotFound;
  }
  if (*pTo != entt::null && reg.valid(*pTo)) {
    return StorageError::ContainerFull;
  }

  auto *itemComp = reg.try_get<ItemComponent>(*pFrom);
  if (!itemComp) {
    return StorageError::NotFound;
  }

  if (static_cast<uint32_t>(itemComp->quantity) <= splitCount) {
    return StorageError::CapacityExceeded;
  }

  auto newEntity = reg.create();
  ItemComponent newComp = *itemComp;
  newComp.quantity = static_cast<int>(splitCount);
  reg.emplace<ItemComponent>(newEntity, newComp);

  itemComp->quantity -= static_cast<int>(splitCount);
  *pTo = newEntity;
  return StorageError::Ok;
}

StorageError ItemStorageAdapter::mergeStack(entt::registry &reg,
                                            const SlotRef &from,
                                            const SlotRef &to) {
  if (isItemStoreEnabled()) {
    return m_service->mergeStack(from, to);
  }

  entt::entity *pFrom = getEntitySlot(reg, from);
  entt::entity *pTo = getEntitySlot(reg, to);
  if (!pFrom || !pTo || from == to) {
    return StorageError::InvalidSlot;
  }

  if (*pFrom == entt::null || !reg.valid(*pFrom) || *pTo == entt::null ||
      !reg.valid(*pTo)) {
    return StorageError::NotFound;
  }

  auto *fromComp = reg.try_get<ItemComponent>(*pFrom);
  auto *toComp = reg.try_get<ItemComponent>(*pTo);
  if (!fromComp || !toComp) {
    return StorageError::NotFound;
  }

  if (fromComp->id != toComp->id || fromComp->rarity != toComp->rarity) {
    return StorageError::TypeMismatch;
  }

  if (toComp->quantity >= toComp->maxStack) {
    return StorageError::CapacityExceeded;
  }

  const int space = toComp->maxStack - toComp->quantity;
  const int amount = std::min(fromComp->quantity, space);

  toComp->quantity += amount;
  fromComp->quantity -= amount;

  if (fromComp->quantity <= 0) {
    reg.destroy(*pFrom);
    *pFrom = entt::null;
  }

  return StorageError::Ok;
}

StorageError ItemStorageAdapter::transferItem(entt::registry &reg,
                                              const SlotRef &from,
                                              const SlotRef &to) {
  if (isItemStoreEnabled()) {
    return m_service->transferItem(from, to);
  }

  entt::entity *pFrom = getEntitySlot(reg, from);
  entt::entity *pTo = getEntitySlot(reg, to);
  if (!pFrom || !pTo) {
    return StorageError::InvalidSlot;
  }

  if (*pFrom == entt::null || !reg.valid(*pFrom)) {
    return StorageError::NotFound;
  }

  if (*pTo == entt::null || !reg.valid(*pTo)) {
    return moveItem(reg, from, to);
  }

  auto *fromComp = reg.try_get<ItemComponent>(*pFrom);
  auto *toComp = reg.try_get<ItemComponent>(*pTo);
  if (fromComp && toComp && fromComp->id == toComp->id &&
      fromComp->rarity == toComp->rarity && toComp->maxStack > 1) {
    const StorageError err = mergeStack(reg, from, to);
    if (err == StorageError::Ok) {
      return StorageError::Ok;
    }
  }

  return swapItem(reg, from, to);
}

StorageError ItemStorageAdapter::autoDeposit(entt::registry &reg,
                                             const SlotRef &from,
                                             ContainerKind targetKind,
                                             uint8_t targetContainer) {
  if (isItemStoreEnabled()) {
    return m_service->autoDeposit(from, targetKind, targetContainer);
  }

  entt::entity *pFrom = getEntitySlot(reg, from);
  if (!pFrom || *pFrom == entt::null || !reg.valid(*pFrom)) {
    return StorageError::NotFound;
  }

  const auto *itemComp = reg.try_get<ItemComponent>(*pFrom);
  if (!itemComp) {
    return StorageError::NotFound;
  }

  const size_t pages = (targetKind == ContainerKind::PersonalStash ||
                        targetKind == ContainerKind::SharedStash)
                           ? 10
                           : 1;
  const size_t slotsPerPage = (targetKind == ContainerKind::PersonalStash ||
                               targetKind == ContainerKind::SharedStash)
                                  ? StashTab::CAPACITY
                                  : InventoryComponent::BASE_CAPACITY;

  // 1. Try stack merge
  if (itemComp->maxStack > 1) {
    for (uint16_t p = 0; p < pages; ++p) {
      for (uint16_t i = 0; i < slotsPerPage; ++i) {
        SlotRef targetSlot{targetKind, targetContainer, p, i};
        entt::entity *pTarget = getEntitySlot(reg, targetSlot);
        if (pTarget && *pTarget != entt::null && reg.valid(*pTarget)) {
          const auto *targetComp = reg.try_get<ItemComponent>(*pTarget);
          if (targetComp && targetComp->id == itemComp->id &&
              targetComp->rarity == itemComp->rarity &&
              targetComp->quantity < targetComp->maxStack) {
            mergeStack(reg, from, targetSlot);
            if (*pFrom == entt::null) {
              return StorageError::Ok;
            }
          }
        }
      }
    }
  }

  // 2. Find empty slot
  for (uint16_t p = 0; p < pages; ++p) {
    for (uint16_t i = 0; i < slotsPerPage; ++i) {
      SlotRef targetSlot{targetKind, targetContainer, p, i};
      entt::entity *pTarget = getEntitySlot(reg, targetSlot);
      if (pTarget && *pTarget == entt::null) {
        return moveItem(reg, from, targetSlot);
      }
    }
  }

  return StorageError::ContainerFull;
}

void ItemStorageAdapter::sortContainer(entt::registry &reg, ContainerKind kind,
                                       uint8_t container, uint16_t page) {
  if (isItemStoreEnabled()) {
    m_service->sortContainer(kind, container, page);
    return;
  }

  (void)container;
  entt::entity player = entt::null;
  for (auto e : reg.view<PlayerTag>()) {
    player = e;
    break;
  }

  if (kind == ContainerKind::Inventory) {
    if (player != entt::null) {
      auto *inv = reg.try_get<InventoryComponent>(player);
      if (inv) {
        std::vector<entt::entity> items;
        for (auto e : inv->items) {
          if (e != entt::null && reg.valid(e)) {
            items.push_back(e);
          }
        }
        std::sort(items.begin(), items.end(),
                  [&reg](entt::entity a, entt::entity b) {
                    const auto *cA = reg.try_get<ItemComponent>(a);
                    const auto *cB = reg.try_get<ItemComponent>(b);
                    if (!cA && !cB)
                      return false;
                    if (!cA)
                      return false;
                    if (!cB)
                      return true;
                    if (cA->rarity != cB->rarity)
                      return cA->rarity > cB->rarity;
                    if (cA->type != cB->type)
                      return cA->type < cB->type;
                    if (cA->itemLevel != cB->itemLevel)
                      return cA->itemLevel > cB->itemLevel;
                    return cA->id < cB->id;
                  });
        inv->items.assign(inv->capacity, entt::null);
        for (size_t i = 0; i < items.size() && i < inv->items.size(); ++i) {
          inv->items[i] = items[i];
        }
      }
    }
  } else if (kind == ContainerKind::PersonalStash) {
    if (player != entt::null) {
      auto *stash = reg.try_get<PersonalStashComponent>(player);
      if (stash && page < stash->tabs.size()) {
        auto &tab = stash->tabs[page];
        std::vector<entt::entity> items;
        for (auto e : tab.items) {
          if (e != entt::null && reg.valid(e)) {
            items.push_back(e);
          }
        }
        std::sort(items.begin(), items.end(),
                  [&reg](entt::entity a, entt::entity b) {
                    const auto *cA = reg.try_get<ItemComponent>(a);
                    const auto *cB = reg.try_get<ItemComponent>(b);
                    if (!cA && !cB)
                      return false;
                    if (!cA)
                      return false;
                    if (!cB)
                      return true;
                    if (cA->rarity != cB->rarity)
                      return cA->rarity > cB->rarity;
                    return cA->id < cB->id;
                  });
        tab.items.fill(entt::null);
        for (size_t i = 0; i < items.size() && i < tab.items.size(); ++i) {
          tab.items[i] = items[i];
        }
      }
    }
  }
}

bool ItemStorageAdapter::canStoreItem(ContainerKind kind, uint8_t container,
                                      uint16_t page,
                                      ItemHandle handle) const {
  if (isItemStoreEnabled()) {
    return m_service->canStoreItem(kind, container, page, handle);
  }
  return true;
}

ItemHandle ItemStorageAdapter::getSlotHandle(ContainerKind kind,
                                             uint8_t container,
                                             uint16_t page,
                                             uint16_t index) const {
  if (isItemStoreEnabled()) {
    return m_service->getSlotHandle(
        SlotRef{kind, container, page, index});
  }
  return ItemHandle{0, 0};
}

} // namespace NoMoreDay
