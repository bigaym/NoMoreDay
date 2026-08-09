#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace NoMoreDay::CombatV2 {

class TagBitset {
  public:
    static constexpr std::size_t kMaxTags = 256;
    static constexpr std::size_t kWordBits = 64;
    static constexpr std::size_t kWordCount = kMaxTags / kWordBits;

    TagBitset() = default;

    [[nodiscard]] static bool IsValidTagId(uint16_t tagId);
    void Set(const uint16_t tagId);
    [[nodiscard]] bool Has(const uint16_t tagId) const;
    [[nodiscard]] bool HasAll(const TagBitset &required) const;
    [[nodiscard]] bool HasAny(const TagBitset &candidate) const;

  private:
    std::array<uint64_t, kWordCount> m_words{};
};

} // namespace NoMoreDay::CombatV2
