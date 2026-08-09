#include "TagDomain.hpp"

#include <array>
#include <utility>

namespace NoMoreDay::CombatV2 {

TagDomain::ResolveResult TagDomain::Resolve(const std::string_view tagName) const {
    constexpr std::array<std::pair<std::string_view, TagId>, 5> kNameToId = {
        std::pair<std::string_view, TagId>{"Fire", static_cast<TagId>(1)},
        std::pair<std::string_view, TagId>{"Burn", static_cast<TagId>(1)},
        std::pair<std::string_view, TagId>{"Cold", static_cast<TagId>(2)},
        std::pair<std::string_view, TagId>{"Shadow", static_cast<TagId>(3)},
        std::pair<std::string_view, TagId>{"Poison", static_cast<TagId>(4)},
    };

    for (const auto &[candidateName, candidateId] : kNameToId) {
        if (candidateName == tagName) {
            return ResolveResult{ResolveStatus::Ok, candidateId};
        }
    }

    return ResolveResult{ResolveStatus::UnknownTag, 0};
}

TagBitset TagDomain::BuildBitset(std::initializer_list<TagId> tagIds) const {
    TagBitset bitset;
    for (const TagId tagId : tagIds) {
        if (!TagBitset::IsValidTagId(tagId)) {
            continue;
        }
        bitset.Set(tagId);
    }
    return bitset;
}

} // namespace NoMoreDay::CombatV2
