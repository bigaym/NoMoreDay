#include "game/systems/ui/UIInventory.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/EquipmentComponent.hpp" // ADDED THIS LINE
#include "game/components/MaterialBankComponent.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/systems/item/CraftingSystem.hpp"
#include "game/systems/item/RunewordSystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "raylib.h"
#include <algorithm>
#include <vector>
#include <cstring>
#include <cctype>

using namespace NoMoreDay;

int UIInventory::m_inventoryPage = 0;
int UIInventory::m_activeTab = 0;
float UIInventory::m_materialScrollOffset = 0.0f;
char UIInventory::m_searchBuffer[64] = "";
std::string UIInventory::m_selectedCategory = "";
bool UIInventory::m_isSearchFocused = false;

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
    // Use Reference Resolution for Layout
    const float panelW = 900.0f; 
    const float panelH = 700.0f;
    
    // Calculate centered position initially, updated by drag system
    float panelX = (UI_REF_WIDTH - panelW) / 2.0f;
    float panelY = (UI_REF_HEIGHT - panelH) / 2.0f;
    
    // Enable Dragging (Header Height ~60px)
    UISystem::UpdatePanelDrag(UIPanelID::Inventory, panelX, panelY, panelW, panelH, 60.0f);
    
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


        // Socketing Logic (Drag Rune -> Equipment)
        bool handledDrop = false;
        if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
             auto* dragItem = registry.try_get<ItemComponent>(UISystem::State.draggedItem);
             auto* targetItem = (item != entt::null) ? registry.try_get<ItemComponent>(item) : nullptr;
             
             if (dragItem && targetItem && RunewordSystem::isRune(dragItem->id)) {
                 // Check available socket
                 int freeSocketIdx = -1;
                 if (targetItem->sockets.size() < (size_t)targetItem->socketCount) {
                     freeSocketIdx = (int)targetItem->sockets.size();
                 } else {
                     for(size_t s=0; s<targetItem->sockets.size(); ++s) {
                         if (targetItem->sockets[s] == entt::null) {
                             freeSocketIdx = (int)s;
                             break;
                         }
                     }
                 }

                 if (freeSocketIdx != -1) {
                     // Perform Socketing
                     // Logic: If stack > 1, decrement and create new single rune.
                     entt::entity runeToSocket = UISystem::State.draggedItem;
                     bool wasSplit = false;

                     if (dragItem->quantity > 1) {
                         dragItem->quantity--;
                         // Create a single rune copy
                         runeToSocket = ItemFactory::createMaterial(registry, dragItem->id, 1);
                         wasSplit = true;
                     }

                     if (CraftingSystem::socketRune(registry, item, runeToSocket, freeSocketIdx) == CraftingResult::Success) {
                         LOG_INFO("UI: Successfully socketed rune into equipment.");
                         registry.get_or_emplace<StatsDirty>(player); // Notify stats system
                         handledDrop = true;
                         // If we split, we keep dragging the original stack? 
                         // Typically usually stop dragging if we just did an action.
                         // But if we split, the original entity is still in our hand (draggedItem).
                         // We just reduced its quantity. 
                         // To prevent "losing" the dragged visual if we just dropped one, we might keep dragging if quantity > 0.
                         // OR, we just drop one and stop dragging. Let's stop dragging for safety.
                         
                         // If we didn't split (used the last one), the dragged item is now inside the socket.
                         if (!wasSplit) {
                              UISystem::State.draggedItem = entt::null;
                              // Clean up source slot if needed
                              if (UISystem::State.isDraggingFromInventory) {
                                  inv->items[UISystem::State.dragSourceInventoryIndex] = entt::null;
                              } else if (UISystem::State.dragSourceBagSlotIndex != -1) {
                                  InventorySystem::unequipBag(registry, player, UISystem::State.dragSourceBagSlotIndex, false); // Detach
                              }
                         }
                     } else {
                         // Failed? If split, we must destroy the copy
                         if (wasSplit) registry.destroy(runeToSocket);
                         UISystem::State.showMessageBox = true;
                         snprintf(UISystem::State.messageBoxText, 64, "镶嵌失败");
                         UISystem::State.messageBoxTimer = 1.0f;
                     }
                 } else {
                     UISystem::State.showMessageBox = true;
                     snprintf(UISystem::State.messageBoxText, 64, "没有可用插槽");
                     UISystem::State.messageBoxTimer = 1.0f;
                     handledDrop = true; // Prevent swap logic
                 }
             }
        }

        if (!handledDrop && isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
            // Drop into equipment slot
            if (InventorySystem::equipItem(registry, player, UISystem::State.draggedItem, slotType)) {
                // If it was from ANOTHER equipment slot, we must clear that slot 
                // because equipItem doesn't know about the source slot
                if (!UISystem::State.isDraggingFromInventory && UISystem::State.dragSourceEquipmentSlot != EquipmentSlot::None) {
                    if (UISystem::State.dragSourceEquipmentSlot != slotType) {
                        equip->set(UISystem::State.dragSourceEquipmentSlot, entt::null);
                    }
                }
                UISystem::State.draggedItem = entt::null;
            }
        }

        UIRenderer::DrawSlot(font, registry, x - offset, y - offset, equipSlotSize * slotScale, (UISystem::State.draggedItem == item) ? entt::null : item, slotDefs[i].label, isHovered, false, alpha, slotType);
        UIRenderer::DrawTextUI(font, slotDefs[i].label, x + equipSlotSize + 10, y + equipSlotSize/2 - 8, 16, isHovered ? theme.textPrimary : theme.textSecondary, alpha);
    }

    // --- 背包区 (Inventory Area) ---
    float invX = panelX + 420.0f;
    float invY = panelY + 80.0f;
    float invW = panelW - (invX - panelX) - padding;
    float invH = panelH - 220.0f; // Viewport height

    // Tabs
    float tabW = 80.0f;
    float tabH = 24.0f;
    float tabX = invX;
    float tabY = invY - tabH - 5.0f;

    auto DrawTab = [&](int index, const char* label) {
        float x = tabX + index * (tabW + 5.0f);
        bool isActive = (m_activeTab == index);
        bool isHovered = CheckCollisionPointRec(mousePos, {x, tabY, tabW, tabH});
        
        Color bg = isActive ? theme.textHighlight : theme.slotBackground;
        if (!isActive && isHovered) bg = theme.buttonHover;

        DrawRectScaled(x, tabY, tabW, tabH, bg);
        DrawRectLinesScaled({x, tabY, tabW, tabH}, 1.0f, theme.panelBorder);
        
        Color textColor = isActive ? BLACK : theme.textPrimary;
        UIRenderer::DrawTextUI(font, label, x + 10, tabY + 2, 18, textColor, alpha);

        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            m_activeTab = index;
        }
    };

    DrawTab(0, "物品");
    DrawTab(1, "材料");

    Color invBg = theme.slotBackground;
    invBg.a = (unsigned char)(invBg.a * 0.3f);
    DrawRectangleRounded({invX*scale, invY*scale, invW*scale, invH*scale}, 0.02f, 4, ApplyAlpha(invBg, alpha));
    DrawRectangleRoundedLinesEx({invX*scale, invY*scale, invW*scale, invH*scale}, 0.02f, 4, 1.0f*scale, ApplyAlpha(theme.panelBorder, alpha));

    // Content
    if (m_activeTab == 0) {
        // --- ITEM GRID ---
        float invSlotSize = 48.0f;
        float invSlotGap = 5.0f;

        // 同步 items 向量大小
        if ((int)inv->items.size() < inv->capacity) {
            inv->items.resize(inv->capacity, entt::null);
        }

        // Scroll Logic for Items
        const int cols = 8;
        int totalCapacity = inv->capacity;
        int renderCount = std::max(totalCapacity, (int)inv->items.size());
        int totalRows = (renderCount + cols - 1) / cols;
        float contentHeight = totalRows * (invSlotSize + invSlotGap) + 20.0f;
        
        if (CheckCollisionPointRec(mousePos, {invX, invY, invW, invH})) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) inv->scrollOffset -= wheel * (invSlotSize + invSlotGap) * 2.0f;
        }
        
        float maxScroll = std::max(0.0f, contentHeight - invH);
        inv->scrollOffset = std::clamp(inv->scrollOffset, 0.0f, maxScroll);

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
            // Drag Drop
            bool handledDropInv = false;
            // Socketing Logic (Drag Rune -> Inventory Item)
            if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
                 auto* dragItem = registry.try_get<ItemComponent>(UISystem::State.draggedItem);
                 auto* targetItem = (item != entt::null) ? registry.try_get<ItemComponent>(item) : nullptr;
                 
                 // If dragging a Rune onto a valid item that is NOT itself (prevent self-socketing if that were possible)
                 if (dragItem && targetItem && item != UISystem::State.draggedItem && RunewordSystem::isRune(dragItem->id)) {
                      // Check available socket
                     int freeSocketIdx = -1;
                     if (targetItem->sockets.size() < (size_t)targetItem->socketCount) {
                         freeSocketIdx = (int)targetItem->sockets.size();
                     } else {
                         for(size_t s=0; s<targetItem->sockets.size(); ++s) {
                             if (targetItem->sockets[s] == entt::null) {
                                 freeSocketIdx = (int)s;
                                 break;
                             }
                         }
                     }

                     if (freeSocketIdx != -1) {
                         // Perform Socketing
                         entt::entity runeToSocket = UISystem::State.draggedItem;
                         bool wasSplit = false;

                         if (dragItem->quantity > 1) {
                             dragItem->quantity--;
                             runeToSocket = ItemFactory::createMaterial(registry, dragItem->id, 1);
                             wasSplit = true;
                         }

                         if (CraftingSystem::socketRune(registry, item, runeToSocket, freeSocketIdx) == CraftingResult::Success) {
                             LOG_INFO("UI: Successfully socketed rune into inventory item.");
                             registry.get_or_emplace<StatsDirty>(player); // Notify stats system
                             handledDropInv = true;
                             if (!wasSplit) {
                                  UISystem::State.draggedItem = entt::null;
                                   if (UISystem::State.isDraggingFromInventory && UISystem::State.dragSourceInventoryIndex != -1) {
                                      // If we dragged the LAST rune from inventory list, update the slot to null
                                      // But wait: item is from inv->items[i]. draggedItem is inv->items[dragSource].
                                      // If we used the whole entity, we just need to NULL the source.
                                      // Note: item is target. draggedItem is source.
                                      inv->items[UISystem::State.dragSourceInventoryIndex] = entt::null;
                                  } else {
                                      // Handle external sources if any (equip/bag)
                                       if (equip && UISystem::State.dragSourceEquipmentSlot != EquipmentSlot::None) {
                                           equip->set(UISystem::State.dragSourceEquipmentSlot, entt::null);
                                       } else if (UISystem::State.dragSourceBagSlotIndex != -1) {
                                           InventorySystem::unequipBag(registry, player, UISystem::State.dragSourceBagSlotIndex, true);
                                       }
                                  }
                             }
                         } else {
                             if (wasSplit) registry.destroy(runeToSocket);
                              UISystem::State.showMessageBox = true;
                             snprintf(UISystem::State.messageBoxText, 64, "镶嵌失败");
                             UISystem::State.messageBoxTimer = 1.0f;
                             // We don't set handledDropInv=true here to fallthrough? 
                             // No, if we tried to socket and failed, we shouldn't try swap.
                             handledDropInv = true; 
                         }
                     } else {
                         // Target full? Or just not socketable. 
                         // If not socketable, maybe userINTENDED to swap?
                         // If target has socketCount > 0 but full, message. 
                         // If socketCount == 0, then maybe it's just a swap.
                         if (targetItem->socketCount > 0) {
                              UISystem::State.showMessageBox = true;
                              snprintf(UISystem::State.messageBoxText, 64, "没有可用插槽");
                              UISystem::State.messageBoxTimer = 1.0f;
                              handledDropInv = true;
                         }
                     }
                 }
            }

            if (!handledDropInv && isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
                 if (UISystem::State.isDraggingFromInventory) {
                     std::swap(inv->items[UISystem::State.dragSourceInventoryIndex], inv->items[i]);
                 } else {
                     if (equip && UISystem::State.dragSourceEquipmentSlot != EquipmentSlot::None) {
                         InventorySystem::unequipItem(registry, player, UISystem::State.dragSourceEquipmentSlot);
                     } else if (UISystem::State.dragSourceBagSlotIndex != -1) {
                         InventorySystem::unequipBag(registry, player, UISystem::State.dragSourceBagSlotIndex, true);
                     }
                 }
                 UISystem::State.draggedItem = entt::null;
            }

            UIRenderer::DrawSlot(font, registry, x, y, invSlotSize, (UISystem::State.draggedItem == item) ? entt::null : item, nullptr, isHovered, false, alpha);
        }

        EndScissorMode();

        // Scrollbar Items
        if (maxScroll > 0) {
            float scrollbarW = 6.0f;
            float scrollbarX = invX + invW - scrollbarW - 5.0f;
            float thumbH = (invH / contentHeight) * invH;
            float thumbY = invY + (inv->scrollOffset / maxScroll) * (invH - thumbH);
            DrawRectScaled(scrollbarX, invY, scrollbarW, invH, theme.slotBackground);
            DrawRectScaled(scrollbarX, thumbY, scrollbarW, thumbH, theme.panelBorderHighlight);
        }

    } else if (m_activeTab == 1) {
        // --- MATERIAL LIST ---
        auto* bank = registry.try_get<MaterialBankComponent>(player);
        if (bank) {
            float contentStartX = invX + 10.0f;
            float contentStartY = invY;
            float contentW = invW - 20.0f;
            
            // 1. Search Bar
            float searchH = 28.0f;
            Rectangle searchRect = {contentStartX, contentStartY, 200.0f, searchH};
            bool searchHover = CheckCollisionPointRec(mousePos, searchRect);
            
            if (searchHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                m_isSearchFocused = true;
            } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !searchHover) {
                m_isSearchFocused = false;
            }

            DrawRectScaled(searchRect.x, searchRect.y, searchRect.width, searchRect.height, m_isSearchFocused ? theme.buttonHover : theme.buttonNormal);
            DrawRectLinesScaled(searchRect, 1.0f, m_isSearchFocused ? theme.panelBorderHighlight : theme.panelBorder);
            
            const char* searchText = (strlen(m_searchBuffer) == 0 && !m_isSearchFocused) ? "搜索..." : m_searchBuffer;
            Color searchColor = (strlen(m_searchBuffer) == 0 && !m_isSearchFocused) ? theme.textSecondary : theme.textPrimary;
            UIRenderer::DrawTextUI(font, searchText, searchRect.x + 5, searchRect.y + 4, 18, searchColor, alpha);

            // Input Logic
            if (m_isSearchFocused) {
                int key = GetCharPressed();
                while (key > 0) {
                    if ((key >= 32) && (key <= 125) && (strlen(m_searchBuffer) < 63)) {
                        int len = strlen(m_searchBuffer);
                        m_searchBuffer[len] = (char)key;
                        m_searchBuffer[len+1] = '\0';
                    }
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    int len = strlen(m_searchBuffer);
                    if (len > 0) m_searchBuffer[len-1] = '\0';
                }
            }

            // 2. Categories
            float catX = searchRect.x + searchRect.width + 15.0f;
            float catH = 24.0f;
            const char* categories[] = {"All", "Ore", "Fragment", "Rune"}; // Common categories
            // Or dynamically find? For now static is cleaner.
            
            for (const char* cat : categories) {
                float textW = MeasureTextEx(font, cat, 18, 1).x;
                float btnW = textW + 20.0f;
                Rectangle btnRect = {catX, contentStartY + 2, btnW, catH};
                bool isSelected = (m_selectedCategory == cat) || (std::string(cat) == "All" && m_selectedCategory.empty());
                bool isHover = CheckCollisionPointRec(mousePos, btnRect);

                Color btnBg = isSelected ? theme.textHighlight : theme.buttonNormal;
                if (!isSelected && isHover) btnBg = theme.buttonHover;
                
                DrawRectScaled(btnRect.x, btnRect.y, btnRect.width, btnRect.height, btnBg);
                
                Color txtColor = isSelected ? BLACK : theme.textSecondary;
                UIRenderer::DrawTextUI(font, cat, btnRect.x + 10, btnRect.y + 2, 18, txtColor, alpha);

                if (isHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (std::string(cat) == "All") m_selectedCategory.clear();
                    else m_selectedCategory = cat;
                    m_materialScrollOffset = 0.0f; // Reset scroll
                }
                
                catX += btnW + 5.0f;
            }

            // 3. Filtering
            // Create a filtered list of pointers
            std::vector<const MaterialEntry*> filteredList;
            filteredList.reserve(bank->materials.size());
            
            std::string lowerSearch = m_searchBuffer;
            std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);

            for (const auto& entry : bank->materials) {
                const auto* def = MaterialRegistry::Get().GetMaterial(entry.id);
                if (!def) continue;

                // Category Check
                if (!m_selectedCategory.empty()) {
                    if (def->category != m_selectedCategory) continue;
                }

                // Search Check
                if (!lowerSearch.empty()) {
                    std::string lowerName = def->name;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    if (lowerName.find(lowerSearch) == std::string::npos) continue;
                }

                filteredList.push_back(&entry);
            }

            // 4. Render List
            float listTopY = contentStartY + searchH + 10.0f;
            float listH = invH - (listTopY - invY);
            
            float rowHeight = 40.0f;
            int totalItems = (int)filteredList.size();
            float contentHeight = totalItems * rowHeight + 10.0f;

            if (CheckCollisionPointRec(mousePos, {invX, listTopY, invW, listH})) {
                float wheel = GetMouseWheelMove();
                if (wheel != 0) m_materialScrollOffset -= wheel * rowHeight * 2.0f;
            }
            
            float maxScroll = std::max(0.0f, contentHeight - listH);
            m_materialScrollOffset = std::clamp(m_materialScrollOffset, 0.0f, maxScroll);
            
            BeginScissorMode((int)(invX * scale), (int)(listTopY * scale), (int)(invW * scale), (int)(listH * scale));
            
            float listStartY = listTopY + 5.0f - m_materialScrollOffset;
            
            for (int i = 0; i < totalItems; ++i) {
                float y = listStartY + i * rowHeight;
                if (y + rowHeight < listTopY || y > listTopY + listH) continue;
                
                const auto* entry = filteredList[i]; // Pointer
                const auto* def = MaterialRegistry::Get().GetMaterial(entry->id);
                // def guaranteed not null from filter step
                
                float x = invX + 10.0f;
                float w = invW - 20.0f;
                // Row BG
                bool isRowHovered = CheckCollisionPointRec(mousePos, {x, y, w, rowHeight});
                if (isRowHovered) {
                    DrawRectScaled(x, y, w, rowHeight, ApplyAlpha(theme.buttonHover, alpha * 0.5f));
                }
                
                // Icon Placeholder
                DrawRectLinesScaled({x+2, y+2, 36, 36}, 1.0f, theme.panelBorder);
                
                // Text
                Color nameColor = theme.textPrimary;
                // Rarity Colors?
                if (def) {
                    switch(def->rarity) {
                        case Rarity::Uncommon: nameColor = components::Colors::RARITY_UNCOMMON; break;
                        case Rarity::Rare: nameColor = components::Colors::RARITY_RARE; break;
                        case Rarity::Epic: nameColor = components::Colors::RARITY_EPIC; break;
                        case Rarity::Legendary: nameColor = components::Colors::RARITY_LEGENDARY; break;
                        case Rarity::Magic: nameColor = components::Colors::RARITY_MAGIC; break;
                        case Rarity::Set: nameColor = components::Colors::RARITY_SET; break;
                        case Rarity::Mythic: nameColor = components::Colors::RARITY_MYTHIC; break;
                        case Rarity::Ancient: nameColor = components::Colors::RARITY_ANCIENT; break;
                        default: break;
                    }
                    UIRenderer::DrawTextUI(font, def->name.c_str(), x + 45, y + 10, 20, nameColor, alpha);
                }
                
                // Quantity
                UIRenderer::DrawTextUI(font, TextFormat("x%d", entry->count), x + w - 80, y + 10, 20, theme.textHighlight, alpha);
            }
            
            EndScissorMode();
            
             // Scrollbar Materials
            if (maxScroll > 0) {
                float scrollbarW = 6.0f;
                float scrollbarX = invX + invW - scrollbarW - 5.0f;
                float thumbH = (listH / contentHeight) * listH;
                float thumbY = listTopY + (m_materialScrollOffset / maxScroll) * (listH - thumbH);
                DrawRectScaled(scrollbarX, listTopY, scrollbarW, listH, theme.slotBackground);
                DrawRectScaled(scrollbarX, thumbY, scrollbarW, thumbH, theme.panelBorderHighlight);
            }
        }
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
