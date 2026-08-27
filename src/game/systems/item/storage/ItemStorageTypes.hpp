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
 * @brief Generation-validated handle referencing an item in ItemStore.
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
 * @brief Categorical identifier for storage containers across gameplay systems.
 */
enum class ContainerKind : uint8_t {
  Inventory,
  Equipment,
  BagSlots,
  PersonalStash,
  SharedStash,
  MaterialBank,
  HeirloomVault,
  GroundPending
};

/**
 * @brief Unified slot addressing structure across all container kinds.
 */
struct SlotRef {
  ContainerKind kind = ContainerKind::Inventory;
  uint8_t container = 0;
  uint16_t page = 0;
  uint16_t index = 0;

  constexpr bool operator==(const SlotRef &other) const noexcept = default;
  constexpr bool operator!=(const SlotRef &other) const noexcept = default;
};

/**
 * @brief Result / error codes for item storage operations.
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
 * @brief Compact 8-byte POD affix representation for inline storage.
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
 * @brief Fixed-size POD item instance structure, zero heap allocation, memcpy-safe.
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
static_assert(std::is_trivially_copyable_v<ItemInstance>, "ItemInstance must be trivially copyable");
static_assert(alignof(ItemInstance) == 8, "ItemInstance alignment must be 8 bytes");

/**
 * @brief Sparse side-table storage for rare variable-length / complex modifiers (Q9).
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
    return (static_cast<size_t>(s.kind) << 24) ^
           (static_cast<size_t>(s.container) << 16) ^
           (static_cast<size_t>(s.page) << 8) ^
           static_cast<size_t>(s.index);
  }
};
} // namespace std
