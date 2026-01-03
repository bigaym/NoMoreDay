#include "UIInventory.hpp"
#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/ItemComponent.hpp"
#include "../components/PlayerState.hpp"
#include "../components/EquipmentComponent.hpp" // ADDED THIS LINE
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
    float dt = GetFrameTime();
    float alphaSpeed = 6.0f;
    if (UISystem::State.showInventory) {
        UISystem::State.inventoryAlpha = std::min(1.0f, UISystem::State.inventoryAlpha + dt * alphaSpeed);
    } else {
        UISystem::State.inventoryAlpha = std::max(0.0f, UISystem::State.inventoryAlpha - dt * alphaSpeed);
    }
}

void UIInventory::Draw(entt::registry& registry) {
    auto playerView = registry.view<PlayerTag>();
    if (playerView.begin() == playerView.end()) return;
    auto player = playerView.front();

    auto* inv = registry.try_get<InventoryComponent>(player);
    auto* equip = registry.try_get<EquipmentComponent>(player);

    if (!inv) return;

    // Use Reference Resolution for Layout
    const float panelW = 900.0f; 
    const float panelH = 700.0f;
    const float panelX = (UI_REF_WIDTH - panelW) / 2.0f;
    const float panelY = (UI_REF_HEIGHT - panelH) / 2.0f;
    const float padding = 30.0f;

    // Use Logic Mouse Position
    Vector2 mousePos = UISystem::GetMousePositionLogic();
    float alpha = UISystem::State.inventoryAlpha;

    // Check if mouse is over UI Panel (Background)
    if (CheckCollisionPointRec(mousePos, {panelX, panelY, panelW, panelH})) {
        UISystem::State.isMouseOverUI = true;
    }

    // Scale helper
    float scale = UIRenderer::GetScale();
    auto& theme = UIRenderer::GetTheme();
    Font font = UISystem::GetFont();
    
    // Helper to apply alpha correctly (Multiply alpha, instead of overwrite)
    auto ApplyAlpha = [&](Color c, float a) -> Color {
        return { c.r, c.g, c.b, (unsigned char)((float)c.a * a) };
    };

    auto DrawRectScaled = [&](float x, float y, float w, float h, Color c) {
        DrawRectangle((int)(x*scale), (int)(y*scale), (int)(w*scale), (int)(h*scale), ApplyAlpha(c, alpha));
    };
    
    auto DrawRectLinesScaled = [&](Rectangle rec, float thick, Color c) {
        DrawRectangleLinesEx({rec.x*scale, rec.y*scale, rec.width*scale, rec.height*scale}, thick*scale, ApplyAlpha(c, alpha));
    };
    
    auto DrawLineScaled = [&](Vector2 start, Vector2 end, float thick, Color c) {
        DrawLineEx({start.x*scale, start.y*scale}, {end.x*scale, end.y*scale}, thick*scale, ApplyAlpha(c, alpha));
    };

    // 背景 (Background)
    DrawRectScaled(panelX, panelY, panelW, panelH, theme.panelBackground);
    DrawRectLinesScaled({panelX, panelY, panelW, panelH}, 1.0f, theme.panelBorder);
    DrawLineScaled({panelX, panelY}, {panelX + panelW, panelY}, 2.0f, theme.panelBorderHighlight);

    // 标题 (Header)
    UIRenderer::DrawTextUI(font, "角色物品栏 & 装备", panelX + padding, panelY + 20, 28, theme.textHighlight, alpha);
    UIRenderer::DrawTextUI(font, "按 'I' 或 'ESC' 关闭", panelX + panelW - 200, panelY + 28, 18, theme.textSecondary, alpha);

    // --- 装备区 (Equipment Area) ---
    float equipX = panelX + padding;
    float equipY = panelY + 80.0f;
    float equipW = 340.0f;
    float equipH = panelH - 110.0f;
    
    // Note: Fade overwrites alpha, so we manually apply our factor if we want to combine.
    // However, slotBackground has alpha 200. We want 0.5 * 200 * alpha.
    Color equipBg = theme.slotBackground;
    equipBg.a = (unsigned char)(equipBg.a * 0.5f);
    DrawRectangleRounded({equipX*scale, equipY*scale, equipW*scale, equipH*scale}, 0.02f, 4, ApplyAlpha(equipBg, alpha));
    DrawRectangleRoundedLinesEx({equipX*scale, equipY*scale, equipW*scale, equipH*scale}, 0.02f, 4, 1.0f*scale, ApplyAlpha(theme.panelBorder, alpha));
    
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
        float x = startX + (i % 2) * (equipSlotSize + 110.0f);
        float y = startY + (i / 2) * (equipSlotSize + slotGap);
        
        EquipmentSlot slotType = slotDefs[i].slot;
        entt::entity item = (equip) ? equip->get(slotType) : entt::null;
        bool isHovered = CheckCollisionPointRec(mousePos, {x, y, equipSlotSize, equipSlotSize});

        // Animation
        auto& anim = UISystem::State.equipmentSlotAnims[i];
        float target = isHovered ? 1.0f : 0.0f;
        anim.hoverValue += (target - anim.hoverValue) * 15.0f * dt;
        float slotScale = 1.0f + anim.hoverValue * 0.08f;
        float offset = (equipSlotSize * (slotScale - 1.0f)) / 2.0f;

        if (isHovered && item != entt::null && UISystem::State.draggedItem == entt::null) {
            UISystem::State.hoveredItem = item;
        }

        // Quick Unequip
        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && IsKeyDown(KEY_LEFT_SHIFT) && item != entt::null) {
            if (!InventorySystem::unequipItem(registry, player, slotType)) {
                UISystem::State.showMessageBox = true;
                snprintf(UISystem::State.messageBoxText, 64, "背包已满");
                UISystem::State.messageBoxTimer = 1.5f;
            }
        }
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
            // 允许匹配的槽位，或者通用的 Ring 放入 Ring1/Ring2
            if (dragItemComp && (dragItemComp->slot == slotType || (dragItemComp->slot == EquipmentSlot::Ring && (slotType == EquipmentSlot::Ring1 || slotType == EquipmentSlot::Ring2)))) {
                if (UISystem::State.isDraggingFromInventory && inv) {
                    entt::entity oldEquip = equip->get(slotType);
                    equip->set(slotType, UISystem::State.draggedItem);
                    inv->items[UISystem::State.dragSourceInventoryIndex] = oldEquip;
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

    // --- 背包区 (Inventory Area) ---
    float invX = panelX + 420.0f;
    float invY = panelY + 80.0f;
    float invW = panelW - (invX - panelX) - padding;
    float invH = panelH - 220.0f; // Viewport height
    float invSlotSize = 48.0f;
    float invSlotGap = 5.0f;

    UIRenderer::DrawTextUI(font, "物品背包", invX + 5, invY - 25, 22, theme.textHighlight, alpha);
    
    Color invBg = theme.slotBackground;
    invBg.a = (unsigned char)(invBg.a * 0.3f);
    DrawRectangleRounded({invX*scale, invY*scale, invW*scale, invH*scale}, 0.02f, 4, ApplyAlpha(invBg, alpha));
    DrawRectangleRoundedLinesEx({invX*scale, invY*scale, invW*scale, invH*scale}, 0.02f, 4, 1.0f*scale, ApplyAlpha(theme.panelBorder, alpha));

    // 同步 items 向量大小，确保所有拾取的物品（如药水）都能被遍历到
    if ((int)inv->items.size() < inv->capacity) {
        inv->items.resize(inv->capacity, entt::null);
    }

    // Scroll Logic
    const int cols = 8;
    int totalCapacity = inv->capacity;
    // 渲染时考虑实际 items 大小，防止 push_back 的物品丢失
    int renderCount = std::max(totalCapacity, (int)inv->items.size());
    int totalRows = (renderCount + cols - 1) / cols;
    float contentHeight = totalRows * (invSlotSize + invSlotGap) + 20.0f;
    
    // Mouse wheel scrolling
    if (CheckCollisionPointRec(mousePos, {invX, invY, invW, invH})) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            inv->scrollOffset -= wheel * (invSlotSize + invSlotGap) * 2.0f;
        }
    }
    
    // Clamp scroll offset
    float maxScroll = contentHeight - invH;
    if (maxScroll < 0) maxScroll = 0;
    if (inv->scrollOffset < 0) inv->scrollOffset = 0;
    if (inv->scrollOffset > maxScroll) inv->scrollOffset = maxScroll;

    BeginScissorMode((int)(invX * scale), (int)(invY * scale), (int)(invW * scale), (int)(invH * scale));

    float gridStartX = invX + 15.0f;
    float gridStartY = invY + 15.0f - inv->scrollOffset;

    for (int i = 0; i < renderCount; ++i) {
        int r = i / cols;
        int c = i % cols;
        float x = gridStartX + c * (invSlotSize + invSlotGap);
        float y = gridStartY + r * (invSlotSize + invSlotGap);

        if (y + invSlotSize < invY || y > invY + invH) continue;

        entt::entity item = (i < (int)inv->items.size()) ? inv->items[i] : entt::null;
        bool isHovered = CheckCollisionPointRec(mousePos, {x, y, invSlotSize, invSlotSize}) && CheckCollisionPointRec(mousePos, {invX, invY, invW, invH});

        if (isHovered && item != entt::null && UISystem::State.draggedItem == entt::null) {
            UISystem::State.hoveredItem = item;
        }

        // Drag Start
        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
            UISystem::State.draggedItem = item;
            UISystem::State.isDraggingFromInventory = true;
            UISystem::State.dragSourceInventoryIndex = i;
            UISystem::State.dragSourceEquipmentSlot = EquipmentSlot::None;
            UISystem::State.dragSourceBagSlotIndex = -1;
        }
        
        // Right Click
        if (isHovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && item != entt::null) {
            UISystem::OpenContextMenu(item, true, i, EquipmentSlot::None);
        }

        // Drag Drop
        if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
             if (UISystem::State.isDraggingFromInventory) {
                 std::swap(inv->items[UISystem::State.dragSourceInventoryIndex], inv->items[i]);
             } else {
                 // From equipment or bag slot to inventory
                 if (equip && UISystem::State.dragSourceEquipmentSlot != EquipmentSlot::None) {
                     equip->set(UISystem::State.dragSourceEquipmentSlot, entt::null);
                 } else if (UISystem::State.dragSourceBagSlotIndex != -1) {
                     // 修复背包复制问题：拖离槽位时调用 unequipBag(..., false)，由 UI 负责放置到 items[i]
                     InventorySystem::unequipBag(registry, player, UISystem::State.dragSourceBagSlotIndex, false);
                 }
                 
                 if (inv->items[i] == entt::null) {
                     inv->items[i] = UISystem::State.draggedItem;
                 } else {
                     bool placed = false;
                     for (auto& s : inv->items) { if(s == entt::null) { s = UISystem::State.draggedItem; placed = true; break; } }
                     if (!placed) inv->items.push_back(UISystem::State.draggedItem);
                 }

                 registry.get_or_emplace<StatsDirty>(player);
             }
             UISystem::State.draggedItem = entt::null;
        }

        UIRenderer::DrawSlot(font, registry, x, y, invSlotSize, (UISystem::State.draggedItem == item) ? entt::null : item, nullptr, isHovered, false, alpha);
    }

    EndScissorMode();

    // Scrollbar
    if (maxScroll > 0) {
        float scrollbarW = 6.0f;
        float scrollbarX = invX + invW - scrollbarW - 5.0f;
        float thumbH = (invH / contentHeight) * invH;
        float thumbY = invY + (inv->scrollOffset / maxScroll) * (invH - thumbH);
        DrawRectScaled(scrollbarX, invY, scrollbarW, invH, theme.slotBackground);
        DrawRectScaled(scrollbarX, thumbY, scrollbarW, thumbH, theme.panelBorderHighlight);
    }

    // --- 底部控制 & 背包扩展槽 ---
    float bottomY = invY + invH + 20.0f;
    UIRenderer::DrawTextUI(font, TextFormat("金币: %d", inv->gold), invX + 5, bottomY, 20, theme.textHighlight, alpha);

    // 整理按钮
    Rectangle sortBtnRec = {invX + invW - 120, bottomY - 5, 100, 32};
    bool sortHover = CheckCollisionPointRec(mousePos, sortBtnRec);
    DrawRectScaled(sortBtnRec.x, sortBtnRec.y, sortBtnRec.width, sortBtnRec.height, sortHover ? theme.buttonHover : theme.buttonNormal);
    DrawRectLinesScaled(sortBtnRec, 1.0f, theme.panelBorder);
    UIRenderer::DrawTextUI(font, "整理背包", sortBtnRec.x + 15, sortBtnRec.y + 6, 18, theme.textPrimary, alpha);
    if (sortHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        InventorySystem::organize(registry, player);
    }

    // 背包扩展槽 (Bag Slots)
    float bagSlotsY = bottomY + 50.0f;
    float bagSlotSize = 48.0f;
    UIRenderer::DrawTextUI(font, "背包栏 (增加容量)", invX + 5, bagSlotsY - 25, 18, theme.textSecondary, alpha);

    for (int i = 0; i < 4; ++i) {
        float x = invX + 5.0f + i * (bagSlotSize + 15.0f);
        float y = bagSlotsY;
        entt::entity bagItem = (i < (int)inv->bag_slots.size()) ? inv->bag_slots[i] : entt::null;
        bool isHovered = CheckCollisionPointRec(mousePos, {x, y, bagSlotSize, bagSlotSize});

        if (isHovered && bagItem != entt::null && UISystem::State.draggedItem == entt::null) {
            UISystem::State.hoveredItem = bagItem;
        }

        // Drag Start from Bag Slot
        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && bagItem != entt::null) {
            UISystem::State.draggedItem = bagItem;
            UISystem::State.isDraggingFromInventory = false;
            UISystem::State.dragSourceBagSlotIndex = i;
            UISystem::State.dragSourceEquipmentSlot = EquipmentSlot::None;
        }

        // Drop into Bag Slot
        if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
            auto* dragItemComp = registry.try_get<ItemComponent>(UISystem::State.draggedItem);
            if (dragItemComp && dragItemComp->type == ItemType::Bag) {
                // 如果是从另一个背包槽位拖过来的，先解除旧位置的引用
                if (UISystem::State.dragSourceBagSlotIndex != -1 && UISystem::State.dragSourceBagSlotIndex != i) {
                     // 调用 unequipBag 但不塞回物品栏，因为下一步要放入新槽位
                     InventorySystem::unequipBag(registry, player, UISystem::State.dragSourceBagSlotIndex, false);
                }

                if (InventorySystem::equipBag(registry, player, UISystem::State.draggedItem, i)) {
                    UISystem::State.draggedItem = entt::null;
                } else {
                    LOG_WARN("UIInventory: 无法装备背包，槽位可能已被占用或物品无效。");
                }
            }
        }

        UIRenderer::DrawSlot(font, registry, x, y, bagSlotSize, (UISystem::State.draggedItem == bagItem) ? entt::null : bagItem, "扩展", isHovered, false, alpha);
    }
}
