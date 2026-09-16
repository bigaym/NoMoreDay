#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "core/logging/Logger.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace NoMoreDay {

namespace {

// 单个物品旁表允许持久化的 UMR 记录 ID 上限。
// 单件装备的 UMR 记录数量级很小（通常 1~数个），64 已为极端情况留足余量；
// 与相邻 convCount/dmgCount 的固定上限语义一致，避免 CRC 可重算的构造载荷
// 用整段字节数作上限把 reserve() 推到接近段大小。
constexpr uint32_t kMaxModifierRecordIdsPerItem = 64;

// 单个物品旁表允许持久化的属性转换 / 伤害修正条数上限，
// 与解码侧 convCount/dmgCount 的固定上限保持一致，避免编码写入解码必然拒绝的数据。
constexpr uint32_t kMaxConversionsPerItem = 100;
constexpr uint32_t kMaxDamageModifiersPerItem = 100;

// 单个物品旁表允许持久化的技能修饰器条数上限，
// 与解码侧 modCount 的固定上限保持一致，避免编码写入解码必然拒绝的数据。
constexpr uint32_t kMaxSkillModifiersPerItem = 100;

// 辅助向 vector<uint8_t> 中追加任意 POD 字节
template <typename T>
void appendBytes(std::vector<uint8_t> &dest, const T &val) {
  static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
  const auto *ptr = reinterpret_cast<const uint8_t *>(&val);
  dest.insert(dest.end(), ptr, ptr + sizeof(T));
}

void appendRaw(std::vector<uint8_t> &dest, const void *data, size_t size) {
  if (data && size > 0) {
    const auto *ptr = static_cast<const uint8_t *>(data);
    dest.insert(dest.end(), ptr, ptr + size);
  }
}

// 辅助从内存流读取 POD 字节
template <typename T>
[[nodiscard]] bool readBytes(const uint8_t *&ptr, const uint8_t *end, T &outVal) {
  static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
  if (ptr + sizeof(T) > end) {
    return false;
  }
  std::memcpy(&outVal, ptr, sizeof(T));
  ptr += sizeof(T);
  return true;
}

[[nodiscard]] bool readRaw(const uint8_t *&ptr, const uint8_t *end, void *outData, size_t size) {
  if (ptr + size > end) {
    return false;
  }
  std::memcpy(outData, ptr, size);
  ptr += size;
  return true;
}

// 收集当前编码容器所引用的全部可达实例索引（含嵌套插槽物品）(N2)
std::unordered_set<uint32_t> collectReachableIndices(const ItemStorageService &service, uint32_t dirtyMask) {
  std::unordered_set<uint32_t> reachable;
  auto addHandle = [&](ItemHandle h) {
    if (!h || !service.getStore().isValid(h)) return;
    std::vector<ItemHandle> stack;
    stack.push_back(h);
    while (!stack.empty()) {
      ItemHandle curr = stack.back();
      stack.pop_back();
      if (reachable.insert(curr.index).second) {
        if (const auto *inst = service.getStore().get(curr)) {
          for (ItemHandle sockH : inst->sockets) {
            if (sockH && service.getStore().isValid(sockH)) {
              stack.push_back(sockH);
            }
          }
        }
      }
    }
  };

  if (dirtyMask & ContainerDirtyFlags::Inventory) {
    for (ItemHandle h : service.getInventorySlots()) addHandle(h);
  }
  if (dirtyMask & ContainerDirtyFlags::Equipment) {
    for (ItemHandle h : service.getEquipmentSlots()) addHandle(h);
  }
  if (dirtyMask & ContainerDirtyFlags::BagSlots) {
    for (ItemHandle h : service.getBagSlots()) addHandle(h);
  }
  if (dirtyMask & ContainerDirtyFlags::PersonalStash) {
    for (const auto &page : service.getPersonalStashPages()) {
      for (ItemHandle h : page) addHandle(h);
    }
  }
  if (dirtyMask & ContainerDirtyFlags::SharedStash) {
    for (const auto &page : service.getSharedStashPages()) {
      for (ItemHandle h : page) addHandle(h);
    }
  }
  if (dirtyMask & ContainerDirtyFlags::HeirloomVault) {
    for (ItemHandle h : service.getHeirloomVaultSlots()) addHandle(h);
  }
  return reachable;
}

// 序列化 Section 1: TemplateFingerprint (L3: 纳入模板关键属性哈希)
uint64_t calculateTemplateFingerprint() {
  uint64_t fingerprint = 0x1469598103934665ULL; // FNV offset basis
  const auto &templates = ItemTemplateRegistry::Get().getAllTemplates();
  if (templates.empty()) {
    LOG_WARN("calculateTemplateFingerprint: ItemTemplateRegistry is empty!");
  }
  for (const auto &[baseId, tmpl] : templates) {
    fingerprint ^= static_cast<uint64_t>(baseId);
    fingerprint *= 1099511628211ULL; // FNV prime
    fingerprint ^= (static_cast<uint64_t>(tmpl.type) << 16) ^ static_cast<uint64_t>(tmpl.slot);
    fingerprint *= 1099511628211ULL;
    fingerprint ^= (static_cast<uint64_t>(tmpl.minLevel) << 8) ^ static_cast<uint64_t>(tmpl.maxStack);
    fingerprint *= 1099511628211ULL;
  }
  return fingerprint;
}

std::vector<uint8_t> buildTemplateFingerprintSection() {
  std::vector<uint8_t> payload;
  const uint64_t fingerprint = calculateTemplateFingerprint();
  appendBytes(payload, fingerprint);
  return payload;
}

// 序列化 Section 2: ItemInstances (N2: 按需过滤可达实例)
std::vector<uint8_t> buildItemInstancesSection(const ItemStorageService &service,
                                              const std::unordered_set<uint32_t> *allowedIndices) {
  std::vector<uint8_t> payload;
  const auto &store = service.getStore();

  std::vector<std::pair<ItemHandle, const ItemInstance *>> itemsToEncode;
  itemsToEncode.reserve(store.activeCount());

  store.visit([&](ItemHandle h, const ItemInstance &inst) {
    if (!allowedIndices || allowedIndices->contains(h.index)) {
      itemsToEncode.emplace_back(h, &inst);
    }
  });

  const uint32_t count = static_cast<uint32_t>(itemsToEncode.size());
  appendBytes(payload, count);

  for (const auto &[h, inst] : itemsToEncode) {
    appendBytes(payload, h.index);
    appendBytes(payload, h.gen);
    appendBytes(payload, *inst);
  }

  return payload;
}

// 序列化 Section 3: ItemSideTables (N2: 按需过滤可达旁表)
std::vector<uint8_t> buildItemSideTablesSection(const ItemStorageService &service,
                                               const std::unordered_set<uint32_t> *allowedIndices) {
  std::vector<uint8_t> payload;
  const auto &sideTables = service.getStore().getAllSideTables();

  std::vector<std::pair<uint32_t, const ItemSideTableData *>> sidesToEncode;
  sidesToEncode.reserve(sideTables.size());

  for (const auto &[idx, data] : sideTables) {
    if (!allowedIndices || allowedIndices->contains(idx)) {
      sidesToEncode.emplace_back(idx, &data);
    }
  }

  const uint32_t count = static_cast<uint32_t>(sidesToEncode.size());
  appendBytes(payload, count);

  for (const auto &[idx, pData] : sidesToEncode) {
    const auto &data = *pData;
    appendBytes(payload, idx);
    
    // conversions
    const uint32_t convCount = static_cast<uint32_t>(data.conversions.size());
    appendBytes(payload, convCount);
    for (const auto &conv : data.conversions) {
      appendBytes(payload, static_cast<uint32_t>(conv.source));
      appendBytes(payload, static_cast<uint32_t>(conv.target));
      appendBytes(payload, conv.ratio);
      appendBytes(payload, static_cast<uint64_t>(conv.required_tags));
    }

    // damage_modifiers
    const uint32_t dmgCount = static_cast<uint32_t>(data.damage_modifiers.size());
    appendBytes(payload, dmgCount);
    for (const auto &dm : data.damage_modifiers) {
      appendBytes(payload, static_cast<uint64_t>(dm.source_tag));
      appendBytes(payload, static_cast<uint64_t>(dm.target_tag));
      appendBytes(payload, dm.value);
      appendBytes(payload, static_cast<uint32_t>(dm.type));
    }

    // modifier_record_ids
    const uint32_t recordIdCount =
        static_cast<uint32_t>(data.modifier_record_ids.size());
    appendBytes(payload, recordIdCount);
    for (const uint32_t recordId : data.modifier_record_ids) {
      appendBytes(payload, recordId);
    }
  }

  return payload;
}

// 序列化 Section 13: ItemSkillModifiers (装备技能修饰器独立持久化分段)
std::vector<uint8_t> buildItemSkillModifiersSection(const ItemStorageService &service,
                                                   const std::unordered_set<uint32_t> *allowedIndices) {
  std::vector<uint8_t> payload;
  const auto &sideTables = service.getStore().getAllSideTables();

  std::vector<std::pair<uint32_t, const ItemSideTableData *>> sidesToEncode;
  sidesToEncode.reserve(sideTables.size());

  for (const auto &[idx, data] : sideTables) {
    if ((!allowedIndices || allowedIndices->contains(idx)) && !data.skill_modifiers.empty()) {
      sidesToEncode.emplace_back(idx, &data);
    }
  }

  const uint32_t count = static_cast<uint32_t>(sidesToEncode.size());
  appendBytes(payload, count);

  for (const auto &[idx, pData] : sidesToEncode) {
    const auto &data = *pData;
    appendBytes(payload, idx);
    const uint32_t modCount = static_cast<uint32_t>(data.skill_modifiers.size());
    appendBytes(payload, modCount);
    for (const auto &mod : data.skill_modifiers) {
      appendBytes(payload, mod.target_skill_id);
      appendBytes(payload, mod.flat_cooldown_delta);
      appendBytes(payload, mod.mana_cost_delta);
      appendBytes(payload, static_cast<int32_t>(mod.extra_projectiles));
      appendBytes(payload, mod.area_radius_mult);
      appendBytes(payload, static_cast<uint64_t>(mod.convert_from));
      appendBytes(payload, static_cast<uint64_t>(mod.convert_to));
      appendBytes(payload, mod.conversion_ratio);
      appendBytes(payload, mod.inject_ailment_id);
      appendBytes(payload, mod.inject_ailment_chance);
    }
  }

  return payload;
}

// 序列化 Section 4: Inventory
std::vector<uint8_t> buildInventorySection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  const auto &slots = service.getInventorySlots();
  const uint32_t capacity = static_cast<uint32_t>(slots.size());
  appendBytes(payload, capacity);

  uint32_t occupied = 0;
  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i]) {
      occupied++;
    }
  }
  appendBytes(payload, occupied);

  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i]) {
      appendBytes(payload, static_cast<uint16_t>(i));
      appendBytes(payload, slots[i]);
    }
  }

  return payload;
}

// 序列化 Section 5: Equipment
std::vector<uint8_t> buildEquipmentSection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  const auto &slots = service.getEquipmentSlots();

  uint32_t occupied = 0;
  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i]) {
      occupied++;
    }
  }
  appendBytes(payload, occupied);

  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i]) {
      appendBytes(payload, static_cast<uint8_t>(i));
      appendBytes(payload, slots[i]);
    }
  }

  return payload;
}

// 序列化 Section 6: BagSlots
std::vector<uint8_t> buildBagSlotsSection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  const auto &slots = service.getBagSlots();

  uint32_t occupied = 0;
  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i]) {
      occupied++;
    }
  }
  appendBytes(payload, occupied);

  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i]) {
      appendBytes(payload, static_cast<uint8_t>(i));
      appendBytes(payload, slots[i]);
    }
  }

  return payload;
}

// 序列化 Section 7: PersonalStash
std::vector<uint8_t> buildPersonalStashSection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  const uint16_t unlockedPages = service.getUnlockedPages(ContainerKind::PersonalStash);
  appendBytes(payload, unlockedPages);

  const auto &metaList = service.getPersonalStashMeta();
  for (size_t p = 0; p < unlockedPages; ++p) {
    const StashTabMeta &meta = (p < metaList.size()) ? metaList[p] : StashTabMeta{};
    const uint16_t nameLen = static_cast<uint16_t>(meta.name.size());
    appendBytes(payload, nameLen);
    appendRaw(payload, meta.name.data(), nameLen);
    appendBytes(payload, meta.type);
    appendBytes(payload, meta.iconId);
    appendBytes(payload, meta.color);
  }

  const auto &pages = service.getPersonalStashPages();
  uint32_t occupied = 0;
  for (size_t p = 0; p < pages.size() && p < unlockedPages; ++p) {
    for (size_t s = 0; s < pages[p].size(); ++s) {
      if (pages[p][s]) {
        occupied++;
      }
    }
  }
  appendBytes(payload, occupied);

  for (size_t p = 0; p < pages.size() && p < unlockedPages; ++p) {
    for (size_t s = 0; s < pages[p].size(); ++s) {
      if (pages[p][s]) {
        appendBytes(payload, static_cast<uint16_t>(p));
        appendBytes(payload, static_cast<uint16_t>(s));
        appendBytes(payload, pages[p][s]);
      }
    }
  }

  return payload;
}

// 序列化 Section 8: SharedStash
std::vector<uint8_t> buildSharedStashSection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  const uint16_t unlockedPages = service.getUnlockedPages(ContainerKind::SharedStash);
  appendBytes(payload, unlockedPages);

  const auto &metaList = service.getSharedStashMeta();
  for (size_t p = 0; p < unlockedPages; ++p) {
    const StashTabMeta &meta = (p < metaList.size()) ? metaList[p] : StashTabMeta{};
    const uint16_t nameLen = static_cast<uint16_t>(meta.name.size());
    appendBytes(payload, nameLen);
    appendRaw(payload, meta.name.data(), nameLen);
    appendBytes(payload, meta.type);
    appendBytes(payload, meta.iconId);
    appendBytes(payload, meta.color);
  }

  const auto &pages = service.getSharedStashPages();
  uint32_t occupied = 0;
  for (size_t p = 0; p < pages.size() && p < unlockedPages; ++p) {
    for (size_t s = 0; s < pages[p].size(); ++s) {
      if (pages[p][s]) {
        occupied++;
      }
    }
  }
  appendBytes(payload, occupied);

  for (size_t p = 0; p < pages.size() && p < unlockedPages; ++p) {
    for (size_t s = 0; s < pages[p].size(); ++s) {
      if (pages[p][s]) {
        appendBytes(payload, static_cast<uint16_t>(p));
        appendBytes(payload, static_cast<uint16_t>(s));
        appendBytes(payload, pages[p][s]);
      }
    }
  }

  return payload;
}

// 序列化 Section 9: HeirloomVault
std::vector<uint8_t> buildHeirloomVaultSection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  const auto &slots = service.getHeirloomVaultSlots();

  uint32_t occupied = 0;
  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i]) {
      occupied++;
    }
  }
  appendBytes(payload, occupied);

  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i]) {
      appendBytes(payload, static_cast<uint16_t>(i));
      appendBytes(payload, slots[i]);
    }
  }

  return payload;
}

// 序列化 Section 10: MaterialBank
std::vector<uint8_t> buildMaterialBankSection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  const auto &mats = service.getMaterials();
  const uint32_t count = static_cast<uint32_t>(mats.size());
  appendBytes(payload, count);

  for (const auto &[id, amount] : mats) {
    appendBytes(payload, id);
    appendBytes(payload, amount);
  }

  return payload;
}

// 序列化 Section 11: EconomyMetadata
std::vector<uint8_t> buildEconomyMetadataSection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  appendBytes(payload, static_cast<int32_t>(service.getGold()));
  appendBytes(payload, static_cast<uint64_t>(service.getNextInstanceId()));
  return payload;
}

// 序列化 Section 12: ProgressionData
std::vector<uint8_t> buildProgressionDataSection(const std::string &progressionPayload) {
  std::vector<uint8_t> payload;
  const uint32_t len = static_cast<uint32_t>(progressionPayload.size());
  appendBytes(payload, len);
  if (len > 0) {
    appendRaw(payload, progressionPayload.data(), len);
  }
  return payload;
}

} // namespace

bool ItemPersistenceCodec::encode(const ItemStorageService &service,
                                  std::ostream &outStream,
                                  InMemorySectionCache *cache,
                                  uint32_t dirtyMask,
                                  const std::string &progressionPayload) {
  if (!outStream.good()) {
    return false;
  }

  // 待编码的各分段数据
  struct SectionEntry {
    SectionType type;
    std::vector<uint8_t> payload;
    uint32_t crc32 = 0;
  };

  std::vector<SectionEntry> sections;
  sections.reserve(12);

  auto processSection = [&](SectionType type, uint32_t flagBit, auto &&builderFn) {
    const bool isDirty = (dirtyMask & flagBit) != 0;
    if (!isDirty && cache && cache->contains(type)) {
      const auto *cached = cache->get(type);
      sections.push_back({type, cached->payload, cached->crc32});
    } else if (isDirty) {
      std::vector<uint8_t> payload = builderFn();
      uint32_t crc = calculateCrc32(payload.data(), payload.size());
      if (cache) {
        cache->put(type, payload, crc);
      }
      sections.push_back({type, std::move(payload), crc});
    }
  };

  constexpr uint32_t kAnyContainerMask = ContainerDirtyFlags::Inventory |
                                         ContainerDirtyFlags::Equipment |
                                         ContainerDirtyFlags::BagSlots |
                                         ContainerDirtyFlags::PersonalStash |
                                         ContainerDirtyFlags::SharedStash |
                                         ContainerDirtyFlags::HeirloomVault;
  std::optional<std::unordered_set<uint32_t>> reachableIndices;
  if ((dirtyMask & kAnyContainerMask) != 0) {
    reachableIndices = collectReachableIndices(service, dirtyMask);
  }
  const std::unordered_set<uint32_t> *allowedPtr = reachableIndices ? &*reachableIndices : nullptr;

  // 编码前对本次会写出的旁表做上限防御：坏数据在此 fail-closed，
  // 避免在 section 构建期抛错造成空段与静默丢档。
  // 旁表拆分在两个 section 中写出（ItemSideTables 与 ItemSkillModifiers），
  // 因此按各自的脏标记独立门控：命中缓存的段在首次构建时已校验过，
  // 未脏写的轻量存盘不应被本次不会写出的旁表卡死。
  const bool checkSideTables = (dirtyMask & ContainerDirtyFlags::ItemSideTables) != 0;
  const bool checkSkillModifiers = (dirtyMask & ContainerDirtyFlags::ItemSkillModifiers) != 0;
  if (checkSideTables || checkSkillModifiers) {
    for (const auto &[idx, data] : service.getStore().getAllSideTables()) {
      if (allowedPtr != nullptr && !allowedPtr->contains(idx)) {
        continue;
      }
      if (checkSideTables &&
          (data.modifier_record_ids.size() > kMaxModifierRecordIdsPerItem ||
           data.conversions.size() > kMaxConversionsPerItem ||
           data.damage_modifiers.size() > kMaxDamageModifiersPerItem)) {
        LOG_ERROR(
            "ItemPersistenceCodec: side-table {} exceeds persistence limits "
            "(modifier_record_ids={}/{}, conversions={}/{}, "
            "damage_modifiers={}/{})",
            idx, data.modifier_record_ids.size(), kMaxModifierRecordIdsPerItem,
            data.conversions.size(), kMaxConversionsPerItem,
            data.damage_modifiers.size(), kMaxDamageModifiersPerItem);
        return false;
      }
      if (checkSkillModifiers && data.skill_modifiers.size() > kMaxSkillModifiersPerItem) {
        LOG_ERROR("ItemPersistenceCodec: side-table {} exceeds persistence limits "
                  "(skill_modifiers={}/{})",
                  idx, data.skill_modifiers.size(), kMaxSkillModifiersPerItem);
        return false;
      }
    }
  }

  processSection(SectionType::TemplateFingerprint, ContainerDirtyFlags::TemplateFingerprint,
                 [&]() { return buildTemplateFingerprintSection(); });
  processSection(SectionType::ItemInstances, ContainerDirtyFlags::ItemInstances,
                 [&]() { return buildItemInstancesSection(service, allowedPtr); });
  processSection(SectionType::ItemSideTables, ContainerDirtyFlags::ItemSideTables,
                 [&]() { return buildItemSideTablesSection(service, allowedPtr); });
  processSection(SectionType::ItemSkillModifiers, ContainerDirtyFlags::ItemSkillModifiers,
                 [&]() { return buildItemSkillModifiersSection(service, allowedPtr); });
  processSection(SectionType::Inventory, ContainerDirtyFlags::Inventory,
                 [&]() { return buildInventorySection(service); });
  processSection(SectionType::Equipment, ContainerDirtyFlags::Equipment,
                 [&]() { return buildEquipmentSection(service); });
  processSection(SectionType::BagSlots, ContainerDirtyFlags::BagSlots,
                 [&]() { return buildBagSlotsSection(service); });
  processSection(SectionType::PersonalStash, ContainerDirtyFlags::PersonalStash,
                 [&]() { return buildPersonalStashSection(service); });
  processSection(SectionType::SharedStash, ContainerDirtyFlags::SharedStash,
                 [&]() { return buildSharedStashSection(service); });
  processSection(SectionType::HeirloomVault, ContainerDirtyFlags::HeirloomVault,
                 [&]() { return buildHeirloomVaultSection(service); });
  processSection(SectionType::MaterialBank, ContainerDirtyFlags::MaterialBank,
                 [&]() { return buildMaterialBankSection(service); });
  processSection(SectionType::EconomyMetadata, ContainerDirtyFlags::EconomyMetadata,
                 [&]() { return buildEconomyMetadataSection(service); });

  if (!progressionPayload.empty() || (dirtyMask & ContainerDirtyFlags::ProgressionData)) {
    processSection(SectionType::ProgressionData, ContainerDirtyFlags::ProgressionData,
                   [&]() { return buildProgressionDataSection(progressionPayload); });
  }

  // 构建 SectionHeader 数组与计算 Header CRC32
  const uint32_t sectionCount = static_cast<uint32_t>(sections.size());
  std::vector<SectionHeader> headers(sectionCount);

  uint32_t currentOffset = sizeof(FileHeader) + sizeof(SectionHeader) * sectionCount;
  for (size_t i = 0; i < sections.size(); ++i) {
    headers[i].type = static_cast<uint32_t>(sections[i].type);
    headers[i].offset = currentOffset;
    headers[i].length = static_cast<uint32_t>(sections[i].payload.size());
    headers[i].crc32 = sections[i].crc32;
    currentOffset += headers[i].length;
  }

  FileHeader fileHeader;
  fileHeader.magic = ITEM_STORE_MAGIC;
  fileHeader.version = ITEM_STORE_BINARY_VERSION;
  fileHeader.sectionCount = sectionCount;
  fileHeader.headerCrc32 = calculateCrc32(
      headers.data(), headers.size() * sizeof(SectionHeader));

  // 流式写出
  outStream.write(reinterpret_cast<const char *>(&fileHeader), sizeof(FileHeader));
  outStream.write(reinterpret_cast<const char *>(headers.data()),
                  headers.size() * sizeof(SectionHeader));

  for (const auto &sec : sections) {
    if (!sec.payload.empty()) {
      outStream.write(reinterpret_cast<const char *>(sec.payload.data()),
                      sec.payload.size());
    }
  }

  return outStream.good();
}

bool ItemPersistenceCodec::decode(std::istream &inStream,
                                  ItemStorageService &service,
                                  std::string *outProgressionPayload) {
  if (!inStream.good()) {
    return false;
  }

  // 0. 流尺寸预检，防止非法空流或过小流 (H1)
  inStream.seekg(0, std::ios::end);
  const std::streampos endPos = inStream.tellg();
  inStream.seekg(0, std::ios::beg);
  if (endPos < static_cast<std::streampos>(sizeof(FileHeader))) {
    // fmt 12 不再支持隐式格式化 std::streampos，日志输出转为整数偏移
  LOG_ERROR("ItemPersistenceCodec: Stream size {} below minimum FileHeader size", static_cast<std::streamoff>(endPos));
    return false;
  }
  const uint64_t totalStreamBytes = static_cast<uint64_t>(endPos);

  // 1. 读取并校验 FileHeader (§6.1: FileHeader 为 16 字节 pack(1) POD)
  FileHeader fileHeader;
  inStream.read(reinterpret_cast<char *>(&fileHeader), sizeof(FileHeader));
  if (!inStream.good()) {
    return false;
  }

  if (fileHeader.magic != ITEM_STORE_MAGIC) {
    LOG_ERROR("ItemPersistenceCodec: Invalid magic number 0x{:08X}", fileHeader.magic);
    return false;
  }

  if (fileHeader.version != ITEM_STORE_BINARY_VERSION) {
    LOG_ERROR("ItemPersistenceCodec: Unsupported binary version {}", fileHeader.version);
    return false;
  }

  if (fileHeader.sectionCount == 0 || fileHeader.sectionCount > 64) {
    LOG_ERROR("ItemPersistenceCodec: Invalid section count {}", fileHeader.sectionCount);
    return false;
  }

  const uint64_t headersByteSize = sizeof(SectionHeader) * fileHeader.sectionCount;
  if (sizeof(FileHeader) + headersByteSize > totalStreamBytes) {
    LOG_ERROR("ItemPersistenceCodec: Section headers table exceeds total file stream size");
    return false;
  }

  // 2. 读取并校验 SectionHeader 表 (§6.1: SectionHeader 表为紧凑 POD 数组)
  std::vector<SectionHeader> headers(fileHeader.sectionCount);
  inStream.read(reinterpret_cast<char *>(headers.data()),
                headers.size() * sizeof(SectionHeader));
  if (!inStream.good()) {
    return false;
  }

  const uint32_t computedHeaderCrc = calculateCrc32(
      headers.data(), headers.size() * sizeof(SectionHeader));
  if (computedHeaderCrc != fileHeader.headerCrc32) {
    LOG_ERROR("ItemPersistenceCodec: Header CRC32 mismatch (computed: 0x{:08X}, expected: 0x{:08X})",
              computedHeaderCrc, fileHeader.headerCrc32);
    return false;
  }

  // 阶段 1: 读取所有 Section 数据并进行 CRC32 事务性预校验 (H1: 分配前检查 length 上界)
  std::vector<std::vector<uint8_t>> sectionData(headers.size());
  for (size_t i = 0; i < headers.size(); ++i) {
    if (headers[i].offset > totalStreamBytes ||
        headers[i].length > totalStreamBytes ||
        headers[i].offset + headers[i].length > totalStreamBytes) {
      LOG_ERROR("ItemPersistenceCodec: Section {} bounds [offset {}, len {}] exceed file stream size {}",
                headers[i].type, headers[i].offset, headers[i].length, totalStreamBytes);
      return false;
    }

    sectionData[i].resize(headers[i].length);
    if (headers[i].length > 0) {
      inStream.seekg(headers[i].offset, std::ios::beg);
      // §6.1: 直接将二进制 Payload 流读入 vector 字节缓冲区
      inStream.read(reinterpret_cast<char *>(sectionData[i].data()), headers[i].length);
      if (!inStream.good()) {
        LOG_ERROR("ItemPersistenceCodec: Failed reading section {}", headers[i].type);
        return false;
      }
    }

    const uint32_t computedCrc = calculateCrc32(
        sectionData[i].data(), sectionData[i].size());
    if (computedCrc != headers[i].crc32) {
      LOG_ERROR("ItemPersistenceCodec: CRC32 mismatch for section {} (computed: 0x{:08X}, expected: 0x{:08X})",
                headers[i].type, computedCrc, headers[i].crc32);
      return false;
    }
  }

  // 阶段 2: 全量校验通过，安全解析各分段内容
  std::vector<ItemStore::RawInstanceEntry> rawEntries;
  std::unordered_map<uint32_t, ItemSideTableData> sideTables;
  std::vector<std::pair<uint16_t, ItemHandle>> invSlots;
  std::vector<std::pair<uint8_t, ItemHandle>> eqSlots;
  std::vector<std::pair<uint8_t, ItemHandle>> bagSlots;
  std::vector<std::tuple<uint16_t, uint16_t, ItemHandle>> personalSlots;
  std::vector<std::tuple<uint16_t, uint16_t, ItemHandle>> sharedSlots;
  std::vector<StashTabMeta> personalMeta;
  std::vector<StashTabMeta> sharedMeta;
  std::vector<std::pair<uint16_t, ItemHandle>> heirloomSlots;
  std::vector<std::pair<uint32_t, int32_t>> materials;
  uint16_t personalUnlocked = 1;
  uint16_t sharedUnlocked = 1;
  int32_t loadedGold = 0;
  uint64_t loadedNextInstanceId = 0;

  for (size_t i = 0; i < headers.size(); ++i) {
    const auto &bytes = sectionData[i];
    const uint8_t *ptr = bytes.data();
    const uint8_t *end = ptr + bytes.size();
    const auto type = static_cast<SectionType>(headers[i].type);

    switch (type) {
    case SectionType::TemplateFingerprint: {
      uint64_t fileFingerprint = 0;
      if (!readBytes(ptr, end, fileFingerprint)) {
        return false;
      }
      if (ItemTemplateRegistry::Get().count() > 0) {
        uint64_t expectedFingerprint = calculateTemplateFingerprint();
        if (fileFingerprint != expectedFingerprint) {
          LOG_WARN("ItemPersistenceCodec: Template fingerprint mismatch (file: 0x{:X}, expected: 0x{:X})",
                   fileFingerprint, expectedFingerprint);
          return false;
        }
      }
      break;
    }
    case SectionType::ItemInstances: {
      uint32_t activeCount = 0;
      if (!readBytes(ptr, end, activeCount)) {
        return false;
      }
      // H1: activeCount 预算检查，防止构造畸形大数字触发 OOM
      constexpr size_t kMinEntrySize = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(ItemInstance);
      if (activeCount > (bytes.size() / kMinEntrySize) + 1 || activeCount > 1000000) {
        LOG_ERROR("ItemPersistenceCodec: Declared activeCount {} exceeds section payload budget", activeCount);
        return false;
      }
      rawEntries.reserve(activeCount);
      for (uint32_t k = 0; k < activeCount; ++k) {
        ItemStore::RawInstanceEntry entry;
        if (!readBytes(ptr, end, entry.index) ||
            !readBytes(ptr, end, entry.gen) ||
            !readBytes(ptr, end, entry.instance)) {
          LOG_ERROR("ItemPersistenceCodec: Truncated ItemInstances section");
          return false;
        }
        rawEntries.push_back(entry);
      }
      break;
    }
    case SectionType::ItemSideTables: {
      uint32_t count = 0;
      if (!readBytes(ptr, end, count)) {
        return false;
      }
      if (count > (bytes.size() / 8) + 1 || count > 1000000) {
        LOG_ERROR("ItemPersistenceCodec: Declared sideTables count {} exceeds section budget", count);
        return false;
      }
      for (uint32_t k = 0; k < count; ++k) {
        uint32_t idx = 0;
        if (!readBytes(ptr, end, idx)) return false;
        ItemSideTableData data;

        uint32_t convCount = 0;
        if (!readBytes(ptr, end, convCount)) return false;
        if (convCount > kMaxConversionsPerItem) return false;
        for (uint32_t c = 0; c < convCount; ++c) {
          uint32_t src = 0, tgt = 0;
          float ratio = 0.0f;
          uint64_t tags = 0;
          if (!readBytes(ptr, end, src) || !readBytes(ptr, end, tgt) ||
              !readBytes(ptr, end, ratio) || !readBytes(ptr, end, tags)) {
            return false;
          }
          data.conversions.push_back({static_cast<StatType>(src),
                                      static_cast<StatType>(tgt), ratio,
                                      static_cast<Tag>(tags)});
        }

        uint32_t dmgCount = 0;
        if (!readBytes(ptr, end, dmgCount)) return false;
        if (dmgCount > kMaxDamageModifiersPerItem) return false;
        for (uint32_t d = 0; d < dmgCount; ++d) {
          uint64_t srcTag = 0, tgtTag = 0;
          float val = 0.0f;
          uint32_t modType = 0;
          if (!readBytes(ptr, end, srcTag) || !readBytes(ptr, end, tgtTag) ||
              !readBytes(ptr, end, val) || !readBytes(ptr, end, modType)) {
            return false;
          }
          data.damage_modifiers.push_back({static_cast<Tag>(srcTag),
                                           static_cast<Tag>(tgtTag), val,
                                           static_cast<ModifierType>(modType)});
        }
        uint32_t recordIdCount = 0;
        if (!readBytes(ptr, end, recordIdCount)) return false;
        if (recordIdCount > kMaxModifierRecordIdsPerItem) return false;
        std::vector<uint32_t> recordIds;
        recordIds.reserve(recordIdCount);
        for (uint32_t r = 0; r < recordIdCount; ++r) {
          uint32_t recordId = 0;
          if (!readBytes(ptr, end, recordId)) return false;
          recordIds.push_back(recordId);
        }

        auto &targetData = sideTables[idx];
        targetData.conversions = std::move(data.conversions);
        targetData.damage_modifiers = std::move(data.damage_modifiers);
        targetData.modifier_record_ids = std::move(recordIds);
      }
      break;
    }
    case SectionType::Inventory: {
      uint32_t capacity = 0, occupied = 0;
      if (!readBytes(ptr, end, capacity) || !readBytes(ptr, end, occupied)) {
        return false;
      }
      if (capacity > 1000 || occupied > capacity || occupied > (bytes.size() / 6) + 1) {
        LOG_ERROR("ItemPersistenceCodec: Inventory occupied {} exceeds capacity {}", occupied, capacity);
        return false;
      }
      for (uint32_t k = 0; k < occupied; ++k) {
        uint16_t slot = 0;
        ItemHandle h;
        if (!readBytes(ptr, end, slot) || !readBytes(ptr, end, h)) {
          return false;
        }
        invSlots.push_back({slot, h});
      }
      break;
    }
    case SectionType::Equipment: {
      uint32_t occupied = 0;
      if (!readBytes(ptr, end, occupied)) {
        return false;
      }
      if (occupied > ItemStorageService::kEquipmentCapacity) {
        LOG_ERROR("ItemPersistenceCodec: Equipment occupied {} exceeds limit", occupied);
        return false;
      }
      for (uint32_t k = 0; k < occupied; ++k) {
        uint8_t slot = 0;
        ItemHandle h;
        if (!readBytes(ptr, end, slot) || !readBytes(ptr, end, h)) {
          return false;
        }
        eqSlots.push_back({slot, h});
      }
      break;
    }
    case SectionType::BagSlots: {
      uint32_t occupied = 0;
      if (!readBytes(ptr, end, occupied)) {
        return false;
      }
      if (occupied > ItemStorageService::kBagSlotsCapacity) {
        LOG_ERROR("ItemPersistenceCodec: BagSlots occupied {} exceeds limit", occupied);
        return false;
      }
      for (uint32_t k = 0; k < occupied; ++k) {
        uint8_t slot = 0;
        ItemHandle h;
        if (!readBytes(ptr, end, slot) || !readBytes(ptr, end, h)) {
          return false;
        }
        bagSlots.push_back({slot, h});
      }
      break;
    }
    case SectionType::PersonalStash: {
      uint32_t occupied = 0;
      if (!readBytes(ptr, end, personalUnlocked)) {
        return false;
      }
      if (personalUnlocked > ItemStorageService::kPersonalStashMaxPages) {
        LOG_ERROR("ItemPersistenceCodec: PersonalStash unlocked pages {} exceeds max", personalUnlocked);
        return false;
      }
      personalMeta.resize(personalUnlocked);
      for (uint16_t p = 0; p < personalUnlocked; ++p) {
        uint16_t nameLen = 0;
        if (!readBytes(ptr, end, nameLen)) return false;
        if (nameLen > 256) return false;
        std::string name(nameLen, '\0');
        if (nameLen > 0 && !readRaw(ptr, end, name.data(), nameLen)) return false;
        uint8_t t = 0;
        uint32_t icon = 0, col = 0xFFFFFFFF;
        if (!readBytes(ptr, end, t) || !readBytes(ptr, end, icon) || !readBytes(ptr, end, col)) return false;
        personalMeta[p] = StashTabMeta{std::move(name), t, icon, col};
      }
      if (!readBytes(ptr, end, occupied)) {
        return false;
      }
      if (occupied > personalUnlocked * ItemStorageService::kStashPageCapacity) {
        LOG_ERROR("ItemPersistenceCodec: PersonalStash occupied {} exceeds capacity", occupied);
        return false;
      }
      for (uint32_t k = 0; k < occupied; ++k) {
        uint16_t page = 0, slot = 0;
        ItemHandle h;
        if (!readBytes(ptr, end, page) || !readBytes(ptr, end, slot) ||
            !readBytes(ptr, end, h)) {
          return false;
        }
        personalSlots.push_back({page, slot, h});
      }
      break;
    }
    case SectionType::SharedStash: {
      uint32_t occupied = 0;
      if (!readBytes(ptr, end, sharedUnlocked)) {
        return false;
      }
      if (sharedUnlocked > ItemStorageService::kSharedStashMaxPages) {
        LOG_ERROR("ItemPersistenceCodec: SharedStash unlocked pages {} exceeds max", sharedUnlocked);
        return false;
      }
      sharedMeta.resize(sharedUnlocked);
      for (uint16_t p = 0; p < sharedUnlocked; ++p) {
        uint16_t nameLen = 0;
        if (!readBytes(ptr, end, nameLen)) return false;
        if (nameLen > 256) return false;
        std::string name(nameLen, '\0');
        if (nameLen > 0 && !readRaw(ptr, end, name.data(), nameLen)) return false;
        uint8_t t = 0;
        uint32_t icon = 0, col = 0xFFFFFFFF;
        if (!readBytes(ptr, end, t) || !readBytes(ptr, end, icon) || !readBytes(ptr, end, col)) return false;
        sharedMeta[p] = StashTabMeta{std::move(name), t, icon, col};
      }
      if (!readBytes(ptr, end, occupied)) {
        return false;
      }
      if (occupied > sharedUnlocked * ItemStorageService::kStashPageCapacity) {
        LOG_ERROR("ItemPersistenceCodec: SharedStash occupied {} exceeds capacity", occupied);
        return false;
      }
      for (uint32_t k = 0; k < occupied; ++k) {
        uint16_t page = 0, slot = 0;
        ItemHandle h;
        if (!readBytes(ptr, end, page) || !readBytes(ptr, end, slot) ||
            !readBytes(ptr, end, h)) {
          return false;
        }
        sharedSlots.push_back({page, slot, h});
      }
      break;
    }
    case SectionType::HeirloomVault: {
      uint32_t occupied = 0;
      if (!readBytes(ptr, end, occupied)) {
        return false;
      }
      if (occupied > ItemStorageService::kHeirloomVaultCapacity) {
        LOG_ERROR("ItemPersistenceCodec: HeirloomVault occupied {} exceeds limit", occupied);
        return false;
      }
      for (uint32_t k = 0; k < occupied; ++k) {
        uint16_t slot = 0;
        ItemHandle h;
        if (!readBytes(ptr, end, slot) || !readBytes(ptr, end, h)) {
          return false;
        }
        heirloomSlots.push_back({slot, h});
      }
      break;
    }
    case SectionType::MaterialBank: {
      uint32_t count = 0;
      if (!readBytes(ptr, end, count)) {
        return false;
      }
      if (count > (bytes.size() / 8) + 1 || count > 10000) {
        LOG_ERROR("ItemPersistenceCodec: MaterialBank count {} exceeds budget", count);
        return false;
      }
      for (uint32_t k = 0; k < count; ++k) {
        uint32_t id = 0;
        int32_t amount = 0;
        if (!readBytes(ptr, end, id) || !readBytes(ptr, end, amount)) {
          return false;
        }
        materials.push_back({id, amount});
      }
      break;
    }
    case SectionType::EconomyMetadata: {
      uint64_t nextInstId = 0;
      if (!readBytes(ptr, end, loadedGold) || !readBytes(ptr, end, nextInstId)) {
        return false;
      }
      loadedNextInstanceId = nextInstId;
      break;
    }
    case SectionType::ProgressionData: {
      uint32_t len = 0;
      if (!readBytes(ptr, end, len)) {
        return false;
      }
      if (len > 0) {
        if (ptr + len > end) {
          return false;
        }
        if (outProgressionPayload) {
          outProgressionPayload->assign(reinterpret_cast<const char *>(ptr), len);
        }
        ptr += len;
      }
      break;
    }
    case SectionType::ItemSkillModifiers: {
      uint32_t count = 0;
      if (!readBytes(ptr, end, count)) {
        return false;
      }
      if (count > (bytes.size() / 8) + 1 || count > 1000000) {
        LOG_ERROR("ItemPersistenceCodec: Declared item skill modifiers count {} exceeds section budget", count);
        return false;
      }
      for (uint32_t k = 0; k < count; ++k) {
        uint32_t idx = 0;
        if (!readBytes(ptr, end, idx)) return false;
        uint32_t modCount = 0;
        if (!readBytes(ptr, end, modCount)) return false;
        if (modCount > kMaxSkillModifiersPerItem) return false;
        std::vector<ItemSkillModifier> mods;
        mods.reserve(modCount);
        for (uint32_t m = 0; m < modCount; ++m) {
          ItemSkillModifier mod{};
          uint64_t convFrom = 0, convTo = 0;
          int32_t extraProj = 0;
          if (!readBytes(ptr, end, mod.target_skill_id) ||
              !readBytes(ptr, end, mod.flat_cooldown_delta) ||
              !readBytes(ptr, end, mod.mana_cost_delta) ||
              !readBytes(ptr, end, extraProj) ||
              !readBytes(ptr, end, mod.area_radius_mult) ||
              !readBytes(ptr, end, convFrom) ||
              !readBytes(ptr, end, convTo) ||
              !readBytes(ptr, end, mod.conversion_ratio) ||
              !readBytes(ptr, end, mod.inject_ailment_id) ||
              !readBytes(ptr, end, mod.inject_ailment_chance)) {
            return false;
          }
          mod.extra_projectiles = extraProj;
          mod.convert_from = static_cast<Tag>(convFrom);
          mod.convert_to = static_cast<Tag>(convTo);
          mods.push_back(mod);
        }
        sideTables[idx].skill_modifiers = std::move(mods);
      }
      break;
    }
    default:
      break;
    }
  }

  // 阶段 3: 将所有解析结果灌入目标 service (H2: 句柄与插槽全面有效性校验 Pass)
  service.clearAll();
  service.getStoreMutable().restoreRawEntries(rawEntries, sideTables);

  // 校验嵌套 sockets 存在性与有效性
  service.getStoreMutable().visit([&](ItemHandle h, const ItemInstance &inst) {
    (void)h;
    for (size_t s = 0; s < inst.sockets.size(); ++s) {
      ItemHandle sockH = inst.sockets[s];
      if (sockH && !service.getStore().isValid(sockH)) {
        LOG_WARN("ItemPersistenceCodec: Item instance index {} contains invalid socket handle {{{}, {}}}, clearing",
                 h.index, sockH.index, sockH.gen);
        service.getStoreMutable().mutate(h, [s](ItemInstance &i) {
          i.sockets[s] = ItemHandle{0, 0};
        });
      }
    }
  });

  auto sanitizeHandle = [&](ItemHandle h) -> ItemHandle {
    if (!h) return ItemHandle{0, 0};
    if (!service.getStore().isValid(h)) {
      LOG_WARN("ItemPersistenceCodec: Dangling/invalid item handle {{{}, {}}} in save container slot, resetting to empty",
               h.index, h.gen);
      return ItemHandle{0, 0};
    }
    return h;
  };

  for (const auto &[slot, h] : invSlots) {
    if (slot < ItemStorageService::kInventoryCapacity) {
      service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, slot}, sanitizeHandle(h));
    }
  }
  for (const auto &[slot, h] : eqSlots) {
    if (slot < ItemStorageService::kEquipmentCapacity) {
      service.setSlotHandle(SlotRef{ContainerKind::Equipment, slot, 0, 0}, sanitizeHandle(h));
    }
  }
  for (const auto &[slot, h] : bagSlots) {
    if (slot < ItemStorageService::kBagSlotsCapacity) {
      service.setSlotHandle(SlotRef{ContainerKind::BagSlots, slot, 0, 0}, sanitizeHandle(h));
    }
  }

  service.setUnlockedPages(ContainerKind::PersonalStash, personalUnlocked);
  for (const auto &[page, slot, h] : personalSlots) {
    if (page < personalUnlocked && slot < ItemStorageService::kStashPageCapacity) {
      service.setSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, page, slot}, sanitizeHandle(h));
    }
  }
  if (!personalMeta.empty()) {
    service.setPersonalStashMeta(personalMeta);
  }

  service.setUnlockedPages(ContainerKind::SharedStash, sharedUnlocked);
  for (const auto &[page, slot, h] : sharedSlots) {
    if (page < sharedUnlocked && slot < ItemStorageService::kStashPageCapacity) {
      service.setSlotHandle(SlotRef{ContainerKind::SharedStash, 0, page, slot}, sanitizeHandle(h));
    }
  }
  if (!sharedMeta.empty()) {
    service.setSharedStashMeta(sharedMeta);
  }

  for (const auto &[slot, h] : heirloomSlots) {
    if (slot < ItemStorageService::kHeirloomVaultCapacity) {
      service.setSlotHandle(SlotRef{ContainerKind::HeirloomVault, 0, 0, slot}, sanitizeHandle(h));
    }
  }

  service.setMaterials(materials);
  service.setGold(loadedGold);
  if (loadedNextInstanceId > 0) {
    service.setNextInstanceId(loadedNextInstanceId);
  }

  return true;
}

bool ItemPersistenceCodec::encodeStore(const ItemStore &store,
                                       std::ostream &outStream) {
  ItemStorageService tempService;
  std::vector<ItemStore::RawInstanceEntry> entries;
  entries.reserve(store.activeCount());

  store.visit([&entries](ItemHandle h, const ItemInstance &inst) {
    entries.push_back({h.index, h.gen, inst});
  });

  tempService.getStoreMutable().restoreRawEntries(entries, store.getAllSideTables());
  return encode(tempService, outStream, nullptr, ContainerDirtyFlags::ItemInstances | ContainerDirtyFlags::ItemSideTables | ContainerDirtyFlags::ItemSkillModifiers);
}

bool ItemPersistenceCodec::decodeStore(std::istream &inStream,
                                       ItemStore &store) {
  ItemStorageService tempService;
  if (!decode(inStream, tempService)) {
    return false;
  }
  store = tempService.getStore();
  return true;
}

bool ItemPersistenceCodec::SaveFileAtomic(const std::string &targetPath,
                                          const std::string &tempPath,
                                          const std::vector<uint8_t> &data,
                                          bool createBackup) {
  try {
    fs::path target(targetPath);
    fs::path temp(tempPath);

    if (target.has_parent_path()) {
      fs::create_directories(target.parent_path());
    }
    if (temp.has_parent_path()) {
      fs::create_directories(temp.parent_path());
    }

    // 1. 全量写入 temp 文件
    {
      std::ofstream out(temp, std::ios::binary | std::ios::trunc);
      if (!out.is_open()) {
        LOG_ERROR("SaveFileAtomic: Unable to open temp file {}", tempPath);
        return false;
      }
      if (!data.empty()) {
        out.write(reinterpret_cast<const char *>(data.data()), data.size());
      }
      out.flush();
      if (!out.good()) {
        LOG_ERROR("SaveFileAtomic: Stream error while writing temp file {}", tempPath);
        return false;
      }
      out.close();
      if (out.fail()) {
        LOG_ERROR("SaveFileAtomic: Failed to close temp file {}", tempPath);
        return false;
      }
    }

    // 2. 校验 temp 文件大小
    std::error_code ec;
    auto writtenSize = fs::file_size(temp, ec);
    if (ec || writtenSize != data.size()) {
      LOG_ERROR("SaveFileAtomic: Temp file size mismatch: {} vs {}", writtenSize, data.size());
      fs::remove(temp, ec);
      return false;
    }

    // 3. 覆盖前将已有目标文件备份为 .bak (M6: 备份失败中止本次保存，保留已有原档)
    if (createBackup && fs::exists(target, ec)) {
      std::string backupPath = targetPath + ".bak";
      fs::copy_file(target, backupPath, fs::copy_options::overwrite_existing, ec);
      if (ec) {
        LOG_ERROR("SaveFileAtomic: Failed to create backup {}, aborting save to protect original: {}",
                  backupPath, ec.message());
        fs::remove(temp, ec);
        return false;
      }
    }

    // 4. 原子重命名覆盖目标文件
    fs::rename(temp, target, ec);
    if (ec) {
      // 跨卷或 Windows 占用降级为 overwrite copy
      fs::copy_file(temp, target, fs::copy_options::overwrite_existing, ec);
      if (ec) {
        LOG_ERROR("SaveFileAtomic: Failed to replace target file {}: {}", targetPath, ec.message());
        return false;
      }
      fs::remove(temp, ec);
    }

    // 5. 校验目标文件大小
    auto finalSize = fs::file_size(target, ec);
    if (ec || finalSize != data.size()) {
      LOG_ERROR("SaveFileAtomic: Final target file verification failed");
      return false;
    }

    return true;
  } catch (const std::exception &e) {
    LOG_ERROR("SaveFileAtomic: Exception while saving {}: {}", targetPath, e.what());
    return false;
  }
}

} // namespace NoMoreDay
