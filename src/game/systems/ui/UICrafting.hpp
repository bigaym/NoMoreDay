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
    
    static inline entt::entity m_targetItem = entt::null;
    static inline float m_craftingAlpha = 0.0f;
    static inline bool m_visible = false;
};

}
