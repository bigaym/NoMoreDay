#include "ConditionCompiler.hpp"

#include <cctype>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct MinimalJsonValue {
    enum class Type : uint8_t {
        Object,
        Array,
        String,
    };

    Type type{Type::Object};
    std::vector<std::pair<std::string, MinimalJsonValue>> objectMembers{};
    std::vector<MinimalJsonValue> arrayItems{};
    std::string stringValue{};
};

class MinimalJsonParser {
  public:
    explicit MinimalJsonParser(const std::string_view input) : m_input(input) {}

    bool Parse(MinimalJsonValue &rootOut) {
        SkipWhitespace();
        if (!ParseValue(rootOut)) {
            return false;
        }
        SkipWhitespace();
        return m_position == m_input.size();
    }

  private:
    bool ParseValue(MinimalJsonValue &valueOut) {
        SkipWhitespace();
        if (m_position >= m_input.size()) {
            return false;
        }

        const char marker = m_input[m_position];
        if (marker == '{') {
            return ParseObject(valueOut);
        }
        if (marker == '[') {
            return ParseArray(valueOut);
        }
        if (marker == '"') {
            valueOut.type = MinimalJsonValue::Type::String;
            valueOut.objectMembers.clear();
            valueOut.arrayItems.clear();
            return ParseString(valueOut.stringValue);
        }
        return false;
    }

    bool ParseObject(MinimalJsonValue &valueOut) {
        if (!Consume('{')) {
            return false;
        }

        valueOut.type = MinimalJsonValue::Type::Object;
        valueOut.objectMembers.clear();
        valueOut.arrayItems.clear();
        valueOut.stringValue.clear();

        SkipWhitespace();
        if (Peek('}')) {
            ++m_position;
            return true;
        }

        while (true) {
            std::string key;
            if (!ParseString(key)) {
                return false;
            }

            SkipWhitespace();
            if (!Consume(':')) {
                return false;
            }

            MinimalJsonValue child;
            if (!ParseValue(child)) {
                return false;
            }
            valueOut.objectMembers.emplace_back(std::move(key), std::move(child));

            SkipWhitespace();
            if (Consume('}')) {
                return true;
            }
            if (!Consume(',')) {
                return false;
            }
        }
    }

    bool ParseArray(MinimalJsonValue &valueOut) {
        if (!Consume('[')) {
            return false;
        }

        valueOut.type = MinimalJsonValue::Type::Array;
        valueOut.objectMembers.clear();
        valueOut.arrayItems.clear();
        valueOut.stringValue.clear();

        SkipWhitespace();
        if (Peek(']')) {
            ++m_position;
            return true;
        }

        while (true) {
            MinimalJsonValue item;
            if (!ParseValue(item)) {
                return false;
            }
            valueOut.arrayItems.push_back(std::move(item));

            SkipWhitespace();
            if (Consume(']')) {
                return true;
            }
            if (!Consume(',')) {
                return false;
            }
        }
    }

    bool ParseString(std::string &valueOut) {
        if (!Consume('"')) {
            return false;
        }

        valueOut.clear();
        while (m_position < m_input.size()) {
            const char ch = m_input[m_position++];
            if (ch == '"') {
                return true;
            }
            if (ch == '\\') {
                if (m_position >= m_input.size()) {
                    return false;
                }
                const char escaped = m_input[m_position++];
                switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    valueOut.push_back(escaped);
                    break;
                case 'b':
                    valueOut.push_back('\b');
                    break;
                case 'f':
                    valueOut.push_back('\f');
                    break;
                case 'n':
                    valueOut.push_back('\n');
                    break;
                case 'r':
                    valueOut.push_back('\r');
                    break;
                case 't':
                    valueOut.push_back('\t');
                    break;
                default:
                    return false;
                }
                continue;
            }

            if (static_cast<unsigned char>(ch) < 0x20u) {
                return false;
            }
            valueOut.push_back(ch);
        }

        return false;
    }

    void SkipWhitespace() {
        while (m_position < m_input.size() && std::isspace(static_cast<unsigned char>(m_input[m_position])) != 0) {
            ++m_position;
        }
    }

    [[nodiscard]] bool Peek(const char expected) const {
        return m_position < m_input.size() && m_input[m_position] == expected;
    }

    bool Consume(const char expected) {
        if (!Peek(expected)) {
            return false;
        }
        ++m_position;
        return true;
    }

    std::string_view m_input;
    std::size_t m_position{0};
};

} // namespace

namespace NoMoreDay::CombatV2 {

ConditionCompileResult ConditionCompiler::CompileFromText(const std::string_view conditionText,
                                                         const TagDomain &tagDomain) const {
    ConditionCompileResult result;
    result.status = ConditionCompileStatus::InvalidSchema;

    MinimalJsonValue parsed;
    MinimalJsonParser parser(conditionText);
    if (!parser.Parse(parsed)) {
        return result;
    }

    std::function<ConditionCompileStatus(const MinimalJsonValue &, uint32_t &)> compileNode;
    compileNode = [&](const MinimalJsonValue &expression, uint32_t &nodeIndexOut) -> ConditionCompileStatus {
        if (expression.type != MinimalJsonValue::Type::Object || expression.objectMembers.size() != 1) {
            return ConditionCompileStatus::InvalidSchema;
        }

        const auto &[opName, opValue] = expression.objectMembers.front();

        auto appendNode = [&](const ConditionNodeOp op) -> uint32_t {
            nodeIndexOut = static_cast<uint32_t>(result.conditionIr.nodes.size());
            result.conditionIr.nodes.push_back(ConditionNode{});
            result.conditionIr.nodes.back().op = op;
            return nodeIndexOut;
        };

        if (opName == "all" || opName == "any" || opName == "none") {
            if (opValue.type != MinimalJsonValue::Type::Array || opValue.arrayItems.empty()) {
                return ConditionCompileStatus::InvalidSchema;
            }

            const ConditionNodeOp op =
                (opName == "all")   ? ConditionNodeOp::All
                : (opName == "any") ? ConditionNodeOp::Any
                                      : ConditionNodeOp::None;

            const uint32_t currentIndex = appendNode(op);
            for (const MinimalJsonValue &child : opValue.arrayItems) {
                uint32_t childIndex = 0;
                const ConditionCompileStatus childStatus = compileNode(child, childIndex);
                if (childStatus != ConditionCompileStatus::Ok) {
                    return childStatus;
                }
                result.conditionIr.nodes[currentIndex].childIndices.push_back(childIndex);
            }

            return ConditionCompileStatus::Ok;
        }

        if (opName == "not") {
            if (opValue.type != MinimalJsonValue::Type::Object) {
                return ConditionCompileStatus::InvalidSchema;
            }

            const uint32_t currentIndex = appendNode(ConditionNodeOp::Not);
            uint32_t childIndex = 0;
            const ConditionCompileStatus childStatus = compileNode(opValue, childIndex);
            if (childStatus != ConditionCompileStatus::Ok) {
                return childStatus;
            }
            result.conditionIr.nodes[currentIndex].childIndices.push_back(childIndex);
            return ConditionCompileStatus::Ok;
        }

        if (opName == "has_tags_all" || opName == "has_tags_any") {
            if (opValue.type != MinimalJsonValue::Type::Array || opValue.arrayItems.empty()) {
                return ConditionCompileStatus::InvalidSchema;
            }

            const ConditionNodeOp op =
                (opName == "has_tags_all") ? ConditionNodeOp::HasTagsAll : ConditionNodeOp::HasTagsAny;

            const uint32_t currentIndex = appendNode(op);
            ConditionNode &node = result.conditionIr.nodes[currentIndex];

            for (const MinimalJsonValue &rawTag : opValue.arrayItems) {
                if (rawTag.type != MinimalJsonValue::Type::String) {
                    return ConditionCompileStatus::InvalidSchema;
                }

                const std::string &tagName = rawTag.stringValue;
                if (tagName.empty()) {
                    return ConditionCompileStatus::InvalidSchema;
                }

                const auto resolved = tagDomain.Resolve(tagName);
                if (resolved.status == TagDomain::ResolveStatus::UnknownTag) {
                    return ConditionCompileStatus::UnknownTag;
                }
                if (resolved.status != TagDomain::ResolveStatus::Ok) {
                    return ConditionCompileStatus::NotImplemented;
                }

                node.tagIds.push_back(resolved.tagId);
            }

            return ConditionCompileStatus::Ok;
        }

        return ConditionCompileStatus::InvalidSchema;
    };

    uint32_t rootNodeIndex = 0;
    const ConditionCompileStatus status = compileNode(parsed, rootNodeIndex);
    result.status = status;
    if (status == ConditionCompileStatus::Ok) {
        result.conditionIr.rootNodeIndex = rootNodeIndex;
    } else {
        result.conditionIr = ConditionIR{};
    }
    return result;
}

} // namespace NoMoreDay::CombatV2
