#pragma once
#include <entt/entt.hpp>

namespace NoMoreDay {

class UICrafting {
public:
    static void Draw(entt::registry& registry);
    static void Update(entt::registry& registry);
    
    // Interaction
    static void SetTargetItem(entt::entity item);
    static entt::entity GetTargetItem();
    static void ClearTargetItem();

    static void Toggle();
    static bool IsVisible();

private:
    static void DrawCraftingPanel(entt::registry& registry);
    static void DrawAffixList(entt::registry& registry, entt::entity item);
    

    // ...
    static inline entt::entity m_targetItem = entt::null;
    
    // Merging State
    static inline entt::entity m_fodderItem = entt::null;
    static inline entt::entity m_catalystItem = entt::null;
    static inline int m_selectedAffixIndex = -1;
    
    enum class CraftingTab { Forging, Merging, Salvaging };
    static inline CraftingTab m_currentTab = CraftingTab::Forging;

    static inline float m_craftingAlpha = 0.0f;
    static inline bool m_visible = false;
    
    static void DrawMergePanel(entt::registry& registry, float startX, float startY, float panelW, float panelH, float alpha);
    static void DrawSalvagePanel(entt::registry& registry, float startX, float startY, float panelW, float panelH, float alpha);
};

}
