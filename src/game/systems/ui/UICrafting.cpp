#include "game/systems/ui/UICrafting.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/item/CraftingSystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/ItemStats.hpp"
#include "core/logging/Logger.hpp"
#include <algorithm>
#include <cmath>

namespace NoMoreDay {

void UICrafting::Toggle() {
    m_visible = !m_visible;
    if (!m_visible) {
        // Return item to inventory if panel is closed? 
        // For now, keep it referenced but maybe clear if entity destroyed.
    }
}

bool UICrafting::IsVisible() {
    return m_visible;
}

void UICrafting::SetTargetItem(entt::entity item) {
    m_targetItem = item;
    m_visible = true; // Auto-open when setting target via context menu
}

entt::entity UICrafting::GetTargetItem() {
    return m_targetItem;
}

void UICrafting::ClearTargetItem() {
    m_targetItem = entt::null;
}

void UICrafting::Update(entt::registry& registry) {
    float dt = GetFrameTime();
    float alphaSpeed = 6.0f;
    if (m_visible) m_craftingAlpha = std::min(1.0f, m_craftingAlpha + dt * alphaSpeed);
    else m_craftingAlpha = std::max(0.0f, m_craftingAlpha - dt * alphaSpeed);

    if (m_targetItem != entt::null && !registry.valid(m_targetItem)) {
        m_targetItem = entt::null;
    }
}

void UICrafting::Draw(entt::registry& registry) {
    if (m_craftingAlpha <= 0.0f) return;

    DrawCraftingPanel(registry);
}

void UICrafting::DrawCraftingPanel(entt::registry& registry) {
    auto& state = UISystem::State;
    float alpha = m_craftingAlpha;
    
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float panelW = 600.0f * state.scaleFactor;
    float panelH = 700.0f * state.scaleFactor;
    float startX = (screenW - panelW) / 2.0f;
    float startY = (screenH - panelH) / 2.0f;

    // Background
    DrawRectangleRec({startX, startY, panelW, panelH}, Fade(Color{30, 30, 40, 255}, 0.95f * alpha));
    DrawRectangleLinesEx({startX, startY, panelW, panelH}, 2.0f, Fade(GOLD, alpha));

    // Title
    UISystem::DrawTextUI("神铸台 (Crafting)", startX + 20, startY + 20, 24, GOLD, alpha);
    
    // Close Button
    if (CheckCollisionPointRec(GetMousePosition(), {startX + panelW - 40, startY + 10, 30, 30})) {
        UISystem::DrawTextUI("X", startX + panelW - 35, startY + 15, 20, RED, alpha);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) Toggle();
    } else {
        UISystem::DrawTextUI("X", startX + panelW - 35, startY + 15, 20, WHITE, alpha);
    }

    // Target Item Slot
    float slotSize = 80.0f * state.scaleFactor;
    float slotX = startX + (panelW - slotSize) / 2.0f;
    float slotY = startY + 80.0f * state.scaleFactor;

    UIRenderer::DrawSlot(state.globalFont, registry, slotX, slotY, slotSize, m_targetItem, "放入装备", false, false, alpha);
    
    // Handle Item Drop
    Rectangle slotRect = {slotX, slotY, slotSize, slotSize};
    if (CheckCollisionPointRec(GetMousePosition(), slotRect)) {
        if (state.draggedItem != entt::null && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            // Check if item is equipment
            if (registry.any_of<ItemComponent>(state.draggedItem)) {
                auto& item = registry.get<ItemComponent>(state.draggedItem);
                if (item.type == ItemType::Weapon || item.type == ItemType::Armor || item.type == ItemType::Jewelry) {
                    m_targetItem = state.draggedItem;
                    state.draggedItem = entt::null; // Consume drag
                }
            }
        }
        // Tooltip
        if (m_targetItem != entt::null) {
            state.hoveredItem = entt::null; // Override other hover
            UIRenderer::DrawTooltip(state.globalFont, registry, m_targetItem, alpha);
        }
    }

    if (m_targetItem != entt::null) {
        auto& item = registry.get<ItemComponent>(m_targetItem);
        
        // Potential Display
        char potBuf[64];
        snprintf(potBuf, 64, "锻造潜力: %d", item.forgingPotential);
        float potW = MeasureTextEx(state.globalFont, potBuf, 20, 1.0f).x;
        UISystem::DrawTextUI(potBuf, startX + (panelW - potW)/2.0f, slotY + slotSize + 10, 20, SKYBLUE, alpha);

        // Draw Affixes List
        DrawAffixList(registry, m_targetItem);
    }
}

void UICrafting::DrawAffixList(entt::registry& registry, entt::entity entity) {
    auto& state = UISystem::State;
    auto& item = registry.get<ItemComponent>(entity);
    float alpha = m_craftingAlpha;
    
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float panelW = 600.0f * state.scaleFactor;
    float panelH = 700.0f * state.scaleFactor;
    float startX = (screenW - panelW) / 2.0f;
    float startY = (screenH - panelH) / 2.0f;
    
    float currentY = startY + 200.0f * state.scaleFactor;
    float rowH = 50.0f * state.scaleFactor;
    float padding = 10.0f * state.scaleFactor;

    // Helper to draw an affix row
    auto DrawAffixRow = [&](Affix* affix, int index, bool isPrefix, int slotIdx) {
        float x = startX + 20 * state.scaleFactor;
        float w = panelW - 40 * state.scaleFactor;
        Rectangle rowRect = {x, currentY, w, rowH};
        
        DrawRectangleRec(rowRect, Fade(DARKGRAY, 0.5f * alpha));
        DrawRectangleLinesEx(rowRect, 1.0f, Fade(GRAY, alpha));
        
        if (affix) {
            // Existing Affix
            Color textColor = WHITE; // Determine by tier?
            char nameBuf[128];
            snprintf(nameBuf, 128, "[T%d] %s (%.1f)", affix->tier, affix->name.c_str(), affix->value);
            UISystem::DrawTextUI(nameBuf, x + 10, currentY + 15, 18, textColor, alpha);
            
            // Upgrade Button
            float btnW = 60.0f * state.scaleFactor;
            float btnH = 30.0f * state.scaleFactor;
            float btnX = x + w - btnW - 10;
            
            bool canAfford = item.forgingPotential > 0;

            if (affix->tier < 5) {
                Rectangle btnRect = {btnX, currentY + 10, btnW, btnH};
                if (canAfford) {
                    bool hover = CheckCollisionPointRec(GetMousePosition(), btnRect);
                    DrawRectangleRec(btnRect, Fade(hover ? GREEN : DARKGREEN, alpha));
                    UISystem::DrawTextUI("升级", btnRect.x + 10, btnRect.y + 5, 16, WHITE, alpha);
                    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        CraftingSystem::upgradeAffix(item, index);
                    }
                } else {
                    DrawRectangleRec(btnRect, Fade(RED, 0.3f * alpha));
                    UISystem::DrawTextUI("潜力", btnRect.x + 10, btnRect.y + 5, 14, GRAY, alpha);
                }
            } else {
                 UISystem::DrawTextUI("MAX", btnX + 10, currentY + 15, 16, GOLD, alpha);
            }
            
            // Chaos (C)
            if (affix->tier < 5) {
                Rectangle cRect = {btnX - 35 * state.scaleFactor, currentY + 10, 30.0f * state.scaleFactor, btnH};
                if (canAfford) {
                    bool hover = CheckCollisionPointRec(GetMousePosition(), cRect);
                    DrawRectangleRec(cRect, Fade(hover ? PURPLE : VIOLET, alpha));
                    UISystem::DrawTextUI("C", cRect.x + 8, cRect.y + 5, 16, WHITE, alpha);
                    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        CraftingSystem::chaosAffix(item, index);
                    }
                    if (hover) {
                        // Tooltip for Chaos
                        // UIRenderer::DrawSimpleTooltip(...) // TODO
                    }
                }
            }

            // Refine (R) - Values
            {
                Rectangle rRect = {btnX - 70 * state.scaleFactor, currentY + 10, 30.0f * state.scaleFactor, btnH};
                if (canAfford) {
                    bool hover = CheckCollisionPointRec(GetMousePosition(), rRect);
                    DrawRectangleRec(rRect, Fade(hover ? SKYBLUE : BLUE, alpha));
                    UISystem::DrawTextUI("R", rRect.x + 8, rRect.y + 5, 16, WHITE, alpha);
                    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        CraftingSystem::refineAffixValues(item, index);
                    }
                }
            }

        } else {
            // Empty Slot
            UISystem::DrawTextUI(isPrefix ? "空前缀槽位" : "空后缀槽位", x + 10, currentY + 15, 18, GRAY, alpha);
            
            // Add Button
            float btnW = 80.0f * state.scaleFactor;
            Rectangle btnRect = {x + w - btnW - 10, currentY + 10, btnW, 30.0f * state.scaleFactor};
             bool canAfford = item.forgingPotential > 0;
             if (canAfford) {
                bool hover = CheckCollisionPointRec(GetMousePosition(), btnRect);
                DrawRectangleRec(btnRect, Fade(hover ? BLUE : DARKBLUE, alpha));
                UISystem::DrawTextUI("添加", btnRect.x + 20, btnRect.y + 5, 16, WHITE, alpha);
                
                if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    // Logic to show shard selection popup...
                    // For prototype, just add a random relevant affix
                    // We need to know type. This is hard without a UI popup.
                    // Let's create a "Add Random" for now.
                    AffixType types[] = { AffixType::Strength, AffixType::Dexterity, AffixType::Intelligence, AffixType::Vitality, AffixType::FlatPhysicalDamage, AffixType::AttackSpeed };
                    AffixType t = types[GetRandomValue(0, 5)];
                    CraftingSystem::addAffix(item, t, isPrefix);
                }
             }
        }
        
        currentY += rowH + padding;
    };

    // Sort affixes into prefixes and suffixes
    std::vector<int> prefixIndices;
    std::vector<int> suffixIndices;
    for(size_t i=0; i<item.affixes.size(); ++i) {
        if (item.affixes[i].isPrefix) prefixIndices.push_back((int)i);
        else suffixIndices.push_back((int)i);
    }

    UISystem::DrawTextUI("前缀 (Prefixes)", startX + 20, currentY, 20, LIGHTGRAY, alpha);
    currentY += 30;
    
    for(int i=0; i<2; ++i) {
        if (i < (int)prefixIndices.size()) {
            DrawAffixRow(&item.affixes[prefixIndices[i]], prefixIndices[i], true, i);
        } else {
            DrawAffixRow(nullptr, -1, true, i);
        }
    }
    
    currentY += 10;
    UISystem::DrawTextUI("后缀 (Suffixes)", startX + 20, currentY, 20, LIGHTGRAY, alpha);
    currentY += 30;
    
    for(int i=0; i<2; ++i) {
        if (i < (int)suffixIndices.size()) {
            DrawAffixRow(&item.affixes[suffixIndices[i]], suffixIndices[i], false, i);
        } else {
            DrawAffixRow(nullptr, -1, false, i);
        }
    }
}

}
