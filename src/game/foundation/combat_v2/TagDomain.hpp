#pragma once

#include "TagBitset.hpp"

#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace NoMoreDay::CombatV2 {

class TagDomain {
  public:
    using TagId = uint16_t;

    enum class ResolveStatus : uint8_t {
        Ok = 0,
        UnknownTag = 1,
        NotImplemented = 2,
    };

    struct ResolveResult {
        ResolveStatus status{ResolveStatus::UnknownTag};
        TagId tagId{0};
    };

    TagDomain() = default;

    [[nodiscard]] ResolveResult Resolve(std::string_view tagName) const;
    [[nodiscard]] TagBitset BuildBitset(std::initializer_list<TagId> tagIds) const;
};

} // namespace NoMoreDay::CombatV2
