#pragma once
#include "game/components/ItemComponent.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {

struct SalvageFilter {
    uint32_t rarityMask = (1 << (uint32_t)Rarity::Magic) | (1 << (uint32_t)Rarity::Rare);
    uint32_t categoryMask = 0xFFFFFFFF; // All types
    bool keepIfTier6Plus = true;
    bool excludeLocked = true;
};

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
    static void OpenMergePanel();

private:
    static void DrawCraftingPanel(entt::registry& registry);
    static void DrawAffixList(entt::registry& registry, entt::entity item, float startX, float startY);
    

    // Forging State
    static inline entt::entity m_forgeItem = entt::null;
    
    // Merging State
    static inline entt::entity m_mergeBase = entt::null;
    static inline entt::entity m_mergeFodder = entt::null;
    static inline entt::entity m_mergeCatalyst = entt::null;
    static inline int m_selectedAffixIndex = -1;

    // Salvaging State
    static inline entt::entity m_salvageItem = entt::null;
    
    static inline SalvageFilter m_salvageFilter;
    static inline bool m_showSalvageFilter = false;
    
    enum class CraftingTab { Forging, Merging, Salvaging };
    static inline CraftingTab m_currentTab = CraftingTab::Forging;

    static inline float m_craftingAlpha = 0.0f;
    static inline bool m_visible = false;
    
    static void DrawMergePanel(entt::registry& registry, float startX, float startY, float panelW, float panelH, float alpha);
    static void DrawSalvagePanel(entt::registry& registry, float startX, float startY, float panelW, float panelH, float alpha);
};

}
