#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace NoMoreDay {

// 掉落物条目类型
enum class LootEntryType {
    Item,
    Gold,
    SubTable
};

// 涓烘灇涓炬彁渚涚畝鍗曠殑搴忓垪鍖栨敮鎸 (杞负搴曞眰鏁存暟)
inline void to_json(nlohmann::json& j, const LootEntryType& e) { j = static_cast<uint8_t>(e); }
inline void from_json(const nlohmann::json& j, LootEntryType& e) { e = static_cast<LootEntryType>(j.get<uint8_t>()); }

// 掉落物条目定义
struct LootEntry {
    LootEntryType type;     // 掉落物类型
    uint32_t id;            // 物品ID (基础物品类型) 或 子表ID
    uint32_t minAmount = 1; // 最小数量
    uint32_t maxAmount = 1; // 最大数量
    float weight = 1.0f;    // 掉落权重
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LootEntry, type, id, minAmount, maxAmount, weight)

// 掉落池/掉落表：包含一组可能的掉落物及其权重
struct LootPool {
    uint32_t id;            // 掉落池唯一ID
    std::string name;       // 掉落池名称
    std::vector<LootEntry> entries;
    float totalWeight = 0.0f;

    // 计算总权重
    void calculateTotalWeight() {
        totalWeight = 0.0f;
        for (const auto& entry : entries) {
            totalWeight += entry.weight;
        }
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LootPool, id, name, entries)

} // namespace NoMoreDay
