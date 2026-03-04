#pragma once

#include "ConditionIR.hpp"
#include "TagDomain.hpp"

#include <cstdint>
#include <string_view>

namespace NoMoreDay::CombatV2 {

enum class ConditionCompileStatus : uint8_t {
    Ok = 0,
    InvalidSchema = 1,
    UnknownTag = 2,
    NotImplemented = 3,
};

struct ConditionCompileResult {
    ConditionCompileStatus status{ConditionCompileStatus::NotImplemented};
    ConditionIR conditionIr{};
};

class ConditionCompiler {
  public:
    [[nodiscard]] ConditionCompileResult CompileFromText(std::string_view conditionText,
                                                         const TagDomain &tagDomain) const;
};

} // namespace NoMoreDay::CombatV2
