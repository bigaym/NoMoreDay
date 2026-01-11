#pragma once

#include "game/components/ItemComponent.hpp"
#include "game/components/Common.hpp"
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace NoMoreDay {

// 过滤行为
enum class FilterActionType {
    SHOW,
    HIDE,
    EMPHASIZE
};

// 过滤动作详情
struct FilterAction {
    FilterActionType type = FilterActionType::SHOW;
    std::optional<Color> colorOverride; // R,G,B,A
    float scale = 1.0f;
    bool minimapIcon = false;
};

// 过滤条件
struct FilterCondition {
    std::optional<Rarity> minRarity;
    std::optional<Rarity> maxRarity;
    std::optional<int> minLevel;
    std::optional<int> maxLevel;
    std::optional<ItemType> itemType;
    std::vector<std::string> hasAffixes; // 必须包含所有列出的词缀 (部分匹配名称)
    std::optional<std::string> baseName; // 底材名称匹配
};

// 单条规则
struct FilterRule {
    std::string name;
    bool enabled = true;
    FilterCondition condition;
    FilterAction action;

    // 评估物品是否匹配此规则
    bool matches(const ItemComponent& item, int itemLevel = 1) const;
};

// 过滤器配置文件结构
struct LootFilterProfile {
    std::string name;
    std::string description;
    std::vector<FilterRule> rules;
};

class LootFilter {
public:
    static void load(const std::string& path);
    static FilterAction evaluate(const ItemComponent& item, int itemLevel);
    static const LootFilterProfile& getCurrentProfile() { return s_currentProfile; }

private:
    static LootFilterProfile s_currentProfile;
    
    // JSON 序列化辅助
    static void from_json(const nlohmann::json& j, LootFilterProfile& p);
    static void to_json(nlohmann::json& j, const LootFilterProfile& p);
};

// 组件：附加在掉落物实体上，指示其过滤状态
struct LootFilterResultComponent {
    bool visible = true;
    Color color = WHITE;
    float scale = 1.0f;
    bool showOnMinimap = false;
};

} // namespace NoMoreDay
