#include "TagBitset.hpp"

namespace NoMoreDay::CombatV2 {

bool TagBitset::IsValidTagId(const uint16_t tagId) {
    return static_cast<std::size_t>(tagId) < kMaxTags;
}

void TagBitset::Set(const uint16_t tagId) {
    if (!IsValidTagId(tagId)) {
        return;
    }

    const std::size_t wordIndex = static_cast<std::size_t>(tagId) / kWordBits;
    const std::size_t bitIndex = static_cast<std::size_t>(tagId) % kWordBits;
    m_words[wordIndex] |= (1ull << bitIndex);
}

bool TagBitset::Has(const uint16_t tagId) const {
    if (!IsValidTagId(tagId)) {
        return false;
    }

    const std::size_t wordIndex = static_cast<std::size_t>(tagId) / kWordBits;
    const std::size_t bitIndex = static_cast<std::size_t>(tagId) % kWordBits;
    return (m_words[wordIndex] & (1ull << bitIndex)) != 0;
}

bool TagBitset::HasAll(const TagBitset &required) const {
    for (std::size_t i = 0; i < m_words.size(); ++i) {
        if ((m_words[i] & required.m_words[i]) != required.m_words[i]) {
            return false;
        }
    }

    return true;
}

bool TagBitset::HasAny(const TagBitset &candidate) const {
    for (std::size_t i = 0; i < m_words.size(); ++i) {
        if ((m_words[i] & candidate.m_words[i]) != 0) {
            return true;
        }
    }

    return false;
}

} // namespace NoMoreDay::CombatV2
