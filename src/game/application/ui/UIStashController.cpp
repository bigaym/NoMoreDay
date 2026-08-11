#include "game/application/ui/UIStashController.hpp"

#include "core/logging/Logger.hpp"
#include "core/utils/FmtBuffer.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/ui_shared/UiShared.hpp"
#include "game/systems/item/SharedStash.hpp"
#include "game/systems/item/StashSystem.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstring>

namespace NoMoreDay::ui {

namespace {

constexpr UiId kUIStashRootNode =
    static_cast<UiId>(entt::hashed_string("ui_stash_panel").value());

} // namespace

UIStashController::UIStashController(UiRuntime& runtime)
    : m_runtime(runtime) {
  // StashType is forward-declared in the header, so the "Personal" default is
  // applied here instead of as an in-class initializer.
  m_activeType = NoMoreDay::StashType::Personal;

  UiNodeDesc desc;
  desc.id = kUIStashRootNode;
  desc.parent = kRootUiId;
  // Full-viewport declarative anchor. The stash panel itself is still drawn by
  // the immediate-mode Draw below; this node is the host-owned root that a
  // later U7 step will render into.
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.layout.horizontalAlignment = UiAlignment::Start;
  desc.layout.verticalAlignment = UiAlignment::Start;
  desc.visible = true;
  // The panel is interactive, but input is routed through the legacy
  // immediate-mode path; the declarative node must not intercept pointers.
  desc.hitTestVisible = false;
  desc.capturePointer = false;
  desc.focusable = false;
  desc.captureKeyboard = false;
  desc.acceptsText = false;
  desc.modal = false;
  desc.zIndex = static_cast<std::int32_t>(UiDrawLayer::Panels);
  desc.customPainter = kInvalidUiResourceId;

  if (m_runtime.CreateNode(desc)) {
    m_rootNodeId = desc.id;
  }
}

void UIStashController::EnterGameplay() {
  ResetSessionState();
  m_inGameplay = true;
  SetNodeVisible(true);
}

void UIStashController::LeaveGameplay() {
  ResetSessionState();
  m_inGameplay = false;
  SetNodeVisible(false);
}

void UIStashController::Update(entt::registry& registry) {
  (void)registry;

  // Ported from UIStash::Update: animate the panel alpha towards the target
  // implied by UISystem::State.showStash. Time-driven, no static state.
  const float dt = GetFrameTime();
  const float alphaSpeed = 6.0f;
  if (UISystem::State.showStash) {
    UISystem::State.stashAlpha =
        std::min(1.0f, UISystem::State.stashAlpha + dt * alphaSpeed);
  } else {
    UISystem::State.stashAlpha =
        std::max(0.0f, UISystem::State.stashAlpha - dt * alphaSpeed);
  }
}

bool UIStashController::IsVisible() const noexcept {
  return UISystem::State.showStash;
}

void UIStashController::Toggle() {
  if (UISystem::State.showStash) {
    Close();
  } else {
    Open(m_activeType);
  }
}

void UIStashController::Open(NoMoreDay::StashType type) {
  UISystem::State.showStash = true;
  UISystem::State.showInventory = true; // Auto open inventory
  m_activeType = type;
  m_activeTabIndex = 0; // Reset to first tab
}

void UIStashController::Close() {
  UISystem::State.showStash = false;
}

NoMoreDay::StashType UIStashController::GetActiveType() const noexcept {
  return m_activeType;
}

int UIStashController::GetActiveTabIndex() const noexcept {
  return m_activeTabIndex;
}

void UIStashController::Draw(entt::registry& registry) {
  float alpha = UISystem::State.stashAlpha;
  if (alpha <= 0.0f) return;

  // Layout
  const float panelW = 680.0f;
  const float panelH = 820.0f;

  float defaultX = 100.0f;
  float defaultY = (UI_REF_HEIGHT - panelH) / 2.0f;

  UISystem::UpdatePanelDrag(UIPanelID::Stash, defaultX, defaultY, panelW,
                            panelH, 60.0f);

  float panelX = UISystem::State.panelStates[(int)UIPanelID::Stash].position.x;
  float panelY = UISystem::State.panelStates[(int)UIPanelID::Stash].position.y;

  if (panelX < 0) {
    panelX = defaultX;
    panelY = defaultY;
    UISystem::State.panelStates[(int)UIPanelID::Stash].position = {panelX,
                                                                  panelY};
  }

  Vector2 mousePos = UISystem::GetMousePositionLogic();
  if (CheckCollisionPointRec(mousePos, {panelX, panelY, panelW, panelH})) {
    UISystem::State.isMouseOverUI = true;
  }

  float scale = UIRenderer::GetScale();
  auto& theme = UIRenderer::GetTheme();
  Font font = UISystem::GetFont();

  auto ApplyAlpha = [&](Color c, float a) -> Color {
    return {c.r, c.g, c.b, (unsigned char)((float)c.a * a)};
  };

  auto DrawRectScaled = [&](float x, float y, float w, float h, Color c) {
    DrawRectangle((int)(x * scale), (int)(y * scale), (int)(w * scale),
                  (int)(h * scale), ApplyAlpha(c, alpha));
  };

  auto DrawRectLinesScaled = [&](Rectangle rec, float thick, Color c) {
    DrawRectangleLinesEx({rec.x * scale, rec.y * scale, rec.width * scale,
                          rec.height * scale},
                         thick * scale, ApplyAlpha(c, alpha));
  };

  // Background
  DrawRectScaled(panelX, panelY, panelW, panelH, theme.panelBackground);
  DrawRectLinesScaled({panelX, panelY, panelW, panelH}, 1.0f, theme.panelBorder);

  // Header
  const char* title =
      (m_activeType == StashType::Shared) ? "共享仓库" : "个人仓库";
  UIRenderer::DrawTextUI(font, title, panelX + 30, panelY + 20, 28,
                         theme.textHighlight, alpha);

  // Close Button
  float closeSize = 28.0f;
  Rectangle closeRect = {panelX + panelW - closeSize - 15.0f, panelY + 15.0f,
                         closeSize, closeSize};
  bool closeHover = CheckCollisionPointRec(mousePos, closeRect);
  Texture2D squareTex =
      AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Square.id);

  UIRenderer::DrawButton(font, squareTex, closeRect, "x", 20,
                         closeHover ? WHITE : theme.textSecondary, WHITE,
                         closeHover,
                         closeHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON),
                         alpha);

  if (closeHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    Close();
  }

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
    UIRenderer::DrawTextUI(font, tab->name.c_str(), tabX + 10, tabY + 6, 18,
                           txtColor, alpha);

    if (isHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      m_activeTabIndex = i;
    }

    tabX += tabW + 5.0f;
  }

  // Unlock Button
  int maxTabs =
      (m_activeType == StashType::Shared)
          ? SharedStash::Get().getMaxTabs()
          : PersonalStashComponent::MAX_TABS;
  if (unlockedCount < maxTabs) {
    float btnW = 30.0f;
    if (tabX + btnW > panelX + panelW - 30.0f) {
      tabX = panelX + 30.0f;
      tabY += tabH + 5.0f;
    }

    bool isHover = CheckCollisionPointRec(mousePos, {tabX, tabY, btnW, tabH});
    UIRenderer::DrawButton(font, squareTex, {tabX, tabY, btnW, tabH}, "+", 20,
                           theme.textHighlight, WHITE, isHover,
                           isHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON),
                           alpha);

    if (isHover) {
      int cost = StashSystem::getNextUnlockCost(registry, m_activeType);
      UIRenderer::DrawTextUI(font,
                             TextFormat("解锁费用: %d 金币", cost),
                             mousePos.x + 15, mousePos.y, 18, WHITE, alpha);

      if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (StashSystem::unlockTab(registry, m_activeType)) {
          // Success
        } else {
          UISystem::State.showMessageBox = true;
          utils::FormatToBuffer(UISystem::State.messageBoxText, "金币不足");
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
      m_cachedSearchResults =
          StashSystem::search(registry, m_activeType, m_searchBuffer);
    } else {
      m_cachedSearchResults.clear();
    }
  }

  StashTab* currentTab = StashSystem::getTab(registry, m_activeType,
                                             m_activeTabIndex);
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

      if (isHovered && item != entt::null &&
          UISystem::State.draggedItem == entt::null) {
        UiShared::HoveredItem() = item;
      }

      // Drag Start
      if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
          item != entt::null) {
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
      if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
          IsKeyDown(KEY_LEFT_CONTROL) && item != entt::null) {
        StashSystem::quickWithdraw(registry, m_activeType, m_activeTabIndex, i);
      }

      // Drop
      if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) &&
          UISystem::State.draggedItem != entt::null) {
        bool success = false;
        if (UISystem::State.isDraggingFromStash) {
          success = StashSystem::transferItem(
              registry, UISystem::State.dragSourceStashType,
              UISystem::State.dragSourceStashTab,
              UISystem::State.dragSourceStashSlot, m_activeType,
              m_activeTabIndex, i);
        } else if (UISystem::State.isDraggingFromInventory) {
          // Inv -> Stash
          if (StashSystem::depositFromInventory(
                  registry, UISystem::State.draggedItem,
                  UISystem::State.dragSourceInventoryIndex, m_activeType,
                  m_activeTabIndex, i)) {
            success = true;
          } else {
            UISystem::State.showMessageBox = true;
            utils::FormatToBuffer(UISystem::State.messageBoxText, "该物品无法存入");
            UISystem::State.messageBoxTimer = 1.0f;
          }
        }

        if (success) {
          UISystem::State.draggedItem = entt::null;
        }
      }

      float itemAlpha = isMatch ? alpha : alpha * 0.3f;
      UIRenderer::DrawSlot(font, registry, x, y, slotSize,
                           (UISystem::State.draggedItem == item) ? entt::null
                                                                 : item,
                           nullptr, isHovered, false, itemAlpha);
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

  // Propagate blocking state
  if (m_isSearchFocused) UISystem::State.isTyping = true;

  DrawRectScaled(searchRect.x, searchRect.y, searchRect.width, searchRect.height,
                 m_isSearchFocused ? theme.buttonHover : theme.buttonNormal);
  DrawRectLinesScaled(searchRect, 1.0f,
                      m_isSearchFocused ? theme.panelBorderHighlight
                                        : theme.panelBorder);

  const char* searchText =
      (strlen(m_searchBuffer) == 0 && !m_isSearchFocused) ? "搜索物品..."
                                                          : m_searchBuffer;
  Color searchColor =
      (strlen(m_searchBuffer) == 0 && !m_isSearchFocused)
          ? theme.textSecondary
          : theme.textPrimary;
  UIRenderer::DrawTextUI(font, searchText, searchRect.x + 5, searchRect.y + 4,
                         18, searchColor, alpha);

  if (m_isSearchFocused) {
    int key = GetCharPressed();
    while (key > 0) {
      if (key >= 32) {
        int len = strlen(m_searchBuffer);
        if (len < 60) {
          if (key <= 0x7F) {
            m_searchBuffer[len++] = (char)key;
          } else if (key <= 0x7FF) {
            m_searchBuffer[len++] = (char)(0xC0 | ((key >> 6) & 0x1F));
            m_searchBuffer[len++] = (char)(0x80 | (key & 0x3F));
          } else if (key <= 0xFFFF) {
            m_searchBuffer[len++] = (char)(0xE0 | ((key >> 12) & 0x0F));
            m_searchBuffer[len++] = (char)(0x80 | ((key >> 6) & 0x3F));
            m_searchBuffer[len++] = (char)(0x80 | (key & 0x3F));
          } else if (key <= 0x10FFFF) {
            m_searchBuffer[len++] = (char)(0xF0 | ((key >> 18) & 0x07));
            m_searchBuffer[len++] = (char)(0x80 | ((key >> 12) & 0x3F));
            m_searchBuffer[len++] = (char)(0x80 | ((key >> 6) & 0x3F));
            m_searchBuffer[len++] = (char)(0x80 | (key & 0x3F));
          }
          m_searchBuffer[len] = '\0';
        }
      }
      key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
      int len = strlen(m_searchBuffer);
      if (len > 0) {
        while (len > 0) {
          len--;
          if ((m_searchBuffer[len] & 0xC0) != 0x80) break;
        }
        m_searchBuffer[len] = '\0';
      }
    }
  }

  // --- Footer Controls ---
  float footerY = panelY + panelH - 50.0f;
  Texture2D rectTex =
      AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);

  // Sort Button
  Rectangle sortBtn = {panelX + 30.0f, footerY, 110.0f, 36.0f};
  bool sortHover = CheckCollisionPointRec(mousePos, sortBtn);
  UIRenderer::DrawButton(font, rectTex, sortBtn, "整理标签页", 18,
                         theme.textPrimary, WHITE, sortHover,
                         sortHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON),
                         alpha);

  if (sortHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    StashSystem::sortTab(registry, m_activeType, m_activeTabIndex,
                         StashSortMode::RarityDesc);
  }

  // Auto Deposit Button
  Rectangle depositBtn = {panelX + 150.0f, footerY, 110.0f, 36.0f};
  bool depositHover = CheckCollisionPointRec(mousePos, depositBtn);
  UIRenderer::DrawButton(font, rectTex, depositBtn, "存入全部", 18,
                         theme.textPrimary, WHITE, depositHover,
                         depositHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON),
                         alpha);

  if (depositHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    int count = StashSystem::autoDeposit(registry, m_activeType);
    if (count > 0) {
      LOG_INFO("Auto Deposit: Moved {} items to stash", count);
    }
  }
}

UiId UIStashController::NodeId() const noexcept {
  return m_rootNodeId;
}

bool UIStashController::IsInGameplay() const noexcept {
  return m_inGameplay;
}

void UIStashController::ResetSessionState() noexcept {
  m_activeType = NoMoreDay::StashType::Personal;
  m_activeTabIndex = 0;
  m_searchBuffer[0] = '\0';
  m_lastSearchBuffer[0] = '\0';
  m_cachedSearchResults.clear();
  m_isSearchFocused = false;
}

void UIStashController::SetNodeVisible(bool visible) {
  if (m_rootNodeId != kInvalidUiId) {
    (void)m_runtime.SetNodeVisible(m_rootNodeId, visible);
  }
}

} // namespace NoMoreDay::ui
