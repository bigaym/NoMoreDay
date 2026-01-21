#include "game/systems/ui/UIStash.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/components/Common.hpp"
#include "game/systems/item/StashSystem.hpp"
#include "engine/render/UIRenderer.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "engine/persistence/SharedStash.hpp" // ADDED
#include "raylib.h"
#include <algorithm>
#include <cstring>
#include <cctype>

using namespace NoMoreDay;
using namespace NoMoreDay::Constants;

// Static member init
bool UIStash::m_isVisible = false;
StashType UIStash::m_activeType = StashType::Personal;
int UIStash::m_activeTabIndex = 0;
float UIStash::m_alpha = 0.0f;
char UIStash::m_searchBuffer[64] = "";
char UIStash::m_lastSearchBuffer[64] = "";
std::vector<std::pair<int, int>> UIStash::m_cachedSearchResults;
bool UIStash::m_isSearchFocused = false;
bool UIStash::m_showUnlockConfirm = false;

bool UIStash::IsVisible() {
    return UISystem::State.showStash;
}

void UIStash::Toggle() {
    if (UISystem::State.showStash) Close();
    else Open(m_activeType);
}

void UIStash::Open(StashType type) {
    UISystem::State.showStash = true;
    UISystem::State.showInventory = true; // Auto open inventory
    m_activeType = type;
    m_isVisible = true;
    m_activeTabIndex = 0; // Reset to first tab
}

void UIStash::Close() {
    UISystem::State.showStash = false;
    m_isVisible = false;
}

void UIStash::Update(entt::registry& registry) {
    float dt = GetFrameTime();
    float alphaSpeed = 6.0f;
    if (UISystem::State.showStash) {
        UISystem::State.stashAlpha = std::min(1.0f, UISystem::State.stashAlpha + dt * alphaSpeed);
    } else {
        UISystem::State.stashAlpha = std::max(0.0f, UISystem::State.stashAlpha - dt * alphaSpeed);
    }
}

void UIStash::Draw(entt::registry& registry) {
    float alpha = UISystem::State.stashAlpha;
    if (alpha <= 0.0f) return;

    // Layout
    const float panelW = 680.0f; 
    const float panelH = 700.0f;
    
    float defaultX = 100.0f;
    float defaultY = (UI_REF_HEIGHT - panelH) / 2.0f;
    
    UISystem::UpdatePanelDrag(UIPanelID::Stash, defaultX, defaultY, panelW, panelH, 60.0f);
    
    float panelX = UISystem::State.panelStates[(int)UIPanelID::Stash].position.x;
    float panelY = UISystem::State.panelStates[(int)UIPanelID::Stash].position.y;
    
    if (panelX < 0) { 
        panelX = defaultX;
        panelY = defaultY;
        UISystem::State.panelStates[(int)UIPanelID::Stash].position = {panelX, panelY};
    }

    Vector2 mousePos = UISystem::GetMousePositionLogic();
    if (CheckCollisionPointRec(mousePos, {panelX, panelY, panelW, panelH})) {
        UISystem::State.isMouseOverUI = true;
    }

    float scale = UIRenderer::GetScale();
    auto& theme = UIRenderer::GetTheme();
    Font font = UISystem::GetFont();

    auto ApplyAlpha = [&](Color c, float a) -> Color {
        return { c.r, c.g, c.b, (unsigned char)((float)c.a * a) };
    };

    auto DrawRectScaled = [&](float x, float y, float w, float h, Color c) {
        DrawRectangle((int)(x*scale), (int)(y*scale), (int)(w*scale), (int)(h*scale), ApplyAlpha(c, alpha));
    };
    
    auto DrawRectLinesScaled = [&](Rectangle rec, float thick, Color c) {
        DrawRectangleLinesEx({rec.x*scale, rec.y*scale, rec.width*scale, rec.height*scale}, thick*scale, ApplyAlpha(c, alpha));
    };

    // Background
    DrawRectScaled(panelX, panelY, panelW, panelH, theme.panelBackground);
    DrawRectLinesScaled({panelX, panelY, panelW, panelH}, 1.0f, theme.panelBorder);
    
    // Header
    const char* title = (m_activeType == StashType::Shared) ? "共享仓库" : "个人仓库";
    UIRenderer::DrawTextUI(font, title, panelX + 30, panelY + 20, 28, theme.textHighlight, alpha);

    // Tabs
    float tabX = panelX + 30.0f;
    float tabY = panelY + 60.0f;
    float tabH = 30.0f;
    
    int unlockedCount = StashSystem::getUnlockedTabCount(registry, m_activeType);
    
    for (int i = 0; i < unlockedCount; ++i) {
        StashTab* tab = StashSystem::getTab(registry, m_activeType, i);
        if (!tab) continue;
        
        float textW = MeasureTextEx(font, tab->name.c_str(), 18, 1).x;
        float tabW = textW + 20.0f;
        
        if (tabX + tabW > panelX + panelW - 60.0f) {
            tabX = panelX + 30.0f;
            tabY += tabH + 5.0f;
        }
        
        bool isActive = (m_activeTabIndex == i);
        bool isHover = CheckCollisionPointRec(mousePos, {tabX, tabY, tabW, tabH});
        
        Color bg = isActive ? theme.textHighlight : theme.slotBackground;
        if (!isActive && isHover) bg = theme.buttonHover;
        
        DrawRectScaled(tabX, tabY, tabW, tabH, bg);
        DrawRectLinesScaled({tabX, tabY, tabW, tabH}, 1.0f, theme.panelBorder);
        
        Color txtColor = isActive ? BLACK : theme.textPrimary;
        UIRenderer::DrawTextUI(font, tab->name.c_str(), tabX + 10, tabY + 6, 18, txtColor, alpha);
        
        if (isHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            m_activeTabIndex = i;
        }
        
        tabX += tabW + 5.0f;
    }
    
    // Unlock Button
    int maxTabs = (m_activeType == StashType::Shared) ? SharedStash::Get().getMaxTabs() : PersonalStashComponent::MAX_TABS;
    if (unlockedCount < maxTabs) {
        float btnW = 30.0f;
        if (tabX + btnW > panelX + panelW - 30.0f) {
            tabX = panelX + 30.0f;
            tabY += tabH + 5.0f;
        }
        
        bool isHover = CheckCollisionPointRec(mousePos, {tabX, tabY, btnW, tabH});
        DrawRectScaled(tabX, tabY, btnW, tabH, isHover ? theme.buttonHover : theme.buttonNormal);
        DrawRectLinesScaled({tabX, tabY, btnW, tabH}, 1.0f, theme.panelBorder);
        UIRenderer::DrawTextUI(font, "+", tabX + 10, tabY + 6, 20, theme.textHighlight, alpha);
        
        if (isHover) {
             int cost = StashSystem::getNextUnlockCost(registry, m_activeType);
             UIRenderer::DrawTextUI(font, TextFormat("解锁费用: %d 金币", cost), mousePos.x + 15, mousePos.y, 18, WHITE, alpha);
             
             if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 if (StashSystem::unlockTab(registry, m_activeType)) {
                     // Success
                 } else {
                     UISystem::State.showMessageBox = true;
                     snprintf(UISystem::State.messageBoxText, 64, "金币不足");
                     UISystem::State.messageBoxTimer = 1.0f;
                 }
             }
        }
    }
    
    // Grid
    float gridY = tabY + tabH + 20.0f;
    float gridX = panelX + 30.0f;
    float slotSize = 48.0f;
    float gap = 4.0f;
    
    // Search Cache Logic
    if (strcmp(m_searchBuffer, m_lastSearchBuffer) != 0) {
        strcpy(m_lastSearchBuffer, m_searchBuffer);
        if (strlen(m_searchBuffer) > 0) {
            m_cachedSearchResults = StashSystem::search(registry, m_activeType, m_searchBuffer);
        } else {
            m_cachedSearchResults.clear();
        }
    }

    StashTab* currentTab = StashSystem::getTab(registry, m_activeType, m_activeTabIndex);
    if (currentTab) {
        for (int i = 0; i < StashTab::CAPACITY; ++i) {
            int r = i / 12; // 12 cols
            int c = i % 12;
            
            float x = gridX + c * (slotSize + gap);
            float y = gridY + r * (slotSize + gap);
            
            entt::entity item = currentTab->items[i];
            bool isHovered = CheckCollisionPointRec(mousePos, {x, y, slotSize, slotSize});
            
            bool isMatch = true;
            if (strlen(m_searchBuffer) > 0) {
                isMatch = false;
                for (const auto& res : m_cachedSearchResults) {
                    if (res.first == m_activeTabIndex && res.second == i) {
                        isMatch = true;
                        break;
                    }
                }
            }

            if (isHovered && item != entt::null && UISystem::State.draggedItem == entt::null) {
                UISystem::State.hoveredItem = item;
            }
            
            // Drag Start
            if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
                UISystem::State.draggedItem = item;
                UISystem::State.isDraggingFromStash = true;
                UISystem::State.dragSourceStashTab = m_activeTabIndex;
                UISystem::State.dragSourceStashSlot = i;
                UISystem::State.dragSourceStashType = m_activeType;
                
                UISystem::State.isDraggingFromInventory = false;
                UISystem::State.dragSourceEquipmentSlot = EquipmentSlot::None;
                UISystem::State.dragSourceBagSlotIndex = -1;
            }
            
            // Ctrl+Click Withdraw
            if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && IsKeyDown(KEY_LEFT_CONTROL) && item != entt::null) {
                StashSystem::quickWithdraw(registry, m_activeType, m_activeTabIndex, i);
            }

            // Drop
            if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null) {
                bool success = false;
                if (UISystem::State.isDraggingFromStash) {
                    success = StashSystem::transferItem(registry, 
                        UISystem::State.dragSourceStashType, UISystem::State.dragSourceStashTab, UISystem::State.dragSourceStashSlot,
                        m_activeType, m_activeTabIndex, i);
                } else if (UISystem::State.isDraggingFromInventory) {
                    // Inv -> Stash
                    if (StashSystem::depositFromInventory(registry, UISystem::State.draggedItem, 
                                                          UISystem::State.dragSourceInventoryIndex,
                                                          m_activeType, m_activeTabIndex, i)) {
                        success = true;
                    } else {
                         UISystem::State.showMessageBox = true;
                         snprintf(UISystem::State.messageBoxText, 64, "该物品无法存入");
                         UISystem::State.messageBoxTimer = 1.0f;
                    }
                }

                if (success) {
                    UISystem::State.draggedItem = entt::null;
                }
            }
            
            float itemAlpha = isMatch ? alpha : alpha * 0.3f;
            UIRenderer::DrawSlot(font, registry, x, y, slotSize, (UISystem::State.draggedItem == item) ? entt::null : item, nullptr, isHovered, false, itemAlpha);
        }
    }

    // Search Bar
    float searchW = 200.0f;
    float searchX = panelX + panelW - searchW - 30.0f;
    float searchY = panelY + 20.0f;
    Rectangle searchRect = {searchX, searchY, searchW, 28.0f};
    
    bool searchHover = CheckCollisionPointRec(mousePos, searchRect);
    if (searchHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        m_isSearchFocused = true;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !searchHover) {
        m_isSearchFocused = false;
    }

    DrawRectScaled(searchRect.x, searchRect.y, searchRect.width, searchRect.height, m_isSearchFocused ? theme.buttonHover : theme.buttonNormal);
    DrawRectLinesScaled(searchRect, 1.0f, m_isSearchFocused ? theme.panelBorderHighlight : theme.panelBorder);
    
    const char* searchText = (strlen(m_searchBuffer) == 0 && !m_isSearchFocused) ? "搜索物品..." : m_searchBuffer;
    Color searchColor = (strlen(m_searchBuffer) == 0 && !m_isSearchFocused) ? theme.textSecondary : theme.textPrimary;
    UIRenderer::DrawTextUI(font, searchText, searchRect.x + 5, searchRect.y + 4, 18, searchColor, alpha);

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

    // --- Footer Controls ---
    float footerY = panelY + panelH - 50.0f;
    
    // Sort Button
    Rectangle sortBtn = { panelX + 30.0f, footerY, 100.0f, 32.0f };
    bool sortHover = CheckCollisionPointRec(mousePos, sortBtn);
    DrawRectScaled(sortBtn.x, sortBtn.y, sortBtn.width, sortBtn.height, sortHover ? theme.buttonHover : theme.buttonNormal);
    DrawRectLinesScaled(sortBtn, 1.0f, theme.panelBorder);
    UIRenderer::DrawTextUI(font, "整理标签页", sortBtn.x + 10, sortBtn.y + 6, 18, theme.textPrimary, alpha);
    if (sortHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        StashSystem::sortTab(registry, m_activeType, m_activeTabIndex, StashSortMode::RarityDesc);
    }

    // Auto Deposit Button
    Rectangle depositBtn = { panelX + 140.0f, footerY, 100.0f, 32.0f };
    bool depositHover = CheckCollisionPointRec(mousePos, depositBtn);
    DrawRectScaled(depositBtn.x, depositBtn.y, depositBtn.width, depositBtn.height, depositHover ? theme.buttonHover : theme.buttonNormal);
    DrawRectLinesScaled(depositBtn, 1.0f, theme.panelBorder);
    UIRenderer::DrawTextUI(font, "存入全部", depositBtn.x + 15, depositBtn.y + 6, 18, theme.textPrimary, alpha);
    if (depositHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int count = StashSystem::autoDeposit(registry, m_activeType);
        if (count > 0) {
            LOG_INFO("Auto Deposit: Moved {} items to stash", count);
        }
    }
}