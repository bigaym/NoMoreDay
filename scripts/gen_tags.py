"""
NoMoreDay Tag Registry Generator
Purpose: Converts 'tags.json' into a C++ 'TagRegistry.hpp' with 64-bit bitmask enums.
         Automates the synchronization of gameplay tags between data and engine.
Usage: python scripts/gen_tags.py
"""

import json
import os


def generate_header(json_path, output_path):
    with open(json_path, "r") as f:
        data = json.load(f)

    header_content = """#pragma once
#include <cstdint>
#include <array>
#include <string_view>
#include <string>
#include <vector>
#include <optional>
#include <bit>

namespace NoMoreDay {

enum class Tag : uint64_t {
    None = 0,
"""

    tag_to_name = {}
    name_to_tag = {}

    for category in data["categories"]:
        start_bit = category["range"][0]
        end_bit = category["range"][1]
        tags = category["tags"]

        header_content += (
            f"\n    // --- {category['name']} Tags ({start_bit}-{end_bit}) ---\n"
        )

        for i, tag_name in enumerate(tags):
            bit_index = start_bit + i
            if bit_index > end_bit:
                raise ValueError(f"Too many tags in category {category['name']}")

            value = 1 << bit_index
            header_content += f"    {tag_name} = 1ULL << {bit_index},\n"
            tag_to_name[value] = tag_name
            name_to_tag[tag_name] = value

    header_content += """};

constexpr Tag operator|(Tag lhs, Tag rhs) {
    return static_cast<Tag>(static_cast<uint64_t>(lhs) | static_cast<uint64_t>(rhs));
}

constexpr Tag operator&(Tag lhs, Tag rhs) {
    return static_cast<Tag>(static_cast<uint64_t>(lhs) & static_cast<uint64_t>(rhs));
}

constexpr Tag operator^(Tag lhs, Tag rhs) {
    return static_cast<Tag>(static_cast<uint64_t>(lhs) ^ static_cast<uint64_t>(rhs));
}

constexpr Tag operator~(Tag t) {
    return static_cast<Tag>(~static_cast<uint64_t>(t));
}

struct TagInfo {
    Tag tag;
    std::string_view id;
};

static constexpr std::array<TagInfo, COUNT_PLACEHOLDER> kTagInfoTable = {{
TABLE_CONTENT_PLACEHOLDER
}};

constexpr bool HasTag(Tag mask, Tag tag) {
    return (mask & tag) == tag;
}

// Helper to get string name of a SINGLE tag
constexpr std::string_view GetTagName(Tag tag) {
    switch(tag) {
"""

    for value, name in tag_to_name.items():
        header_content += f'        case Tag::{name}: return "{name}";\n'

    header_content += """        default: return \"Unknown\";
    }
}

// Helper to get Tag from string name
constexpr std::optional<Tag> TagFromString(std::string_view name) {
"""

    for value, name in tag_to_name.items():
        header_content += f'    if (name == "{name}") return Tag::{name};\n'

    header_content += """    return std::nullopt;
}

// Helper to parse a list of strings into a Tag mask
inline Tag ParseTagList(const std::vector<std::string>& tags) {
    Tag mask = Tag::None;
    for (const auto& str : tags) {
        if (auto t = TagFromString(str)) {
            mask = mask | *t;
        }
    }
    return mask;
}

} // namespace NoMoreDay
"""

    table_content = ""
    for value, name in tag_to_name.items():
        table_content += f'    {{Tag::{name}, "{name}"}},\n'

    header_content = header_content.replace("COUNT_PLACEHOLDER", str(len(tag_to_name)))
    header_content = header_content.replace("TABLE_CONTENT_PLACEHOLDER", table_content)

    with open(output_path, "w") as f:
        f.write(header_content)


if __name__ == "__main__":
    json_path = os.path.join("assets", "data", "tags.json")
    output_path = os.path.join("src", "game", "foundation", "data", "TagRegistry.hpp")

    # Ensure directory exists
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    generate_header(json_path, output_path)
    print(f"Generated {output_path} from {json_path}")
