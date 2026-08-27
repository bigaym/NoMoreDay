#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "core/logging/Logger.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace NoMoreDay {

namespace {

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

// 序列化 Section 1: TemplateFingerprint
uint64_t calculateTemplateFingerprint() {
  uint64_t fingerprint = 0x1469598103934665ULL; // FNV offset basis
  for (const auto &[baseId, tmpl] : ItemTemplateRegistry::Get().getAllTemplates()) {
    fingerprint ^= static_cast<uint64_t>(baseId);
    fingerprint *= 1099511628211ULL; // FNV prime
  }
  return fingerprint;
}

std::vector<uint8_t> buildTemplateFingerprintSection() {
  std::vector<uint8_t> payload;
  const uint64_t fingerprint = calculateTemplateFingerprint();
  appendBytes(payload, fingerprint);
  return payload;
}

// 序列化 Section 2: ItemInstances
std::vector<uint8_t> buildItemInstancesSection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  const auto &store = service.getStore();
  const uint32_t activeCount = static_cast<uint32_t>(store.activeCount());
  appendBytes(payload, activeCount);

  store.visit([&payload](ItemHandle h, const ItemInstance &inst) {
    appendBytes(payload, h.index);
    appendBytes(payload, h.gen);
    appendBytes(payload, inst);
  });

  return payload;
}

// 序列化 Section 3: ItemSideTables
std::vector<uint8_t> buildItemSideTablesSection(const ItemStorageService &service) {
  std::vector<uint8_t> payload;
  const auto &sideTables = service.getStore().getAllSideTables();
  const uint32_t count = static_cast<uint32_t>(sideTables.size());
  appendBytes(payload, count);

  for (const auto &[idx, data] : sideTables) {
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
    } else {
      std::vector<uint8_t> payload = builderFn();
      uint32_t crc = calculateCrc32(payload.data(), payload.size());
      if (cache) {
        cache->put(type, payload, crc);
      }
      sections.push_back({type, std::move(payload), crc});
    }
  };

  processSection(SectionType::TemplateFingerprint, ContainerDirtyFlags::TemplateFingerprint,
                 [&]() { return buildTemplateFingerprintSection(); });
  processSection(SectionType::ItemInstances, ContainerDirtyFlags::ItemInstances,
                 [&]() { return buildItemInstancesSection(service); });
  processSection(SectionType::ItemSideTables, ContainerDirtyFlags::ItemSideTables,
                 [&]() { return buildItemSideTablesSection(service); });
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

  // 1. 读取并校验 FileHeader
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

  // 2. 读取并校验 SectionHeader 表
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

  // 阶段 1: 读取所有 Section 数据并进行 CRC32 事务性预校验
  std::vector<std::vector<uint8_t>> sectionData(headers.size());
  for (size_t i = 0; i < headers.size(); ++i) {
    sectionData[i].resize(headers[i].length);
    if (headers[i].length > 0) {
      inStream.seekg(headers[i].offset, std::ios::beg);
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
      for (uint32_t k = 0; k < count; ++k) {
        uint32_t idx = 0;
        if (!readBytes(ptr, end, idx)) return false;
        ItemSideTableData data;

        uint32_t convCount = 0;
        if (!readBytes(ptr, end, convCount)) return false;
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
        sideTables[idx] = std::move(data);
      }
      break;
    }
    case SectionType::Inventory: {
      uint32_t capacity = 0, occupied = 0;
      if (!readBytes(ptr, end, capacity) || !readBytes(ptr, end, occupied)) {
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
      personalMeta.resize(personalUnlocked);
      for (uint16_t p = 0; p < personalUnlocked; ++p) {
        uint16_t nameLen = 0;
        if (!readBytes(ptr, end, nameLen)) return false;
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
      sharedMeta.resize(sharedUnlocked);
      for (uint16_t p = 0; p < sharedUnlocked; ++p) {
        uint16_t nameLen = 0;
        if (!readBytes(ptr, end, nameLen)) return false;
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
    default:
      break;
    }
  }

  // 阶段 3: 将所有解析结果灌入目标 service
  service.clearAll();
  service.getStoreMutable().restoreRawEntries(rawEntries, sideTables);

  for (const auto &[slot, h] : invSlots) {
    service.setSlotHandle(SlotRef{ContainerKind::Inventory, 0, 0, slot}, h);
  }
  for (const auto &[slot, h] : eqSlots) {
    service.setSlotHandle(SlotRef{ContainerKind::Equipment, slot, 0, 0}, h);
  }
  for (const auto &[slot, h] : bagSlots) {
    service.setSlotHandle(SlotRef{ContainerKind::BagSlots, slot, 0, 0}, h);
  }

  service.setUnlockedPages(ContainerKind::PersonalStash, personalUnlocked);
  for (const auto &[page, slot, h] : personalSlots) {
    service.setSlotHandle(SlotRef{ContainerKind::PersonalStash, 0, page, slot}, h);
  }
  if (!personalMeta.empty()) {
    service.setPersonalStashMeta(personalMeta);
  }

  service.setUnlockedPages(ContainerKind::SharedStash, sharedUnlocked);
  for (const auto &[page, slot, h] : sharedSlots) {
    service.setSlotHandle(SlotRef{ContainerKind::SharedStash, 0, page, slot}, h);
  }
  if (!sharedMeta.empty()) {
    service.setSharedStashMeta(sharedMeta);
  }

  for (const auto &[slot, h] : heirloomSlots) {
    service.setSlotHandle(SlotRef{ContainerKind::HeirloomVault, 0, 0, slot}, h);
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
  return encode(tempService, outStream, nullptr, ContainerDirtyFlags::ItemInstances | ContainerDirtyFlags::ItemSideTables);
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

    // 3. 覆盖前将已有目标文件备份为 .bak
    if (createBackup && fs::exists(target, ec)) {
      std::string backupPath = targetPath + ".bak";
      fs::copy_file(target, backupPath, fs::copy_options::overwrite_existing, ec);
      if (ec) {
        LOG_WARN("SaveFileAtomic: Failed to create backup {}: {}", backupPath, ec.message());
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
