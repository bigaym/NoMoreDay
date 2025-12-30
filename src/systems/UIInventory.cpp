#include "UIInventory.hpp"
#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/ItemComponent.hpp"
#include "../components/PlayerState.hpp"
#include "InventorySystem.hpp"
#include "../core/UIRenderer.hpp"
#include "raylib.h"
#include <algorithm>
#include <vector>

using namespace NoMoreDay;

int UIInventory::m_inventoryPage = 0;

bool UIInventory::IsVisible() {
    return UISystem::State.showInventory;
}

void UIInventory::Toggle() {
    UISystem::State.showInventory = !UISystem::State.showInventory;
    if (!UISystem::State.showInventory) {
        m_inventoryPage = 0;
        UISystem::State.showContextMenu = false;
    }
}

void UIInventory::SetPage(int page) {
    m_inventoryPage = page;
}

void UIInventory::Update(entt::registry& registry) {
}

void UIInventory::Draw(entt::registry& registry) {
    auto playerView = registry.view<PlayerTag>();
    if (playerView.begin() == playerView.end()) return;
    auto player = playerView.front();

    auto* inv = registry.try_get<InventoryComponent>(player);
    auto* equip = registry.try_get<EquipmentComponent>(player);

    if (!inv) return;

    // Use Reference Resolution for Layout
    const float panelW = 900.0f; // Wider for better spacing
    const float panelH = 700.0f;
    const float panelX = (UI_REF_WIDTH - panelW) / 2.0f;
    const float panelY = (UI_REF_HEIGHT - panelH) / 2.0f;
    const float padding = 30.0f;

    // Use Logic Mouse Position
    Vector2 mousePos = UISystem::GetMousePositionLogic();
    float alpha = UISystem::State.inventoryAlpha;

    // Scale helper
    float scale = UIRenderer::GetScale();
    auto& theme = UIRenderer::GetTheme();
    Font font = UISystem::GetFont();
    
    auto DrawRectScaled = [&](float x, float y, float w, float h, Color c) {
        DrawRectangle((int)(x*scale), (int)(y*scale), (int)(w*scale), (int)(h*scale), Fade(c, alpha));
    };
    
    auto DrawRectLinesScaled = [&](Rectangle rec, float thick, Color c) {
        DrawRectangleLinesEx({rec.x*scale, rec.y*scale, rec.width*scale, rec.height*scale}, thick*scale, Fade(c, alpha));
    };
    
    auto DrawLineScaled = [&](Vector2 start, Vector2 end, float thick, Color c) {
        DrawLineEx({start.x*scale, start.y*scale}, {end.x*scale, end.y*scale}, thick*scale, Fade(c, alpha));
    };

    // 背景 (Background)
    DrawRectScaled(panelX, panelY, panelW, panelH, theme.panelBackground);
    DrawRectLinesScaled({panelX, panelY, panelW, panelH}, 1.0f, theme.panelBorder);
    // Top border accent
    DrawLineScaled({panelX, panelY}, {panelX + panelW, panelY}, 2.0f, theme.panelBorderHighlight);

    // 标题 (Header)
    UIRenderer::DrawTextUI(font, "角色物品栏 & 装备", panelX + padding, panelY + 20, 28, theme.textHighlight, alpha);
    UIRenderer::DrawTextUI(font, "按 'I' 或 'ESC' 关闭", panelX + panelW - 200, panelY + 28, 18, theme.textSecondary, alpha);

    // --- 装备区 (Equipment Area) ---
    float equipX = panelX + padding;
    float equipY = panelY + 80.0f;
    float equipW = 340.0f;
    float equipH = panelH - 110.0f;
    
    DrawRectangleRounded({equipX*scale, equipY*scale, equipW*scale, equipH*scale}, 0.02f, 4, Fade(theme.slotBackground, 0.5f * alpha));
    DrawRectangleRoundedLines({equipX*scale, equipY*scale, equipW*scale, equipH*scale}, 0.02f, 4, 1.0f*scale, Fade(theme.panelBorder, alpha));
    
    UIRenderer::DrawTextUI(font, "装备槽位", equipX + 15, equipY + 15, 22, theme.textPrimary, alpha);

    struct SlotDef { const char* label; EquipmentSlot slot; };
    static const SlotDef slotDefs[] = {
        {"头盔", EquipmentSlot::Head}, {"护肩", EquipmentSlot::Shoulder},
        {"胸甲", EquipmentSlot::Chest}, {"手套", EquipmentSlot::Hands},
        {"护腿", EquipmentSlot::Legs}, {"靴子", EquipmentSlot::Feet},
        {"项链", EquipmentSlot::Neck}, {"戒指 1", EquipmentSlot::Ring1},
        {"戒指 2", EquipmentSlot::Ring2}, {"主手武器", EquipmentSlot::MainHand},
        {"副手武器", EquipmentSlot::OffHand}
    };

    float equipSlotSize = 56.0f;
    float slotGap = 20.0f;
    float startX = equipX + 30.0f;
    float startY = equipY + 60.0f;

    float dt = GetFrameTime();

    for (int i = 0; i < 11; ++i) {
        float x = startX + (i % 2) * (equipSlotSize + 110.0f); // Wider spacing for labels
        float y = startY + (i / 2) * (equipSlotSize + slotGap);
        
        EquipmentSlot slotType = slotDefs[i].slot;
        entt::entity item = (equip) ? equip->get(slotType) : entt::null;
        
        // Interaction Check in Logic Space
        bool isHovered = CheckCollisionPointRec(mousePos, {x, y, equipSlotSize, equipSlotSize});

        // Update Animation
        auto& anim = UISystem::State.equipmentSlotAnims[i];
        float target = isHovered ? 1.0f : 0.0f;
        anim.hoverValue += (target - anim.hoverValue) * 15.0f * dt;
        float slotScale = 1.0f + anim.hoverValue * 0.08f;
        float offset = (equipSlotSize * (slotScale - 1.0f)) / 2.0f;

        // 交互逻辑
        if (isHovered && item != entt::null && UISystem::State.draggedItem == entt::null) {
            UISystem::State.hoveredItem = item;
        }

        // Quick Unequip (Shift + Click)
        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && IsKeyDown(KEY_LEFT_SHIFT) && item != entt::null) {
            if (!InventorySystem::unequipItem(registry, player, slotType)) {
                UISystem::State.showMessageBox = true;
                snprintf(UISystem::State.messageBoxText, 64, "背包已满");
                UISystem::State.messageBoxTimer = 1.5f;
            }
        }
        // Drag Start
        else if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
            UISystem::State.draggedItem = item;
            UISystem::State.isDraggingFromInventory = false;
            UISystem::State.dragSourceBagSlotIndex = -1;
            UISystem::State.dragSourceEquipmentSlot = slotType;
        }

        if (isHovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && item != entt::null) {
            UISystem::OpenContextMenu(item, false, -1, slotType);
        }

        if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
            auto* dragItemComp = registry.try_get<ItemComponent>(UISystem::State.draggedItem);
            if (dragItemComp && dragItemComp->slot == slotType) {
                if (UISystem::State.isDraggingFromInventory && inv) {
                    entt::entity oldEquip = equip->get(slotType);
                    equip->set(slotType, UISystem::State.draggedItem);
                    inv->items[UISystem::State.dragSourceInventoryIndex] = oldEquip;
                    if (oldEquip == entt::null) {
                        inv->items.erase(inv->items.begin() + UISystem::State.dragSourceInventoryIndex);
                    }
                } else if (!UISystem::State.isDraggingFromInventory) {
                    entt::entity oldEquip = equip->get(slotType);
                    equip->set(slotType, UISystem::State.draggedItem);
                    equip->set(UISystem::State.dragSourceEquipmentSlot, oldEquip);
                }
                UISystem::State.draggedItem = entt::null;
                registry.get_or_emplace<StatsDirty>(player);
            }
        }

        UIRenderer::DrawSlot(font, registry, x - offset, y - offset, equipSlotSize * slotScale, (UISystem::State.draggedItem == item) ? entt::null : item, slotDefs[i].label, isHovered, false, alpha);
        UIRenderer::DrawTextUI(font, slotDefs[i].label, x + equipSlotSize + 10, y + equipSlotSize/2 - 8, 16, isHovered ? theme.textPrimary : theme.textSecondary, alpha);
    }

    // 分割线
    DrawLineScaled({panelX + 400, panelY + 80}, {panelX + 400, panelY + panelH - padding}, 1.0f, theme.panelBorder);

    // --- 背包区 (Inventory Area) ---
    float invX = panelX + 420.0f;
    float invY = panelY + 80.0f;
    float invW = panelW - (invX - panelX) - padding;
    float invSlotSize = 48.0f;

    // 标签页 (简化)
    UIRenderer::DrawTextUI(font, "物品背包", invX + 5, invY - 25, 22, theme.textHighlight, alpha);

    DrawRectangleRounded({invX*scale, invY*scale, invW*scale, (panelH - 150)*scale}, 0.02f, 4, Fade(theme.slotBackground, 0.3f * alpha));
    DrawRectangleRoundedLines({invX*scale, invY*scale, invW*scale, (panelH - 150)*scale}, 0.02f, 4, 1.0f*scale, Fade(theme.panelBorder, alpha));

    int totalCapacity = inv ? inv->capacity : 20;
    if (inv && inv->items.size() < (size_t)totalCapacity) inv->items.resize(totalCapacity, entt::null);

    const int cols = 8; // Increased columns
    const int rows = 8;
    const int pageSize = cols * rows;
    int totalPages = (totalCapacity + pageSize - 1) / pageSize;
    if (totalPages < 1) totalPages = 1;
    if (m_inventoryPage >= totalPages) m_inventoryPage = totalPages - 1;

    float gridStartX = invX + 15.0f;
    float gridStartY = invY + 15.0f;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int index = r * cols + c;
            int overallIndex = m_inventoryPage * pageSize + index;
            float x = gridStartX + c * (invSlotSize + 5.0f);
            float y = gridStartY + r * (invSlotSize + 5.0f);
            
            entt::entity item = (inv && overallIndex < (int)inv->items.size()) ? inv->items[overallIndex] : entt::null;
            bool isHovered = CheckCollisionPointRec(mousePos, {x, y, invSlotSize, invSlotSize});
            bool isLocked = overallIndex >= totalCapacity;

            // Update Animation
            auto& anim = UISystem::State.inventorySlotAnims[index];
            float target = (isHovered && !isLocked) ? 1.0f : 0.0f;
            anim.hoverValue += (target - anim.hoverValue) * 15.0f * dt;
            float slotScale = 1.0f + anim.hoverValue * 0.1f;
            float offset = (invSlotSize * (slotScale - 1.0f)) / 2.0f;

            // Interaction Logic
            if (isHovered && item != entt::null && UISystem::State.draggedItem == entt::null) {
                UISystem::State.hoveredItem = item;
            }

            // Quick Move/Equip (Shift + Click)
            if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && IsKeyDown(KEY_LEFT_SHIFT) && item != entt::null) {
                // Attempt to equip
                if (equip) {
                    if (InventorySystem::equipItem(registry, player, item)) {
                        // Success - sound effect?
                    } else {
                        // Fail - maybe show error?
                        UISystem::State.showMessageBox = true;
                        snprintf(UISystem::State.messageBoxText, 64, "无法装备或槽位不匹配");
                        UISystem::State.messageBoxTimer = 1.5f;
                    }
                }
            }
            // Drag Start
            else if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
                UISystem::State.draggedItem = item;
                UISystem::State.isDraggingFromInventory = true;
                UISystem::State.dragSourceInventoryIndex = overallIndex;
                UISystem::State.dragSourceEquipmentSlot = EquipmentSlot::None;
                UISystem::State.dragSourceBagSlotIndex = -1;
            }

            if (isHovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && item != entt::null) {
                UISystem::OpenContextMenu(item, true, overallIndex, EquipmentSlot::None);
            }

            if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
                if (UISystem::State.isDraggingFromInventory && inv) {
                    if (overallIndex < (int)inv->items.size()) {
                        std::swap(inv->items[UISystem::State.dragSourceInventoryIndex], inv->items[overallIndex]);
                    }
                } else if (!UISystem::State.isDraggingFromInventory && inv) {
                    if (equip && UISystem::State.dragSourceEquipmentSlot != EquipmentSlot::None) {
                        equip->set(UISystem::State.dragSourceEquipmentSlot, entt::null);
                    } else if (UISystem::State.dragSourceBagSlotIndex != -1) {
                        inv->bag_slots[UISystem::State.dragSourceBagSlotIndex] = entt::null;
                        InventorySystem::recalculateCapacity(registry, player);
                    }

                    if (overallIndex < (int)inv->items.size() && inv->items[overallIndex] == entt::null) {
                        inv->items[overallIndex] = UISystem::State.draggedItem;
                    } else {
                        bool placed = false;
                        for (auto& slot : inv->items) {
                            if (slot == entt::null) { slot = UISystem::State.draggedItem; placed = true; break; }
                        }
                        if (!placed) inv->items.push_back(UISystem::State.draggedItem);
                    }
                    registry.get_or_emplace<StatsDirty>(player);
                }
                UISystem::State.draggedItem = entt::null;
            }

            UIRenderer::DrawSlot(font, registry, x - offset, y - offset, invSlotSize * slotScale, (UISystem::State.draggedItem == item) ? entt::null : item, nullptr, isHovered && !isLocked, isLocked, alpha);
        }
    }

    // 底部控制栏 (Bottom Controls)
    float bottomBarY = invY + (panelH - 150.0f) + 15.0f;
    
    // 分页
    if (totalPages > 1) {
        const char* pageText = TextFormat("第 %d / %d 页", m_inventoryPage + 1, totalPages);
        UIRenderer::DrawTextUI(font, pageText, invX + invW / 2 - 40, bottomBarY + 5, 18, theme.textPrimary, alpha);
        
        if (m_inventoryPage > 0) {
            Rectangle btn = {invX + invW/2 - 90, bottomBarY, 32, 32};
            bool h = CheckCollisionPointRec(mousePos, btn);
            DrawRectScaled(btn.x, btn.y, btn.width, btn.height, h ? theme.buttonHover : theme.buttonNormal);
            DrawRectLinesScaled(btn, 1.0f, theme.panelBorder);
            UIRenderer::DrawTextUI(font, "<", btn.x + 10, btn.y + 4, 20, theme.textPrimary, alpha);
            if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) m_inventoryPage--;
        }
        if (m_inventoryPage < totalPages - 1) {
            Rectangle btn = {invX + invW/2 + 60, bottomBarY, 32, 32};
            bool h = CheckCollisionPointRec(mousePos, btn);
            DrawRectScaled(btn.x, btn.y, btn.width, btn.height, h ? theme.buttonHover : theme.buttonNormal);
            DrawRectLinesScaled(btn, 1.0f, theme.panelBorder);
            UIRenderer::DrawTextUI(font, ">", btn.x + 12, btn.y + 4, 20, theme.textPrimary, alpha);
            if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) m_inventoryPage++;
        }
    }

    // 金币
    UIRenderer::DrawTextUI(font, TextFormat("金币: %d", inv->gold), invX + 15, bottomBarY + 5, 20, theme.textHighlight, alpha);

    // 整理按钮
    Rectangle sortBtnRec = {invX + invW - 120, bottomBarY, 100, 32};
    bool sortHover = CheckCollisionPointRec(mousePos, sortBtnRec);
    DrawRectScaled(sortBtnRec.x, sortBtnRec.y, sortBtnRec.width, sortBtnRec.height, sortHover ? theme.buttonHover : theme.buttonNormal);
    DrawRectLinesScaled(sortBtnRec, 1.0f, theme.panelBorder);
    UIRenderer::DrawTextUI(font, "整理背包", sortBtnRec.x + 15, sortBtnRec.y + 6, 18, theme.textPrimary, alpha);
    if (sortHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        InventorySystem::organize(registry, player);
    }

    // 背包扩展槽 (Bag Slots)
    float bagSlotsY = bottomBarY + 50.0f;
    float bagSlotSize = 42.0f;
    UIRenderer::DrawTextUI(font, "背包栏", invX + 15, bagSlotsY - 25, 18, theme.textSecondary, alpha);

    for (int i = 0; i < 4; ++i) {
        float x = invX + 15.0f + i * (bagSlotSize + 15.0f);
        float y = bagSlotsY;
        entt::entity bagItem = (i < inv->bag_slots.size()) ? inv->bag_slots[i] : entt::null;
        bool isHovered = CheckCollisionPointRec(mousePos, {x, y, bagSlotSize, bagSlotSize});

        // Update Animation
        auto& anim = UISystem::State.bagSlotAnims[i];
        float target = isHovered ? 1.0f : 0.0f;
        anim.hoverValue += (target - anim.hoverValue) * 15.0f * dt;
        float slotScale = 1.0f + anim.hoverValue * 0.1f;
        float offset = (bagSlotSize * (slotScale - 1.0f)) / 2.0f;

        if (isHovered && bagItem != entt::null && UISystem::State.draggedItem == entt::null) {
            UISystem::State.hoveredItem = bagItem;
        }

        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && bagItem != entt::null && UISystem::State.draggedItem == entt::null) {
             bool hasItems = false;
             int startIdx = 56;
             
             for (int k = 0; k < i; ++k) {
                 if (k < (int)inv->bag_slots.size() && registry.valid(inv->bag_slots[k])) {
                     if (auto* b = registry.try_get<ItemComponent>(inv->bag_slots[k])) {
                         startIdx += b->bagCapacity;
                     }
                 }
             }
             
             int thisBagCap = 0;
             if (auto* b = registry.try_get<ItemComponent>(bagItem)) thisBagCap = b->bagCapacity;
             
             for (int k = 0; k < thisBagCap; ++k) {
                 int idx = startIdx + k;
                 if (idx < (int)inv->items.size() && registry.valid(inv->items[idx])) {
                     hasItems = true;
                     break;
                 }
             }

             if (hasItems) {
                 LOG_WARN("UIInventory: 无法移除背包，其中包含物品！请先清空该页面。");
             } else {
                 UISystem::State.draggedItem = bagItem;
                 UISystem::State.isDraggingFromInventory = false;
                 UISystem::State.dragSourceBagSlotIndex = i;
                 UISystem::State.dragSourceEquipmentSlot = EquipmentSlot::None;
                 UISystem::State.dragSourceInventoryIndex = -1;
             }
        }

        if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
            auto* dragItemComp = registry.try_get<ItemComponent>(UISystem::State.draggedItem);
            if (dragItemComp && dragItemComp->type == ItemType::Bag) {
                if (UISystem::State.isDraggingFromInventory) {
                    if (InventorySystem::equipBag(registry, player, UISystem::State.draggedItem, i)) {
                        UISystem::State.draggedItem = entt::null;
                    }
                }
            }
        }

        UIRenderer::DrawSlot(font, registry, x - offset, y - offset, bagSlotSize * slotScale, (UISystem::State.draggedItem == bagItem) ? entt::null : bagItem, "背包", isHovered, false, alpha);
    }
}
