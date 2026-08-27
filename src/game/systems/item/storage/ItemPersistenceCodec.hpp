#pragma once

#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemStorageTypes.hpp"
#include "game/systems/item/storage/ItemStore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {

/**
 * @brief 二进制存储魔数: 'NMDS' (NoMoreDay Storage) -> 0x4E4D4453
 */
inline constexpr uint32_t ITEM_STORE_MAGIC = 0x4E4D4453;

/**
 * @brief 当前二进制持久化格式版本号 (v1)。
 * 契约说明：ItemInstance 结构体基于 static_assert(std::is_trivially_copyable_v) 进行整块 POD 序列化，
 * 若未来向 ItemInstance 增删字段、调整对齐或改变词缀/插槽容量，必须递增 ITEM_STORE_BINARY_VERSION。
 */
inline constexpr uint32_t ITEM_STORE_BINARY_VERSION = 1;

/**
 * @brief 高效标准 IEEE 802.3 CRC32 查找表与计算函数
 */
namespace Detail {
inline constexpr auto GenerateCrc32Table() {
  std::array<uint32_t, 256> table{};
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t crc = i;
    for (int j = 0; j < 8; ++j) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320u;
      } else {
        crc >>= 1;
      }
    }
    table[i] = crc;
  }
  return table;
}

inline constexpr auto kCrc32LookupTable = GenerateCrc32Table();
} // namespace Detail

/**
 * @brief 计算给定内存缓冲区的 IEEE 802.3 CRC32 校验和。
 * @param data 指向数据缓冲区的指针
 * @param size 数据大小（字节数）
 * @param initial 初始值（默认 0xFFFFFFFF）
 * @return 32 位 CRC 校验和
 */
inline uint32_t calculateCrc32(const void *data, size_t size,
                               uint32_t initial = 0xFFFFFFFF) noexcept {
  if (!data || size == 0) {
    return 0;
  }
  const auto *bytes = static_cast<const uint8_t *>(data);
  uint32_t crc = initial;
  for (size_t i = 0; i < size; ++i) {
    crc = (crc >> 8) ^ Detail::kCrc32LookupTable[(crc ^ bytes[i]) & 0xFF];
  }
  return crc ^ 0xFFFFFFFF;
}

/**
 * @brief 二进制分段 (Section) 类型枚举。
 */
enum class SectionType : uint32_t {
  TemplateFingerprint = 1, //!< 模板指纹哈希（校验版本兼容性）
  ItemInstances = 2,       //!< ItemStore 内所有活跃的 ItemInstance POD 数组
  ItemSideTables = 3,      //!< 稀疏旁表数据（StatConversion, DamageModifier）
  Inventory = 4,           //!< capacity 与 (slotIndex, ItemHandle) 数组
  Equipment = 5,           //!< (slotEnum, ItemHandle) 数组
  BagSlots = 6,            //!< (bagIndex, ItemHandle) 数组
  PersonalStash = 7,       //!< unlockedTabs, tabs 与 (tabIndex, slotIndex, ItemHandle) 数组
  SharedStash = 8,         //!< unlockedTabs, tabs 与 (tabIndex, slotIndex, ItemHandle) 数组
  HeirloomVault = 9,       //!< (slotIndex, ItemHandle) 数组
  MaterialBank = 10,       //!< (id, count) 有序数组
  EconomyMetadata = 11,    //!< gold, nextInstanceId
  ProgressionData = 12,    //!< 预留承载角色的非物品核心状态（JSON 或紧凑字节流）
  Count
};

/**
 * @brief 容器与 Section 脏标记位掩码
 */
namespace ContainerDirtyFlags {
inline constexpr uint32_t None = 0;
inline constexpr uint32_t TemplateFingerprint = 1 << 0;
inline constexpr uint32_t ItemInstances = 1 << 1;
inline constexpr uint32_t ItemSideTables = 1 << 2;
inline constexpr uint32_t Inventory = 1 << 3;
inline constexpr uint32_t Equipment = 1 << 4;
inline constexpr uint32_t BagSlots = 1 << 5;
inline constexpr uint32_t PersonalStash = 1 << 6;
inline constexpr uint32_t SharedStash = 1 << 7;
inline constexpr uint32_t HeirloomVault = 1 << 8;
inline constexpr uint32_t MaterialBank = 1 << 9;
inline constexpr uint32_t EconomyMetadata = 1 << 10;
inline constexpr uint32_t ProgressionData = 1 << 11;
inline constexpr uint32_t All = 0xFFFFFFFF;
} // namespace ContainerDirtyFlags

using ContainerDirtyMask = uint32_t;

#pragma pack(push, 1)
/**
 * @brief 二进制文件头 (16 字节)
 */
struct FileHeader {
  uint32_t magic = ITEM_STORE_MAGIC;
  uint32_t version = ITEM_STORE_BINARY_VERSION;
  uint32_t sectionCount = 0;
  uint32_t headerCrc32 = 0; // SectionHeaders 表自身的 CRC32 校验
};
static_assert(sizeof(FileHeader) == 16, "FileHeader must be exactly 16 bytes");

/**
 * @brief 分段头元数据结构 (16 字节)
 */
struct SectionHeader {
  uint32_t type = 0;   //!< SectionType
  uint32_t offset = 0; //!< 相对文件起点的绝对字节偏移
  uint32_t length = 0; //!< Payload 字节长度
  uint32_t crc32 = 0;  //!< Payload 的 CRC32 校验和
};
static_assert(sizeof(SectionHeader) == 16, "SectionHeader must be exactly 16 bytes");
#pragma pack(pop)

/**
 * @brief 内存级 Section 字节流与 CRC 缓存。
 * 用于增量保存时直接复用未修改容器的已序列化内存块。
 */
class InMemorySectionCache {
public:
  struct CachedSection {
    std::vector<uint8_t> payload;
    uint32_t crc32 = 0;
  };

  void put(SectionType type, std::vector<uint8_t> payload, uint32_t crc32) {
    m_cache[type] = CachedSection{std::move(payload), crc32};
  }

  [[nodiscard]] const CachedSection *get(SectionType type) const noexcept {
    auto it = m_cache.find(type);
    if (it != m_cache.end()) {
      return &it->second;
    }
    return nullptr;
  }

  [[nodiscard]] bool contains(SectionType type) const noexcept {
    return m_cache.find(type) != m_cache.end();
  }

  void invalidate(SectionType type) noexcept {
    m_cache.erase(type);
  }

  void clear() noexcept {
    m_cache.clear();
  }

  [[nodiscard]] size_t size() const noexcept {
    return m_cache.size();
  }

private:
  std::unordered_map<SectionType, CachedSection> m_cache;
};

/**
 * @brief 核心二进制持久化编解码器
 */
class ItemPersistenceCodec {
public:
  /**
   * @brief 将 ItemStorageService 编码为版本化二进制流。
   * 支持增量内存 Section 缓存与脏标记优化。
   *
   * @param service 物品存储服务
   * @param outStream 输出流
   * @param cache 可选的内存 Section 缓存指针
   * @param dirtyMask 容器脏标记位掩码（默认全脏）
   * @param progressionPayload 预留承载角色的非物品核心状态字符串或数据
   * @return true 编码成功，false 编码失败
   */
  static bool encode(const ItemStorageService &service,
                     std::ostream &outStream,
                     InMemorySectionCache *cache = nullptr,
                     uint32_t dirtyMask = ContainerDirtyFlags::All,
                     const std::string &progressionPayload = "");

  /**
   * @brief 从二进制流解码并恢复 ItemStorageService。
   * 包含 Magic/Version 校验、SectionHeader 校验、全段 CRC32 事务性校验与防脏数据机制。
   *
   * @param inStream 输入流
   * @param service 目标物品存储服务（仅在全量校验通过后恢复）
   * @param outProgressionPayload 可选的非物品核心状态输出
   * @return true 解码成功，false 格式损坏/校验失败
   */
  static bool decode(std::istream &inStream,
                     ItemStorageService &service,
                     std::string *outProgressionPayload = nullptr);

  /**
   * @brief 纯 ItemStore 的快速独立二进制编码。
   */
  static bool encodeStore(const ItemStore &store, std::ostream &outStream);

  /**
   * @brief 纯 ItemStore 的快速独立二进制解码。
   */
  static bool decodeStore(std::istream &inStream, ItemStore &store);

  /**
   * @brief 磁盘安全写入辅助函数。
   * 全量写入 temp 目录，内容校验后原子 rename 为目标文件，并在 rename 前将既有文件备份为 .bak。
   *
   * @param targetPath 目标文件路径
   * @param tempPath 临时文件路径
   * @param data 要写入的二进制数据
   * @param createBackup 是否在覆盖前备份既有文件为 .bak
   * @return true 写入并原子替换成功，false 发生错误
   */
  static bool SaveFileAtomic(const std::string &targetPath,
                             const std::string &tempPath,
                             const std::vector<uint8_t> &data,
                             bool createBackup = true);
};

} // namespace NoMoreDay
