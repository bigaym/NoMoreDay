#include "game/application/ui/UIStashController.hpp"

#include "core/logging/Logger.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/StashComponent.hpp"
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

UIStashController::UIStashController(UiRuntime& runtime, GameUiHost* uiHost)
    : m_runtime(runtime), m_uiHost(uiHost) {
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
  // U8 收尾: 面板可见性是实例权威状态（原逐帧 re-adopt State.showStash 已删），
  // 进入新会话时显式复位。
  m_visible = false;
  m_alpha = 0.0f;
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

  // U8 收尾: 实例 alpha 动画（m_visible 为实例权威状态，由 Open/Close/Toggle
  // 写入；原逐帧 re-adopt State.showStash 与 mirror State.stashAlpha 已删）。
  const float dt = GetFrameTime();
  const float alphaSpeed = 6.0f;
  if (m_visible) {
    m_alpha = std::min(1.0f, m_alpha + dt * alphaSpeed);
  } else {
    m_alpha = std::max(0.0f, m_alpha - dt * alphaSpeed);
  }
}

void UIStashController::SetVisible(bool visible) {
  m_visible = visible;
  SetNodeVisible(visible);
}

bool UIStashController::IsVisible() const noexcept {
  return m_visible;
}

void UIStashController::Toggle() {
  SetVisible(!m_visible);
}

void UIStashController::Open(NoMoreDay::StashType type) {
  m_visible = true;
  SetNodeVisible(true);
  // U8 收尾: 打开仓库连带打开背包面板（legacy 语义：State.showStash 与
  // State.showInventory 同置 true）——经 host 路由到实例化背包控制器。
  if (m_uiHost) {
    m_uiHost->SetInventoryVisible(true);
  }
  m_activeType = type;
  m_activeTabIndex = 0; // Reset to first tab
}

void UIStashController::Close() {
  SetVisible(false);
}

NoMoreDay::StashType UIStashController::GetActiveType() const noexcept {
  return m_activeType;
}

int UIStashController::GetActiveTabIndex() const noexcept {
  return m_activeTabIndex;
}

void UIStashController::Draw(entt::registry& registry) {
  float alpha = m_alpha;
  if (alpha <= 0.0f) return;

  // Layout
  const float panelW = 680.0f;
  const float panelH = 820.0f;

  float defaultX = 100.0f;
  float defaultY = (UI_REF_HEIGHT - panelH) / 2.0f;

  // U8: panel drag state is instance-owned (m_panelState / m_activeDragPanel)
  // and runs through the stateless service directly, replacing the legacy
  // the legacy static drag wrapper that bound the shared panel-drag state /
  // the shared active-drag panel.
  UIPanelDragInputs dragInputs;
  dragInputs.mousePosition = UISystem::GetMousePositionLogic();
  dragInputs.isMousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
  dragInputs.isMouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
  UIPanelDragBounds dragBounds;
  dragBounds.panelWidth = panelW;
  dragBounds.panelHeight = panelH;
  dragBounds.headerHeight = 60.0f;
  dragBounds.uiRefWidth = UI_REF_WIDTH;
  dragBounds.uiRefHeight = UI_REF_HEIGHT;
  float panelX = defaultX;
  float panelY = defaultY;
  UIPanelDragService::UpdatePanelDrag(m_panelState, UIPanelID::Stash,
                                      m_activeDragPanel, panelX, panelY,
                                      dragInputs, dragBounds);

  Vector2 mousePos = UISystem::GetMousePositionLogic();
  if (CheckCollisionPointRec(mousePos, {panelX, panelY, panelW, panelH})) {
    // U8 收尾: mouse-over-UI 门控经 host 实例成员（原 State.isMouseOverUI）。
    if (m_uiHost) {
      m_uiHost->SetMouseOverUI(true);
    }
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
          // U8: message box routes through the host channel (overlay
          // controller); the host is null in headless tests, where the
          // notification is skipped.
          if (m_uiHost) {
            m_uiHost->ShowMessageBox("金币不足");
          }
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
    // U8: the drag session is host-owned (single instance across panels);
    // reads/writes below route through it instead of the legacy State fields.
    UIDragSession& drag = DragSession();
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
          drag.draggedItem == entt::null) {
        // U8: route the panel hover write through the host channel instead of
        // the static UiShared::HoveredItem() slot (hover highlight consumer is
        // the tooltip controller / frame write-back). m_uiHost may be null in
        // headless unit tests, where the hover pipeline is not exercised.
        if (m_uiHost) {
          m_uiHost->SetHoveredItem(item);
        }
      }

      // Drag Start
      if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
          item != entt::null) {
        drag.draggedItem = item;
        drag.isDraggingFromStash = true;
        drag.dragSourceStashTab = m_activeTabIndex;
        drag.dragSourceStashSlot = i;
        drag.dragSourceStashType = m_activeType;

        drag.isDraggingFromInventory = false;
        drag.dragSourceEquipmentSlot = EquipmentSlot::None;
        drag.dragSourceBagSlotIndex = -1;
      }

      // Ctrl+Click Withdraw
      if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
          IsKeyDown(KEY_LEFT_CONTROL) && item != entt::null) {
        StashSystem::quickWithdraw(registry, m_activeType, m_activeTabIndex, i);
      }

      // Drop
      if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) &&
          drag.draggedItem != entt::null) {
        bool success = false;
        if (drag.isDraggingFromStash) {
          success = StashSystem::transferItem(
              registry, drag.dragSourceStashType, drag.dragSourceStashTab,
              drag.dragSourceStashSlot, m_activeType, m_activeTabIndex, i);
        } else if (drag.isDraggingFromInventory) {
          // Inv -> Stash
          if (StashSystem::depositFromInventory(
                  registry, drag.draggedItem, drag.dragSourceInventoryIndex,
                  m_activeType, m_activeTabIndex, i)) {
            success = true;
          } else {
            // U8: message box routes through the host channel.
            if (m_uiHost) {
              m_uiHost->ShowMessageBox("该物品无法存入");
            }
          }
        }

        if (success) {
          drag.draggedItem = entt::null;
        }
      }

      float itemAlpha = isMatch ? alpha : alpha * 0.3f;
      UIRenderer::DrawSlot(font, registry, x, y, slotSize,
                           (drag.draggedItem == item) ? entt::null : item,
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

  // Search focus is instance state; the host InputCapture aggregates
  // IsSearchFocused() for the typing gate (the legacy State.isTyping write is
  // gone; F2 removes the legacy field).

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

UIDragSession& UIStashController::DragSession() noexcept {
  return m_uiHost ? m_uiHost->DragSession() : m_localDragSession;
}

} // namespace NoMoreDay::ui
