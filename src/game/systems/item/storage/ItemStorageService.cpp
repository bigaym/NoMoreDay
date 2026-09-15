#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <algorithm>
#include <chrono>

namespace NoMoreDay {

namespace {
// 缺少 maxStack 模板的物品的回退堆叠上限；镜像旧版 MaterialRegistry JSON 默认值 ("max_stack")。
constexpr uint32_t kDefaultMaxStack = 9999;
} // namespace

ItemStorageService::ItemStorageService() {
  m_inventorySlots.assign(kInventoryCapacity, ItemHandle{0, 0});
  m_equipmentSlots.assign(kEquipmentCapacity, ItemHandle{0, 0});
  m_bagSlots.assign(kBagSlotsCapacity, ItemHandle{0, 0});

  m_personalStash.resize(kPersonalStashMaxPages);
  for (auto &page : m_personalStash) {
    page.assign(kStashPageCapacity, ItemHandle{0, 0});
  }
  m_personalStashMeta.resize(kPersonalStashMaxPages);
  for (size_t i = 0; i < kPersonalStashMaxPages; ++i) {
    m_personalStashMeta[i].name = "Stash " + std::to_string(i + 1);
  }

  m_sharedStash.resize(kSharedStashMaxPages);
  for (auto &page : m_sharedStash) {
    page.assign(kStashPageCapacity, ItemHandle{0, 0});
  }
  m_sharedStashMeta.resize(kSharedStashMaxPages);
  for (size_t i = 0; i < kSharedStashMaxPages; ++i) {
    m_sharedStashMeta[i].name = "Shared " + std::to_string(i + 1);
  }

  m_heirloomVault.assign(kHeirloomVaultCapacity, ItemHandle{0, 0});
}

uint64_t ItemStorageService::generateInstanceId() noexcept {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  const auto micros =
      std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  return static_cast<uint64_t>(micros) + (m_nextInstanceId++);
}

ItemHandle *ItemStorageService::getSlotPointer(const SlotRef &slot) {
  switch (slot.kind) {
  case ContainerKind::Inventory:
    if (slot.index < m_inventorySlots.size()) {
      return &m_inventorySlots[slot.index];
    }
    return nullptr;
  case ContainerKind::Equipment: {
    // 约定: Equipment 通过 SlotRef::container (EquipmentSlot 值) 寻址；index 必须为 0。参见 SlotRef::isWellFormed()。
    assert(slot.isWellFormed() &&
           "Equipment slots are addressed via SlotRef::container; index must be 0");
    const size_t idx = static_cast<size_t>(slot.container);
    if (idx < m_equipmentSlots.size()) {
      return &m_equipmentSlots[idx];
    }
    return nullptr;
  }
  case ContainerKind::BagSlots: {
    // 约定: BagSlots 通过 SlotRef::container (背包索引) 寻址。
    assert(slot.isWellFormed() &&
           "BagSlots are addressed via SlotRef::container; index must be 0");
    const size_t idx = static_cast<size_t>(slot.container);
    if (idx < m_bagSlots.size()) {
      return &m_bagSlots[idx];
    }
    return nullptr;
  }
  case ContainerKind::PersonalStash:
    if (slot.page < m_personalStash.size() &&
        slot.index < m_personalStash[slot.page].size()) {
      return &m_personalStash[slot.page][slot.index];
    }
    return nullptr;
  case ContainerKind::SharedStash:
    if (slot.page < m_sharedStash.size() &&
        slot.index < m_sharedStash[slot.page].size()) {
      return &m_sharedStash[slot.page][slot.index];
    }
    return nullptr;
  case ContainerKind::HeirloomVault:
    if (slot.index < m_heirloomVault.size()) {
      return &m_heirloomVault[slot.index];
    }
    return nullptr;
  case ContainerKind::MaterialBank:
    return nullptr;
  }
  return nullptr;
}

const ItemHandle *ItemStorageService::getSlotPointer(const SlotRef &slot) const {
  switch (slot.kind) {
  case ContainerKind::Inventory:
    if (slot.index < m_inventorySlots.size()) {
      return &m_inventorySlots[slot.index];
    }
    return nullptr;
  case ContainerKind::Equipment: {
    // 约定: Equipment 通过 SlotRef::container (EquipmentSlot 值) 寻址；index 必须为 0。参见 SlotRef::isWellFormed()。
    assert(slot.isWellFormed() &&
           "Equipment slots are addressed via SlotRef::container; index must be 0");
    const size_t idx = static_cast<size_t>(slot.container);
    if (idx < m_equipmentSlots.size()) {
      return &m_equipmentSlots[idx];
    }
    return nullptr;
  }
  case ContainerKind::BagSlots: {
    // 约定: BagSlots 通过 SlotRef::container (背包索引) 寻址。
    assert(slot.isWellFormed() &&
           "BagSlots are addressed via SlotRef::container; index must be 0");
    const size_t idx = static_cast<size_t>(slot.container);
    if (idx < m_bagSlots.size()) {
      return &m_bagSlots[idx];
    }
    return nullptr;
  }
  case ContainerKind::PersonalStash:
    if (slot.page < m_personalStash.size() &&
        slot.index < m_personalStash[slot.page].size()) {
      return &m_personalStash[slot.page][slot.index];
    }
    return nullptr;
  case ContainerKind::SharedStash:
    if (slot.page < m_sharedStash.size() &&
        slot.index < m_sharedStash[slot.page].size()) {
      return &m_sharedStash[slot.page][slot.index];
    }
    return nullptr;
  case ContainerKind::HeirloomVault:
    if (slot.index < m_heirloomVault.size()) {
      return &m_heirloomVault[slot.index];
    }
    return nullptr;
  case ContainerKind::MaterialBank:
    return nullptr;
  }
  return nullptr;
}

bool ItemStorageService::canStoreItem(ContainerKind kind, uint8_t container,
                                      uint16_t page, ItemHandle handle) const {
  // 防御性契约: 调用者在询问是否能放入之前必须先解析出有效句柄；
  // 对无效句柄返回 "true" 会掩盖 bug。
  if (!handle || !m_store.isValid(handle)) {
    return false;
  }

  const ItemInstance *inst = m_store.get(handle);
  if (!inst) {
    return false;
  }

  const ItemTemplate *tpl = ItemTemplateRegistry::Instance().find(inst->baseId);

  switch (kind) {
  case ContainerKind::Inventory:
    return true;

  case ContainerKind::Equipment: {
    const size_t slotIdx = static_cast<size_t>(container);
    if (slotIdx >= kEquipmentCapacity) {
      return false;
    }
    if (!tpl) {
      return true;
    }
    const EquipmentSlot reqSlot = static_cast<EquipmentSlot>(slotIdx);
    if (reqSlot == EquipmentSlot::MainHand) {
      return tpl->slot == EquipmentSlot::MainHand ||
             tpl->slot == EquipmentSlot::None ||
             tpl->type == ItemType::Weapon;
    }
    if (reqSlot == EquipmentSlot::OffHand) {
      return tpl->slot == EquipmentSlot::OffHand ||
             tpl->type == ItemType::Shield ||
             tpl->type == ItemType::Weapon;
    }
    if (reqSlot == EquipmentSlot::Ring1 || reqSlot == EquipmentSlot::Ring2) {
      return tpl->slot == EquipmentSlot::Ring ||
             tpl->slot == EquipmentSlot::Ring1 ||
             tpl->slot == EquipmentSlot::Ring2 ||
             tpl->type == ItemType::Jewelry;
    }
    return tpl->slot == reqSlot || tpl->slot == EquipmentSlot::None;
  }

  case ContainerKind::BagSlots:
    if (tpl) {
      return tpl->type == ItemType::Bag;
    }
    return true;

  case ContainerKind::PersonalStash:
    return page < m_personalStashUnlockedPages;

  case ContainerKind::SharedStash:
    return page < m_sharedStashUnlockedPages;

  case ContainerKind::HeirloomVault:
    if (tpl && (tpl->type == ItemType::Quest ||
                tpl->type == ItemType::Material)) {
      return false;
    }
    return true;

  case ContainerKind::MaterialBank:
    if (tpl) {
      return tpl->type == ItemType::Material;
    }
    return true;
  }

  return true;
}

StorageError ItemStorageService::moveItem(const SlotRef &from,
                                          const SlotRef &to) {
  ItemHandle *pFrom = getSlotPointer(from);
  ItemHandle *pTo = getSlotPointer(to);
  if (!pFrom || !pTo) {
    return StorageError::InvalidSlot;
  }
  if (from == to) {
    return StorageError::Ok;
  }

  const ItemHandle hFrom = *pFrom;
  if (!m_store.isValid(hFrom)) {
    return StorageError::NotFound;
  }

  const ItemHandle hTo = *pTo;
  if (m_store.isValid(hTo)) {
    return StorageError::ContainerFull;
  }

  if (!canStoreItem(to.kind, to.container, to.page, hFrom)) {
    return StorageError::TypeMismatch;
  }

  *pTo = hFrom;
  *pFrom = ItemHandle{0, 0};

  m_store.mutate(hFrom, [](ItemInstance &) {});
  return StorageError::Ok;
}

StorageError ItemStorageService::swapItem(const SlotRef &a, const SlotRef &b) {
  ItemHandle *pA = getSlotPointer(a);
  ItemHandle *pB = getSlotPointer(b);
  if (!pA || !pB) {
    return StorageError::InvalidSlot;
  }
  if (a == b) {
    return StorageError::Ok;
  }

  const ItemHandle hA = *pA;
  const ItemHandle hB = *pB;

  if (m_store.isValid(hA) &&
      !canStoreItem(b.kind, b.container, b.page, hA)) {
    return StorageError::TypeMismatch;
  }
  if (m_store.isValid(hB) &&
      !canStoreItem(a.kind, a.container, a.page, hB)) {
    return StorageError::TypeMismatch;
  }

  *pA = hB;
  *pB = hA;

  if (m_store.isValid(hA)) {
    m_store.mutate(hA, [](ItemInstance &) {});
  }
  if (m_store.isValid(hB)) {
    m_store.mutate(hB, [](ItemInstance &) {});
  }
  return StorageError::Ok;
}

StorageError ItemStorageService::splitStack(const SlotRef &from,
                                            const SlotRef &to,
                                            uint32_t splitCount) {
  ItemHandle *pFrom = getSlotPointer(from);
  ItemHandle *pTo = getSlotPointer(to);
  if (!pFrom || !pTo || from == to || splitCount == 0) {
    return StorageError::InvalidSlot;
  }

  const ItemHandle hFrom = *pFrom;
  const ItemInstance *instFrom = m_store.get(hFrom);
  if (!instFrom) {
    return StorageError::NotFound;
  }

  // 锁定的堆叠无法拆分 (镜像 destroyItem)。
  if (instFrom->isLocked()) {
    return StorageError::Locked;
  }

  if (instFrom->quantity <= splitCount) {
    return StorageError::CapacityExceeded;
  }

  const ItemHandle hTo = *pTo;
  if (m_store.isValid(hTo)) {
    return StorageError::ContainerFull;
  }

  if (!canStoreItem(to.kind, to.container, to.page, hFrom)) {
    return StorageError::TypeMismatch;
  }

  ItemInstance proto = *instFrom;
  proto.instanceId = generateInstanceId();
  proto.quantity = splitCount;
  proto.sockets.fill(ItemHandle{0, 0});
  proto.socketCount = 0;

  const ItemHandle hNew = m_store.create(proto);
  if (const auto *side = m_store.getSideTable(hFrom)) {
    m_store.setSideTable(hNew, *side);
  }

  m_store.mutate(hFrom, [splitCount](ItemInstance &i) {
    i.quantity -= splitCount;
  });

  *pTo = hNew;
  return StorageError::Ok;
}

StorageError ItemStorageService::mergeStack(const SlotRef &from,
                                            const SlotRef &to) {
  ItemHandle *pFrom = getSlotPointer(from);
  ItemHandle *pTo = getSlotPointer(to);
  if (!pFrom || !pTo || from == to) {
    return StorageError::InvalidSlot;
  }

  const ItemHandle hFrom = *pFrom;
  const ItemHandle hTo = *pTo;
  const ItemInstance *instFrom = m_store.get(hFrom);
  const ItemInstance *instTo = m_store.get(hTo);
  if (!instFrom || !instTo) {
    return StorageError::NotFound;
  }

  // M2: 全字段与词缀/旁表差异比对，防止实例差异被静默丢弃
  if (instFrom->baseId != instTo->baseId ||
      instFrom->rarity != instTo->rarity ||
      instFrom->itemLevel != instTo->itemLevel ||
      instFrom->flags != instTo->flags ||
      instFrom->affixCount != instTo->affixCount ||
      instFrom->affixes != instTo->affixes ||
      instFrom->socketCount != instTo->socketCount ||
      instFrom->sockets != instTo->sockets ||
      instFrom->activeRunewordId != instTo->activeRunewordId) {
    return StorageError::TypeMismatch;
  }

  const auto *sideFrom = m_store.getSideTable(hFrom);
  const auto *sideTo = m_store.getSideTable(hTo);
  const bool sideFromEmpty = !sideFrom || sideFrom->empty();
  const bool sideToEmpty = !sideTo || sideTo->empty();
  if (!sideFromEmpty || !sideToEmpty) {
    if (sideFromEmpty != sideToEmpty) return StorageError::TypeMismatch;
    // 统一走 ItemSideTableData::operator!=，确保全部旁表字段
    // (含 modifier_record_ids) 参与比较，未来新增字段不会被遗漏。
    if (*sideFrom != *sideTo) {
      return StorageError::TypeMismatch;
    }
  }

  // 保持锁定物品规则与 destroyItem/splitStack 的对称性。
  if (instFrom->isLocked() || instTo->isLocked()) {
    return StorageError::Locked;
  }

  uint32_t maxStack = kDefaultMaxStack;
  if (const auto *tpl = ItemTemplateRegistry::Instance().find(instTo->baseId)) {
    if (tpl->maxStack > 0) {
      maxStack = tpl->maxStack;
    }
  }

  if (instTo->quantity >= maxStack) {
    return StorageError::CapacityExceeded;
  }

  const uint32_t space = maxStack - instTo->quantity;
  const uint32_t amount = std::min(instFrom->quantity, space);

  m_store.mutate(hTo, [amount](ItemInstance &i) { i.quantity += amount; });
  m_store.mutate(hFrom, [amount](ItemInstance &i) { i.quantity -= amount; });

  if (m_store.get(hFrom)->quantity == 0) {
    m_store.destroy(hFrom);
    *pFrom = ItemHandle{0, 0};
  }

  return StorageError::Ok;
}

StorageError ItemStorageService::transferItem(const SlotRef &from,
                                              const SlotRef &to) {
  ItemHandle *pFrom = getSlotPointer(from);
  ItemHandle *pTo = getSlotPointer(to);
  if (!pFrom || !pTo) {
    return StorageError::InvalidSlot;
  }

  const ItemHandle hFrom = *pFrom;
  if (!m_store.isValid(hFrom)) {
    return StorageError::NotFound;
  }

  const ItemHandle hTo = *pTo;
  if (!m_store.isValid(hTo)) {
    return moveItem(from, to);
  }

  const ItemInstance *instFrom = m_store.get(hFrom);
  const ItemInstance *instTo = m_store.get(hTo);
  if (instFrom && instTo && instFrom->baseId == instTo->baseId &&
      instFrom->rarity == instTo->rarity) {
    const StorageError err = mergeStack(from, to);
    if (err == StorageError::Ok) {
      return StorageError::Ok;
    }
  }

  return swapItem(from, to);
}

StorageError ItemStorageService::autoDeposit(const SlotRef &from,
                                             ContainerKind targetKind,
                                             uint8_t targetContainer) {
  // L1: 显式拒绝不支持槽位寻址的 targetKind
  if (targetKind != ContainerKind::Inventory &&
      targetKind != ContainerKind::PersonalStash &&
      targetKind != ContainerKind::SharedStash) {
    return StorageError::TypeMismatch;
  }

  ItemHandle *pFrom = getSlotPointer(from);
  if (!pFrom) {
    return StorageError::InvalidSlot;
  }
  const ItemHandle hFrom = *pFrom;
  const ItemInstance *inst = m_store.get(hFrom);
  if (!inst) {
    return StorageError::NotFound;
  }

  // 1. 尝试堆叠合并
  const uint16_t pages = getUnlockedPages(targetKind);
  for (uint16_t p = 0; p < pages; ++p) {
    const size_t cap =
        (targetKind == ContainerKind::PersonalStash ||
         targetKind == ContainerKind::SharedStash)
            ? kStashPageCapacity
            : kInventoryCapacity;
    for (uint16_t i = 0; i < cap; ++i) {
      const SlotRef targetSlot{targetKind, targetContainer, p, i};
      const ItemHandle hTarget = getSlotHandle(targetSlot);
      if (hTarget) {
        const ItemInstance *instTarget = m_store.get(hTarget);
        if (instTarget && instTarget->baseId == inst->baseId &&
            instTarget->rarity == inst->rarity) {
          mergeStack(from, targetSlot);
          if (!m_store.isValid(*pFrom)) {
            return StorageError::Ok;
          }
        }
      }
    }
  }

  // 2. 寻找空槽位
  for (uint16_t p = 0; p < pages; ++p) {
    const size_t cap =
        (targetKind == ContainerKind::PersonalStash ||
         targetKind == ContainerKind::SharedStash)
            ? kStashPageCapacity
            : kInventoryCapacity;
    for (uint16_t i = 0; i < cap; ++i) {
      const SlotRef targetSlot{targetKind, targetContainer, p, i};
      const ItemHandle hTarget = getSlotHandle(targetSlot);
      if (!hTarget &&
          canStoreItem(targetKind, targetContainer, p, *pFrom)) {
        return moveItem(from, targetSlot);
      }
    }
  }

  return StorageError::ContainerFull;
}

void ItemStorageService::sortContainer(ContainerKind kind, uint8_t container,
                                       uint16_t page,
                                       SlotFilterPredicate filter) {
  std::vector<ItemHandle> *slotArray = nullptr;
  if (kind == ContainerKind::Inventory) {
    slotArray = &m_inventorySlots;
  } else if (kind == ContainerKind::PersonalStash &&
             page < m_personalStash.size()) {
    slotArray = &m_personalStash[page];
  } else if (kind == ContainerKind::SharedStash &&
             page < m_sharedStash.size()) {
    slotArray = &m_sharedStash[page];
  } else if (kind == ContainerKind::HeirloomVault) {
    slotArray = &m_heirloomVault;
  }

  if (!slotArray || slotArray->empty()) {
    return;
  }

  std::vector<ItemHandle> validHandles;
  std::vector<size_t> participatingIndices;
  validHandles.reserve(slotArray->size());
  participatingIndices.reserve(slotArray->size());

  for (size_t i = 0; i < slotArray->size(); ++i) {
    ItemHandle h = (*slotArray)[i];
    SlotRef sRef{kind, container, page, static_cast<uint16_t>(i)};
    if (filter && !filter(sRef, h)) {
      continue;
    }
    participatingIndices.push_back(i);
    if (m_store.isValid(h)) {
      validHandles.push_back(h);
    }
  }

  std::sort(validHandles.begin(), validHandles.end(),
            [this](ItemHandle hA, ItemHandle hB) {
              const ItemInstance *a = m_store.get(hA);
              const ItemInstance *b = m_store.get(hB);
              if (!a && !b)
                return false;
              if (!a)
                return false;
              if (!b)
                return true;

              if (a->rarity != b->rarity) {
                return a->rarity > b->rarity;
              }

              const auto *tA =
                  ItemTemplateRegistry::Instance().find(a->baseId);
              const auto *tB =
                  ItemTemplateRegistry::Instance().find(b->baseId);
              const uint8_t typeA =
                  tA ? static_cast<uint8_t>(tA->type) : 0;
              const uint8_t typeB =
                  tB ? static_cast<uint8_t>(tB->type) : 0;
              if (typeA != typeB) {
                return typeA < typeB;
              }
              if (a->itemLevel != b->itemLevel) {
                return a->itemLevel > b->itemLevel;
              }
              if (a->baseId != b->baseId) {
                return a->baseId < b->baseId;
              }
              return a->quantity > b->quantity;
            });

  for (size_t k = 0; k < participatingIndices.size(); ++k) {
    size_t slotIdx = participatingIndices[k];
    if (k < validHandles.size()) {
      (*slotArray)[slotIdx] = validHandles[k];
    } else {
      (*slotArray)[slotIdx] = ItemHandle{0, 0};
    }
  }

  if (!validHandles.empty()) {
    m_store.mutate(validHandles[0], [](ItemInstance &) {});
  }
}

StorageError ItemStorageService::destroyItem(const SlotRef &slot,
                                             int quantity) {
  ItemHandle *pSlot = getSlotPointer(slot);
  if (!pSlot) {
    return StorageError::InvalidSlot;
  }
  const ItemHandle h = *pSlot;
  const ItemInstance *inst = m_store.get(h);
  if (!inst) {
    return StorageError::NotFound;
  }

  if (inst->isLocked()) {
    return StorageError::Locked;
  }

  if (quantity <= 0 ||
      quantity >= static_cast<int>(inst->quantity)) {
    m_store.destroy(h);
    *pSlot = ItemHandle{0, 0};
  } else {
    m_store.mutate(h, [quantity](ItemInstance &i) {
      i.quantity -= static_cast<uint32_t>(quantity);
    });
  }
  return StorageError::Ok;
}

ItemHandle ItemStorageService::getSlotHandle(const SlotRef &slot) const {
  const ItemHandle *p = getSlotPointer(slot);
  return p ? *p : ItemHandle{0, 0};
}

bool ItemStorageService::setSlotHandle(const SlotRef &slot, ItemHandle handle) {
  ItemHandle *p = getSlotPointer(slot);
  if (!p) {
    return false;
  }
  *p = handle;
  return true;
}

std::vector<ItemHandle>
ItemStorageService::getContainerSlots(ContainerKind kind, uint8_t container,
                                      uint16_t page) const {
  (void)container;
  switch (kind) {
  case ContainerKind::Inventory:
    return m_inventorySlots;
  case ContainerKind::Equipment:
    return m_equipmentSlots;
  case ContainerKind::BagSlots:
    return m_bagSlots;
  case ContainerKind::PersonalStash:
    if (page < m_personalStash.size()) {
      return m_personalStash[page];
    }
    break;
  case ContainerKind::SharedStash:
    if (page < m_sharedStash.size()) {
      return m_sharedStash[page];
    }
    break;
  case ContainerKind::HeirloomVault:
    return m_heirloomVault;
  case ContainerKind::MaterialBank:
    break;
  }
  return {};
}

int32_t ItemStorageService::addMaterial(uint32_t id, int32_t amount) {
  if (amount <= 0) {
    return getMaterialCount(id);
  }
  auto it = std::lower_bound(
      m_materialBank.begin(), m_materialBank.end(),
      std::make_pair(id, 0),
      [](const auto &a, const auto &b) { return a.first < b.first; });

  if (it != m_materialBank.end() && it->first == id) {
    it->second += amount;
    return it->second;
  }
  m_materialBank.insert(it, std::make_pair(id, amount));
  return amount;
}

bool ItemStorageService::removeMaterial(uint32_t id, int32_t amount) {
  if (amount <= 0) {
    return true;
  }
  auto it = std::lower_bound(
      m_materialBank.begin(), m_materialBank.end(),
      std::make_pair(id, 0),
      [](const auto &a, const auto &b) { return a.first < b.first; });

  if (it != m_materialBank.end() && it->first == id) {
    if (it->second >= amount) {
      it->second -= amount;
      if (it->second == 0) {
        m_materialBank.erase(it);
      }
      return true;
    }
  }
  return false;
}

int32_t ItemStorageService::getMaterialCount(uint32_t id) const {
  auto it = std::lower_bound(
      m_materialBank.begin(), m_materialBank.end(),
      std::make_pair(id, 0),
      [](const auto &a, const auto &b) { return a.first < b.first; });

  if (it != m_materialBank.end() && it->first == id) {
    return it->second;
  }
  return 0;
}

bool ItemStorageService::hasMaterial(uint32_t id, int32_t amount) const {
  return getMaterialCount(id) >= amount;
}

void ItemStorageService::setMaterials(
    std::vector<std::pair<uint32_t, int32_t>> materials) {
  m_materialBank = std::move(materials);
  std::sort(m_materialBank.begin(), m_materialBank.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
}

uint16_t ItemStorageService::getUnlockedPages(ContainerKind kind) const noexcept {
  if (kind == ContainerKind::PersonalStash) {
    return m_personalStashUnlockedPages;
  }
  if (kind == ContainerKind::SharedStash) {
    return m_sharedStashUnlockedPages;
  }
  return 1;
}

bool ItemStorageService::unlockPage(ContainerKind kind) {
  if (kind == ContainerKind::PersonalStash) {
    if (m_personalStashUnlockedPages < kPersonalStashMaxPages) {
      m_personalStashUnlockedPages++;
      return true;
    }
  } else if (kind == ContainerKind::SharedStash) {
    if (m_sharedStashUnlockedPages < kSharedStashMaxPages) {
      m_sharedStashUnlockedPages++;
      return true;
    }
  }
  return false;
}

void ItemStorageService::setUnlockedPages(ContainerKind kind,
                                          uint16_t pages) noexcept {
  if (kind == ContainerKind::PersonalStash) {
    m_personalStashUnlockedPages =
        std::clamp<uint16_t>(pages, 1, kPersonalStashMaxPages);
  } else if (kind == ContainerKind::SharedStash) {
    m_sharedStashUnlockedPages =
        std::clamp<uint16_t>(pages, 1, kSharedStashMaxPages);
  }
}

void ItemStorageService::clearContainer(ContainerKind kind, uint8_t container,
                                        uint16_t page) {
  (void)container;
  switch (kind) {
  case ContainerKind::Inventory:
    m_inventorySlots.assign(kInventoryCapacity, ItemHandle{0, 0});
    break;
  case ContainerKind::Equipment:
    m_equipmentSlots.assign(kEquipmentCapacity, ItemHandle{0, 0});
    break;
  case ContainerKind::BagSlots:
    m_bagSlots.assign(kBagSlotsCapacity, ItemHandle{0, 0});
    break;
  case ContainerKind::PersonalStash:
    if (page < m_personalStash.size()) {
      m_personalStash[page].assign(kStashPageCapacity, ItemHandle{0, 0});
    }
    break;
  case ContainerKind::SharedStash:
    if (page < m_sharedStash.size()) {
      m_sharedStash[page].assign(kStashPageCapacity, ItemHandle{0, 0});
    }
    break;
  case ContainerKind::HeirloomVault:
    m_heirloomVault.assign(kHeirloomVaultCapacity, ItemHandle{0, 0});
    break;
  case ContainerKind::MaterialBank:
    m_materialBank.clear();
    break;
  }
}

void ItemStorageService::clearAll() {
  m_inventorySlots.assign(kInventoryCapacity, ItemHandle{0, 0});
  m_equipmentSlots.assign(kEquipmentCapacity, ItemHandle{0, 0});
  m_bagSlots.assign(kBagSlotsCapacity, ItemHandle{0, 0});

  for (auto &page : m_personalStash) {
    page.assign(kStashPageCapacity, ItemHandle{0, 0});
  }
  for (auto &page : m_sharedStash) {
    page.assign(kStashPageCapacity, ItemHandle{0, 0});
  }
  m_heirloomVault.assign(kHeirloomVaultCapacity, ItemHandle{0, 0});
  m_materialBank.clear();
  m_store.clear();
}

} // namespace NoMoreDay
