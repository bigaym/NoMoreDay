#pragma once

#include <array>
#include <string>
#include <vector>
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {

enum class StashTabType : uint8_t { Normal, Equipment, Material, Runeword, Custom };

enum class StashType : uint8_t { Personal, Shared };

struct StashTab {
    static constexpr int CAPACITY = 144;
    std::string name;
    StashTabType type = StashTabType::Normal;
    uint32_t iconId = 0;
    uint32_t color = 0xFFFFFFFF;
    std::array<entt::entity, CAPACITY> items;

    StashTab() { 
        items.fill(entt::null); 
    }
};

struct PersonalStashComponent {
    static constexpr int MAX_TABS = 10;
    static constexpr int INITIAL_UNLOCKED = 1;
    int unlockedTabs = INITIAL_UNLOCKED;
    std::vector<StashTab> tabs;

    PersonalStashComponent() { 
        tabs.resize(INITIAL_UNLOCKED); 
        tabs[0].name = "Stash 1"; 
    }
};

// 城镇仓库实体组件
struct StashInteractableComponent {
    StashType type = StashType::Personal;
};

// 临时占位渲染 (后续替换为 Sprite)
struct StashPlaceholderRender {
    static constexpr float WIDTH = 64.0f;
    static constexpr float HEIGHT = 64.0f;
    Color color = BROWN; // 使用 Raylib 的 BROWN 颜色
};

} // namespace NoMoreDay
