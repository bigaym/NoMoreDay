#pragma once

#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

namespace NoMoreDay {

/**
 * @brief 引用 ItemStore 中物品的带代际校验句柄。
 */
struct ItemHandle {
  uint32_t index = 0;
  uint32_t gen = 0;

  constexpr bool operator==(const ItemHandle &other) const noexcept = default;
  constexpr bool operator!=(const ItemHandle &other) const noexcept = default;

  constexpr explicit operator bool() const noexcept {
    return index != 0 || gen != 0;
  }
};
static_assert(sizeof(ItemHandle) == 8, "ItemHandle size must be exactly 8 bytes");
static_assert(std::is_trivially_copyable_v<ItemHandle>, "ItemHandle must be trivially copyable");

/**
 * @brief 跨玩法系统的存储容器类别标识符。
 */
enum class ContainerKind : uint8_t {
  Inventory,
  Equipment,
  BagSlots,
  PersonalStash,
  SharedStash,
  MaterialBank,
  HeirloomVault
};

/**
 * @brief 跨所有容器类别的统一槽位寻址结构。
 *
 * 寻址约定 (单一真实来源，由 isWellFormed() 强制保证):
 * - 平铺容器 (Inventory, HeirloomVault): kind + index。
 * - 分页仓库 (PersonalStash, SharedStash): kind + page + index。
 * - 具名槽位容器 (Equipment, BagSlots): 仅 kind + container —
 *   container 携带 EquipmentSlot 枚举值 / 背包索引；index 必须保持为 0，以防两字段冲突。
 */
struct SlotRef {
  ContainerKind kind = ContainerKind::Inventory;
  uint8_t container = 0;
  uint16_t page = 0;
  uint16_t index = 0;

  constexpr bool operator==(const SlotRef &other) const noexcept = default;
  constexpr bool operator!=(const SlotRef &other) const noexcept = default;

  [[nodiscard]] constexpr bool isWellFormed() const noexcept {
    switch (kind) {
    case ContainerKind::Equipment:
    case ContainerKind::BagSlots:
      return index == 0;
    case ContainerKind::PersonalStash:
    case ContainerKind::SharedStash:
      return container == 0;
    default:
      return container == 0 && page == 0;
    }
  }
};

/**
 * @brief 物品存储操作的结果/错误代码。
 */
enum class StorageError : uint8_t {
  Ok = 0,
  NotFound,
  ContainerFull,
  TypeMismatch,
  Locked,
  InvalidHandle,
  InvalidSlot,
  CapacityExceeded,
  Unknown
};

/**
 * @brief 用于内联存储的紧凑 8 字节 POD 词缀表示。
 */
struct alignas(4) CompactAffix {
  AffixType type = AffixType::Strength;
  uint8_t tier = 0;
  bool isPrefix = false;
  float value = 0.0f;

  constexpr bool operator==(const CompactAffix &other) const noexcept = default;
  constexpr bool operator!=(const CompactAffix &other) const noexcept = default;
};
static_assert(sizeof(CompactAffix) == 8, "CompactAffix size must be 8 bytes");
static_assert(std::is_trivially_copyable_v<CompactAffix>, "CompactAffix must be trivially copyable");

/**
 * @brief 固定大小的 POD 物品实例结构，零堆分配，可安全 memcpy。
 */
struct alignas(8) ItemInstance {
  uint64_t instanceId = 0;
  uint32_t baseId = 0;
  uint32_t quantity = 1;
  uint16_t itemLevel = 1;
  uint8_t rarity = 0;
  uint8_t flags = 0; // bit 0: isLocked, bit 1: isTwoHanded
  int16_t forgingPotential = 0;
  uint8_t legendaryPotential = 0;
  uint8_t socketCount = 0;
  float attack = 0.0f;
  float defense = 0.0f;
  float value = 0.0f;
  uint8_t affixCount = 0;
  uint8_t _padding[3] = {0, 0, 0};
  std::array<CompactAffix, 12> affixes{};
  std::array<ItemHandle, 6> sockets{};
  uint32_t activeRunewordId = 0;
  uint32_t _padding2 = 0;

  [[nodiscard]] constexpr bool isLocked() const noexcept {
    return (flags & 0x01) != 0;
  }

  constexpr void setLocked(bool locked) noexcept {
    if (locked) {
      flags |= 0x01;
    } else {
      flags &= ~0x01;
    }
  }

  [[nodiscard]] constexpr bool isTwoHanded() const noexcept {
    return (flags & 0x02) != 0;
  }

  constexpr void setTwoHanded(bool twoHanded) noexcept {
    if (twoHanded) {
      flags |= 0x02;
    } else {
      flags &= ~0x02;
    }
  }

  bool operator==(const ItemInstance &other) const noexcept = default;
  bool operator!=(const ItemInstance &other) const noexcept = default;
};
static_assert(sizeof(ItemInstance) == 192, "ItemInstance size must be exactly 192 bytes. Increment ITEM_STORE_BINARY_VERSION if modified.");
static_assert(std::is_trivially_copyable_v<ItemInstance>, "ItemInstance must be trivially copyable");
static_assert(alignof(ItemInstance) == 8, "ItemInstance alignment must be 8 bytes");

/**
 * @brief 针对罕见的变长/复杂修改器的稀疏旁表存储 (Q9)。
 */
struct ItemSideTableData {
  std::vector<StatConversion> conversions;
  std::vector<DamageModifier> damage_modifiers;

  [[nodiscard]] bool empty() const noexcept {
    return conversions.empty() && damage_modifiers.empty();
  }

  void clear() noexcept {
    conversions.clear();
    damage_modifiers.clear();
  }
};

/**
 * @brief 仓库分页元数据（名称、类型、图标、颜色）。
 */
struct StashTabMeta {
  std::string name = "Stash";
  uint8_t type = 0;
  uint32_t iconId = 0;
  uint32_t color = 0xFFFFFFFF;

  bool operator==(const StashTabMeta &other) const noexcept = default;
  bool operator!=(const StashTabMeta &other) const noexcept = default;
};

} // namespace NoMoreDay

namespace std {
template <>
struct hash<NoMoreDay::ItemHandle> {
  size_t operator()(const NoMoreDay::ItemHandle &h) const noexcept {
    return (static_cast<size_t>(h.index) << 32) ^ static_cast<size_t>(h.gen);
  }
};

template <>
struct hash<NoMoreDay::SlotRef> {
  size_t operator()(const NoMoreDay::SlotRef &s) const noexcept {
    // 64 位字段打包，字段间零位重叠:
    // [56..64) kind | [40..48) container | [24..40) page | [0..16) index.
    // 先前的打包 (page<<8) 会将 page 重叠进 index 位中并降低哈希桶分布均匀性。
    return (static_cast<size_t>(s.kind) << 56) ^
           (static_cast<size_t>(s.container) << 40) ^
           (static_cast<size_t>(s.page) << 24) ^
           static_cast<size_t>(s.index);
  }
};
} // namespace std
