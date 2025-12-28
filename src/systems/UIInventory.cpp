#include "UIInventory.hpp"
#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/ItemComponent.hpp"
#include "../components/PlayerState.hpp"
#include "InventorySystem.hpp"
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
    // 逻辑更新可以在这里处理，目前主要由 InputSystem 和 UISystem::Update 处理快捷键
}

void UIInventory::Draw(entt::registry& registry) {
    auto playerView = registry.view<PlayerTag>();
    if (playerView.begin() == playerView.end()) return;
    auto player = playerView.front();

    auto* inv = registry.try_get<InventoryComponent>(player);
    auto* equip = registry.try_get<EquipmentComponent>(player);

    if (!inv) return;

    const float panelW = 840.0f;
    const float panelH = 650.0f;
    const float panelX = ((float)GetScreenWidth() - panelW) / 2.0f;
    const float panelY = ((float)GetScreenHeight() - panelH) / 2.0f;
    const float padding = 20.0f;

    // 背景
    DrawRectangle(panelX, panelY, panelW, panelH, Fade(BLACK, 0.92f));
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 3.0f, DARKGRAY);
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 1.0f, GOLD);

    // 标题
    DrawRectangle(panelX, panelY, panelW, 40, Fade(GRAY, 0.2f));
    UISystem::DrawTextUI("角色物品栏 & 装备", panelX + padding, panelY + 8, 24, GOLD);
    UISystem::DrawTextUI("按 'I' 或 'ESC' 关闭", panelX + panelW - 180, panelY + 12, 16, LIGHTGRAY);

    // --- 装备区 ---
    float equipX = panelX + padding;
    float equipY = panelY + 60.0f;
    float equipW = 320.0f;
    
    DrawRectangleRounded({equipX, equipY, equipW, panelH - 80}, 0.05f, 4, Fade(WHITE, 0.05f));
    UISystem::DrawTextUI("装备槽位", equipX + 10, equipY + 10, 20, YELLOW);

    struct SlotDef { const char* label; EquipmentSlot slot; };
    static const SlotDef slotDefs[] = {
        {"头盔", EquipmentSlot::Head}, {"护肩", EquipmentSlot::Shoulder},
        {"胸甲", EquipmentSlot::Chest}, {"手套", EquipmentSlot::Hands},
        {"护腿", EquipmentSlot::Legs}, {"靴子", EquipmentSlot::Feet},
        {"项链", EquipmentSlot::Neck}, {"戒指 1", EquipmentSlot::Ring1},
        {"戒指 2", EquipmentSlot::Ring2}, {"主手武器", EquipmentSlot::MainHand},
        {"副手武器", EquipmentSlot::OffHand}
    };

    float equipSlotSize = 50.0f;
    float slotGap = 12.0f;
    float startX = equipX + 20.0f;
    float startY = equipY + 45.0f;

    for (int i = 0; i < 11; ++i) {
        float x = startX + (i % 2) * (equipSlotSize + 80.0f);
        float y = startY + (i / 2) * (equipSlotSize + slotGap);
        
        EquipmentSlot slotType = slotDefs[i].slot;
        entt::entity item = (equip) ? equip->get(slotType) : entt::null;
        bool isHovered = CheckCollisionPointRec(GetMousePosition(), {x, y, equipSlotSize, equipSlotSize});

        // 交互逻辑
        if (isHovered && item != entt::null && UISystem::State.draggedItem == entt::null) {
            UISystem::State.hoveredItem = item;
        }

        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
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

        UISystem::DrawSlot(registry, x, y, equipSlotSize, (UISystem::State.draggedItem == item) ? entt::null : item, slotDefs[i].label, isHovered);
        UISystem::DrawTextUI(slotDefs[i].label, x + equipSlotSize + 5, y + equipSlotSize/2 - 8, 14, isHovered ? WHITE : GRAY);
    }

    // 分割线
    DrawLineEx({panelX + 355, panelY + 60}, {panelX + 355, panelY + panelH - padding}, 2.0f, DARKGRAY);

    // --- 背包区 ---
    float invX = panelX + 375.0f;
    float invY = panelY + 60.0f;
    float invW = panelW - (invX - panelX) - padding;
    float invSlotSize = 44.0f;

    // 标签页 (简化)
    UISystem::DrawTextUI("物品", invX + 15, invY + 5, 18, WHITE);

    DrawRectangleRounded({invX, invY + 30, invW, panelH - 110}, 0.05f, 4, Fade(WHITE, 0.05f));

    int totalCapacity = inv ? inv->capacity : 20;
    if (inv && inv->items.size() < (size_t)totalCapacity) inv->items.resize(totalCapacity, entt::null);

    const int cols = 7;
    const int rows = 8;
    const int pageSize = cols * rows;
    int totalPages = (totalCapacity + pageSize - 1) / pageSize;
    if (totalPages < 1) totalPages = 1;
    if (m_inventoryPage >= totalPages) m_inventoryPage = totalPages - 1;

    float gridStartX = invX + 15.0f;
    float gridStartY = invY + 45.0f;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int index = r * cols + c;
            int overallIndex = m_inventoryPage * pageSize + index;
            float x = gridStartX + c * (invSlotSize + 5.0f);
            float y = gridStartY + r * (invSlotSize + 5.0f);
            
            entt::entity item = (inv && overallIndex < (int)inv->items.size()) ? inv->items[overallIndex] : entt::null;
            bool isHovered = CheckCollisionPointRec(GetMousePosition(), {x, y, invSlotSize, invSlotSize});

            if (isHovered && item != entt::null && UISystem::State.draggedItem == entt::null) {
                UISystem::State.hoveredItem = item;
            }

            if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
                UISystem::State.draggedItem = item;
                UISystem::State.isDraggingFromInventory = true;
                UISystem::State.dragSourceInventoryIndex = overallIndex;
                // 重置其他拖拽源状态，防止逻辑冲突
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
                    // 处理从装备栏或背包栏拖拽到物品栏的情况
                    if (equip && UISystem::State.dragSourceEquipmentSlot != EquipmentSlot::None) {
                        equip->set(UISystem::State.dragSourceEquipmentSlot, entt::null);
                    } else if (UISystem::State.dragSourceBagSlotIndex != -1) {
                        // 如果是从背包槽位拖拽，先移除背包并重新计算容量
                        inv->bag_slots[UISystem::State.dragSourceBagSlotIndex] = entt::null;
                        InventorySystem::recalculateCapacity(registry, player);
                    }

                    // 尝试放置物品
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

            bool isLocked = overallIndex >= totalCapacity;
            UISystem::DrawSlot(registry, x, y, invSlotSize, (UISystem::State.draggedItem == item) ? entt::null : item, nullptr, isHovered && !isLocked, isLocked);
        }
    }

    // 底部控制栏
    float bottomBarY = invY + panelH - 50.0f;
    
    // 分页
    if (totalPages > 1) {
        const char* pageText = TextFormat("第 %d / %d 页", m_inventoryPage + 1, totalPages);
        UISystem::DrawTextUI(pageText, invX + invW / 2 - 40, bottomBarY + 5, 18, WHITE);
        // 简化按钮逻辑，实际应复用 DrawBtn
        if (m_inventoryPage > 0) {
            DrawRectangle(invX + invW/2 - 80, bottomBarY, 30, 30, DARKGRAY);
            UISystem::DrawTextUI("<", invX + invW/2 - 70, bottomBarY + 5, 18, WHITE);
            if (CheckCollisionPointRec(GetMousePosition(), {invX + invW/2 - 80, bottomBarY, 30, 30}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) m_inventoryPage--;
        }
        if (m_inventoryPage < totalPages - 1) {
            DrawRectangle(invX + invW/2 + 50, bottomBarY, 30, 30, DARKGRAY);
            UISystem::DrawTextUI(">", invX + invW/2 + 60, bottomBarY + 5, 18, WHITE);
            if (CheckCollisionPointRec(GetMousePosition(), {invX + invW/2 + 50, bottomBarY, 30, 30}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) m_inventoryPage++;
        }
    }

    // 金币
    UISystem::DrawTextUI(TextFormat("金币: %d", inv->gold), invX + 15, bottomBarY + 5, 20, GOLD);

    // 整理按钮
    Rectangle sortBtnRec = {invX + invW - 115, bottomBarY, 100, 30};
    bool sortHover = CheckCollisionPointRec(GetMousePosition(), sortBtnRec);
    DrawRectangleRec(sortBtnRec, sortHover ? GOLD : DARKGRAY);
    DrawRectangleLinesEx(sortBtnRec, 1.0f, WHITE);
    UISystem::DrawTextUI("整理背包", sortBtnRec.x + 15, sortBtnRec.y + 5, 18, WHITE);
    if (sortHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        InventorySystem::organize(registry, player);
    }

    // 背包扩展槽 (Bag Slots)
    float bagSlotsY = invY + 500.0f;
    float bagSlotSize = 38.0f;
    UISystem::DrawTextUI("背包栏", invX + 15, bagSlotsY - 20, 16, YELLOW);

    for (int i = 0; i < 4; ++i) {
        float x = invX + 15.0f + i * (bagSlotSize + 10.0f);
        float y = bagSlotsY;
        entt::entity bagItem = (i < inv->bag_slots.size()) ? inv->bag_slots[i] : entt::null;
        bool isHovered = CheckCollisionPointRec(GetMousePosition(), {x, y, bagSlotSize, bagSlotSize});

        if (isHovered && bagItem != entt::null && UISystem::State.draggedItem == entt::null) {
            UISystem::State.hoveredItem = bagItem;
        }

        // 点击切换页面
        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && bagItem != entt::null) {
            // 简单逻辑：每20格一页，计算该背包起始页
            // 实际应根据 BagRange 计算
        }

        // 拖出背包
        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && bagItem != entt::null && UISystem::State.draggedItem == entt::null) {
             // 检查背包是否为空
             bool hasItems = false;
             int startIdx = 56; // 基础容量 (需与 InventorySystem::recalculateCapacity 保持一致)
             
             // 计算当前背包的起始索引
             for (int k = 0; k < i; ++k) {
                 if (k < (int)inv->bag_slots.size() && registry.valid(inv->bag_slots[k])) {
                     if (auto* b = registry.try_get<ItemComponent>(inv->bag_slots[k])) {
                         startIdx += b->bagCapacity;
                     }
                 }
             }
             
             // 检查该背包范围内的物品
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
                 // 重置其他拖拽源状态，防止逻辑冲突 (修复背包复制 Bug)
                 UISystem::State.dragSourceEquipmentSlot = EquipmentSlot::None;
                 UISystem::State.dragSourceInventoryIndex = -1;
             }
        }

        // 放入背包
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

        UISystem::DrawSlot(registry, x, y, bagSlotSize, (UISystem::State.draggedItem == bagItem) ? entt::null : bagItem, "背包", isHovered, false);
    }
}