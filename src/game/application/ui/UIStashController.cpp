#include "game/application/ui/UIStashController.hpp"

#include "core/logging/Logger.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiResourceIds.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstring>

namespace NoMoreDay::ui {

namespace {

constexpr UiId kUIStashRootNode =
    static_cast<UiId>(entt::hashed_string("ui_stash_panel").value());

// Panel geometry (mirrors the legacy UIStash::Draw constants).
inline constexpr float kPanelW = 680.0f;
inline constexpr float kPanelH = 820.0f;
inline constexpr float kHeaderHeight = 60.0f;
inline constexpr float kCloseSize = 28.0f;
inline constexpr float kTabH = 30.0f;
inline constexpr float kSlotSize = 48.0f;
inline constexpr float kSlotGap = 4.0f;
inline constexpr int kGridCols = 12;
inline constexpr int kMaxTabs = 10; // PersonalStashComponent::MAX_TABS == SharedStash::getMaxTabs()
inline constexpr float kFooterBtnW = 110.0f;
inline constexpr float kFooterBtnH = 36.0f;
// Stash tab capacity (StashTab::CAPACITY); kept as a local constant so the
// controller does not need StashComponent.hpp (registry-free paint contract).
inline constexpr int kStashTabCapacity = 144;
// NoMoreDay::StashSortMode::RarityDesc underlying value (the handler casts
// the payload back to StashSortMode; kept literal to avoid a systems include).
inline constexpr std::uint8_t kStashSortModeRarityDesc = 0;

constexpr UiColor kWhiteTint{255, 255, 255, 255};
constexpr UiColor kBlackText{0, 0, 0, 255};

// Alpha-multiplies a color (paint colors derive from the theme + panel alpha).
[[nodiscard]] UiColor Faded(const UiColor& color, float alpha) noexcept {
  const std::uint8_t a = static_cast<std::uint8_t>(
      static_cast<float>(color.a) * std::clamp(alpha, 0.0f, 1.0f));
  return {color.r, color.g, color.b, a};
}

[[nodiscard]] UiColor ToUiColor(const Color& color) noexcept {
  return {color.r, color.g, color.b, color.a};
}

} // namespace

UIStashController::UIStashController(UiRuntime& runtime, GameUiHost* uiHost)
    : m_runtime(runtime), m_uiHost(uiHost) {
  // StashType is forward-declared in the header, so the "Personal" default is
  // applied here instead of as an in-class initializer.
  m_activeType = NoMoreDay::StashType::Personal;
  // R7: panel origin is instance state so dragging persists across frames
  // (same pattern as the R6 inventory controller).
  m_panelX = 100.0f;
  m_panelY = (UI_REF_HEIGHT - kPanelH) * 0.5f;

  UiNodeDesc desc;
  desc.id = kUIStashRootNode;
  desc.parent = kRootUiId;
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.layout.horizontalAlignment = UiAlignment::Start;
  desc.layout.verticalAlignment = UiAlignment::Start;
  desc.visible = true;
  // The panel is interactive, but input is routed through the controller
  // Update phase; the declarative node must not intercept pointers.
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

UIStashController::Layout UIStashController::ComputeLayout() const noexcept {
  Layout layout;
  layout.panelX = m_panelX;
  layout.panelY = m_panelY;
  layout.panelW = kPanelW;
  layout.panelH = kPanelH;

  // Tabs start below the header and wrap to a second row when needed.
  layout.tabY = m_panelY + 60.0f;
  // Grid (12 columns) sits below the tab strip + the unlock gap.
  layout.gridY = layout.tabY + kTabH + 20.0f;
  layout.gridX = m_panelX + 30.0f;
  layout.footerY = m_panelY + kPanelH - 50.0f;
  return layout;
}

int UIStashController::FirstFreeInventorySlot(
    const GameUiSnapshot& snapshot) noexcept {
  const int capacity = std::max(0, snapshot.inventory.capacity);
  for (int slot = 0; slot < capacity; ++slot) {
    bool occupied = false;
    for (const GameUiItemView& item : snapshot.inventory.items) {
      if (item.inventoryIndex == slot) {
        occupied = true;
        break;
      }
    }
    if (!occupied) {
      return slot;
    }
  }
  return -1;
}

void UIStashController::Update(const GameUiSnapshot& snapshot,
                               const UiInputFrame& input) {
  // Alpha animation (instance state; m_visible authoritative).
  const float target = m_visible ? 1.0f : 0.0f;
  if (m_alpha < target) {
    m_alpha = std::min(target, m_alpha + 6.0f * input.deltaSeconds);
  } else if (m_alpha > target) {
    m_alpha = std::max(target, m_alpha - 6.0f * input.deltaSeconds);
  }

  if (!m_visible) {
    if (m_rootNodeId != kInvalidUiId) {
      (void)m_runtime.SetNodeVisible(m_rootNodeId, false);
    }
    return;
  }
  if (m_rootNodeId != kInvalidUiId) {
    (void)m_runtime.SetNodeVisible(m_rootNodeId, true);
  }
  if (!m_inGameplay) {
    return;
  }

  // Panel drag (U8: instance drag state via the shared service).
  {
    const UIPanelDragInputs dragInputs{
        {input.pointer.logicalPosition.x, input.pointer.logicalPosition.y},
        input.pointer.pressed,
        IsMouseButtonDown(MOUSE_LEFT_BUTTON),
    };
    const UIPanelDragBounds dragBounds{
        kPanelW, kPanelH, kHeaderHeight, 50.0f, UI_REF_WIDTH, UI_REF_HEIGHT};
    UIPanelDragService::UpdatePanelDrag(
        m_panelState, UIPanelID::Stash, m_activeDragPanel, m_panelX, m_panelY,
        dragInputs, dragBounds);
  }

  const bool allowStashInput =
      m_uiHost == nullptr || !m_uiHost->IsModalInputCaptured();
  if (!allowStashInput) {
    return;
  }

  const Layout layout = ComputeLayout();
  const UiVec2 mouse = input.pointer.logicalPosition;
  const UiRect panelRect{{layout.panelX, layout.panelY},
                         {layout.panelW, layout.panelH}};

  if (panelRect.Contains(mouse) && m_uiHost != nullptr) {
    m_uiHost->SetMouseOverUI(true);
  }

  UIDragSession& drag = DragSession();
  const bool isDragging = drag.IsDragging();

  const GameUiStashView& stash = snapshot.stash;
  const int unlockedCount = std::max(0, stash.unlockedTabs);

  // --- Close button ---------------------------------------------------------
  {
    const UiRect closeRect{{layout.panelX + layout.panelW - kCloseSize - 15.0f,
                            layout.panelY + 15.0f},
                           {kCloseSize, kCloseSize}};
    if (closeRect.Contains(mouse) && input.pointer.pressed) {
      Close();
    }
  }

  // --- Tabs -----------------------------------------------------------------
  float tabX = layout.panelX + 30.0f;
  float tabY = layout.tabY;
  for (int i = 0; i < unlockedCount && i < static_cast<int>(stash.tabs.size());
       ++i) {
    const GameUiStashTabView& tab = stash.tabs[static_cast<std::size_t>(i)];
    // Bounded name cache: length is capped at 15 chars by the builder.
    const std::size_t nameLen = std::strlen(tab.name.data());
    const float tabW = static_cast<float>(nameLen) * 11.0f + 20.0f;
    if (tabX + tabW > layout.panelX + layout.panelW - 60.0f) {
      tabX = layout.panelX + 30.0f;
      tabY += kTabH + 5.0f;
    }
    const UiRect tabRect{{tabX, tabY}, {tabW, kTabH}};
    if (tabRect.Contains(mouse) && input.pointer.pressed) {
      m_activeTabIndex = i;
    }
    tabX += tabW + 5.0f;
  }

  // --- Unlock button --------------------------------------------------------
  if (unlockedCount < kMaxTabs) {
    const float btnW = 30.0f;
    if (tabX + btnW > layout.panelX + layout.panelW - 30.0f) {
      tabX = layout.panelX + 30.0f;
      tabY += kTabH + 5.0f;
    }
    const UiRect unlockRect{{tabX, tabY}, {btnW, kTabH}};
    if (unlockRect.Contains(mouse)) {
      // Tooltip (drawn in Paint from the snapshot cost).
      m_unlockButtonHovered = true;
      m_unlockTooltipPos = mouse;
      if (input.pointer.pressed) {
        GameUiIntent intent;
        intent.sourceNode = m_rootNodeId;
        intent.kind = GameUiIntentKind::StashUnlockTab;
        intent.payload.stashTarget =
            static_cast<std::uint8_t>(m_activeType);
        EnqueueIntent(std::move(intent));
      }
    } else {
      m_unlockButtonHovered = false;
    }
  } else {
    m_unlockButtonHovered = false;
  }

  // --- Search bar -----------------------------------------------------------
  {
    const float searchW = 200.0f;
    const UiRect searchRect{{layout.panelX + layout.panelW - searchW - 30.0f,
                             layout.panelY + 20.0f},
                            {searchW, 28.0f}};
    if (searchRect.Contains(mouse) && input.pointer.pressed) {
      m_isSearchFocused = true;
    } else if (input.pointer.pressed && !searchRect.Contains(mouse)) {
      m_isSearchFocused = false;
    }

    if (m_isSearchFocused) {
      int key = GetCharPressed();
      while (key > 0) {
        if (key >= 32) {
          int len = static_cast<int>(std::strlen(m_searchBuffer));
          if (len < 60) {
            if (key <= 0x7F) {
              m_searchBuffer[len++] = static_cast<char>(key);
            } else if (key <= 0x7FF) {
              m_searchBuffer[len++] =
                  static_cast<char>(0xC0 | ((key >> 6) & 0x1F));
              m_searchBuffer[len++] =
                  static_cast<char>(0x80 | (key & 0x3F));
            } else if (key <= 0xFFFF) {
              m_searchBuffer[len++] =
                  static_cast<char>(0xE0 | ((key >> 12) & 0x0F));
              m_searchBuffer[len++] =
                  static_cast<char>(0x80 | ((key >> 6) & 0x3F));
              m_searchBuffer[len++] =
                  static_cast<char>(0x80 | (key & 0x3F));
            } else if (key <= 0x10FFFF) {
              m_searchBuffer[len++] =
                  static_cast<char>(0xF0 | ((key >> 18) & 0x07));
              m_searchBuffer[len++] =
                  static_cast<char>(0x80 | ((key >> 12) & 0x3F));
              m_searchBuffer[len++] =
                  static_cast<char>(0x80 | ((key >> 6) & 0x3F));
              m_searchBuffer[len++] =
                  static_cast<char>(0x80 | (key & 0x3F));
            }
            m_searchBuffer[len] = '\0';
          }
        }
        key = GetCharPressed();
      }

      if (IsKeyPressed(KEY_BACKSPACE)) {
        int len = static_cast<int>(std::strlen(m_searchBuffer));
        if (len > 0) {
          while (len > 0) {
            len--;
            if ((m_searchBuffer[len] & 0xC0) != 0x80) {
              break;
            }
          }
          m_searchBuffer[len] = '\0';
        }
      }
    }
  }

  // --- Grid (active tab) ----------------------------------------------------
  // Legacy semantics: the grid iterates the full tab capacity so drops can
  // target empty cells too; the snapshot only carries occupied slots, so the
  // per-index lookup below resolves the occupied ones.
  const int activeTab = std::min(m_activeTabIndex, unlockedCount - 1);
  if (activeTab >= 0 &&
      activeTab < static_cast<int>(stash.tabs.size())) {
    const GameUiStashTabView& tab =
        stash.tabs[static_cast<std::size_t>(activeTab)];

    for (int i = 0; i < kStashTabCapacity; ++i) {
      const int r = i / kGridCols;
      const int c = i % kGridCols;
      const float x = layout.gridX + static_cast<float>(c) * (kSlotSize + kSlotGap);
      const float y = layout.gridY + static_cast<float>(r) * (kSlotSize + kSlotGap);
      const UiRect slotRect{{x, y}, {kSlotSize, kSlotSize}};
      const bool hovered = slotRect.Contains(mouse);

      // Resolve the occupied slot view for this cell (empty cells are valid
      // drop targets, so the loop still handles them).
      const GameUiStashSlotView* slot = nullptr;
      for (const GameUiStashSlotView& candidate : tab.slots) {
        if (candidate.slotIndex == i) {
          slot = &candidate;
          break;
        }
      }
      const bool hasItem = slot != nullptr && slot->domainId != kInvalidDomainId;

      if (hovered && hasItem && !isDragging && m_uiHost != nullptr) {
        m_uiHost->SetHoveredItemDomain(slot->domainId);
      }

      // Drag start (stash source).
      if (hovered && hasItem && input.pointer.pressed &&
          !IsKeyDown(KEY_LEFT_CONTROL)) {
        drag.draggedItemDomainId = slot->domainId;
        drag.isDraggingFromStash = true;
        drag.dragSourceStashTab = activeTab;
        drag.dragSourceStashSlot = i;
        drag.dragSourceStashType = m_activeType;

        drag.isDraggingFromInventory = false;
        drag.dragSourceEquipmentSlot = EquipmentSlot::None;
        drag.dragSourceBagSlotIndex = -1;
      }

      // Ctrl+Click quick withdraw (R7: intent path; the handler computes the
      // destination slot from the payload).
      if (hovered && hasItem && input.pointer.pressed &&
          IsKeyDown(KEY_LEFT_CONTROL)) {
        GameUiIntent intent;
        intent.sourceNode = m_rootNodeId;
        intent.kind = GameUiIntentKind::StashWithdraw;
        intent.payload.sourceTab = activeTab;
        intent.payload.sourceSlot = i;
        intent.payload.targetSlot = FirstFreeInventorySlot(snapshot);
        intent.payload.stashTarget =
            static_cast<std::uint8_t>(m_activeType);
        if (intent.payload.targetSlot >= 0) {
          EnqueueIntent(std::move(intent));
        }
      }

      // Drop onto a stash slot.
      if (hovered && input.pointer.released && isDragging &&
          drag.draggedItemDomainId != 0) {
        if (drag.isDraggingFromStash) {
          // Stash -> Stash (transfer between tabs/slots).
          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          intent.kind = GameUiIntentKind::StashTransfer;
          intent.payload.sourceTab = drag.dragSourceStashTab;
          intent.payload.sourceSlot = drag.dragSourceStashSlot;
          intent.payload.targetTab = activeTab;
          intent.payload.targetSlot = i;
          intent.payload.stashTarget =
              static_cast<std::uint8_t>(drag.dragSourceStashType);
          EnqueueIntent(std::move(intent));
          drag.Clear();
        } else if (drag.isDraggingFromInventory) {
          // Inventory -> Stash (deposit).
          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          intent.kind = GameUiIntentKind::StashDeposit;
          intent.payload.sourceDomainId = drag.draggedItemDomainId;
          intent.payload.sourceSlot = drag.dragSourceInventoryIndex;
          intent.payload.targetTab = activeTab;
          intent.payload.targetSlot = i;
          intent.payload.stashTarget =
              static_cast<std::uint8_t>(m_activeType);
          EnqueueIntent(std::move(intent));
          drag.Clear();
        }
      }
    }
  }

  // --- Footer: sort + auto-deposit ------------------------------------------
  {
    const UiRect sortBtn{{layout.panelX + 30.0f, layout.footerY},
                         {kFooterBtnW, kFooterBtnH}};
    if (sortBtn.Contains(mouse) && input.pointer.pressed) {
      GameUiIntent intent;
      intent.sourceNode = m_rootNodeId;
      intent.kind = GameUiIntentKind::StashSort;
      intent.payload.targetTab = m_activeTabIndex;
      intent.payload.sortMode = kStashSortModeRarityDesc;
      EnqueueIntent(std::move(intent));
    }

    const UiRect depositBtn{{layout.panelX + 150.0f, layout.footerY},
                            {kFooterBtnW, kFooterBtnH}};
    if (depositBtn.Contains(mouse) && input.pointer.pressed) {
      GameUiIntent intent;
      intent.sourceNode = m_rootNodeId;
      intent.kind = GameUiIntentKind::StashAutoDeposit;
      intent.payload.stashTarget =
          static_cast<std::uint8_t>(m_activeType);
      EnqueueIntent(std::move(intent));
    }
  }
}

void UIStashController::Paint(UiDrawList& drawList, const UiViewport& viewport,
                              const GameUiSnapshot& snapshot) const {
  (void)viewport;
  if (!m_visible || !m_inGameplay || m_alpha <= 0.001f ||
      m_rootNodeId == kInvalidUiId) {
    return;
  }
  const float alpha = m_alpha;
  const Layout layout = ComputeLayout();
  const UiDrawLayer layer = UiDrawLayer::Panels;
  const UiId node = m_rootNodeId;

  const UiColor themeBg = ToUiColor(UIRenderer::GetTheme().panelBackground);
  const UiColor themeBorder = ToUiColor(UIRenderer::GetTheme().panelBorder);
  const UiColor themeText = ToUiColor(UIRenderer::GetTheme().textPrimary);
  const UiColor themeTextSecondary =
      ToUiColor(UIRenderer::GetTheme().textSecondary);
  const UiColor themeHighlight =
      ToUiColor(UIRenderer::GetTheme().textHighlight);
  const UiColor themeSlot = ToUiColor(UIRenderer::GetTheme().slotBackground);
  const UiColor themeBtn = ToUiColor(UIRenderer::GetTheme().buttonNormal);
  const UiColor themeBtnHover = ToUiColor(UIRenderer::GetTheme().buttonHover);
  const UiColor themeBorderHighlight =
      ToUiColor(UIRenderer::GetTheme().panelBorderHighlight);

  // Background.
  drawList.FillRect(layer, node, {{layout.panelX, layout.panelY},
                                  {layout.panelW, layout.panelH}},
                    Faded(themeBg, alpha));
  drawList.StrokeRect(layer, node, {{layout.panelX, layout.panelY},
                                    {layout.panelW, layout.panelH}},
                      Faded(themeBorder, alpha), 1.0f);

  // Header title.
  const char* title =
      (m_activeType == StashType::Shared) ? "共享仓库" : "个人仓库";
  drawList.Text(layer, node, title, {layout.panelX + 30.0f, layout.panelY + 20.0f},
                28.0f, Faded(themeHighlight, alpha), kGlobalFontResourceId);

  // Close button.
  const UiRect closeRect{{layout.panelX + layout.panelW - kCloseSize - 15.0f,
                          layout.panelY + 15.0f},
                         {kCloseSize, kCloseSize}};
  drawList.FillRect(layer, node, closeRect, Faded(themeBtn, alpha));
  drawList.StrokeRect(layer, node, closeRect, Faded(themeBorder, alpha), 1.0f);
  drawList.Text(layer, node, "x",
                {closeRect.origin.x + 7.0f, closeRect.origin.y + 4.0f},
                20.0f, Faded(themeTextSecondary, alpha), kGlobalFontResourceId);

  // Tabs.
  float tabX = layout.panelX + 30.0f;
  float tabY = layout.tabY;
  const int unlockedCount = std::max(0, snapshot.stash.unlockedTabs);
  for (int i = 0;
       i < unlockedCount && i < static_cast<int>(snapshot.stash.tabs.size());
       ++i) {
    const GameUiStashTabView& tab =
        snapshot.stash.tabs[static_cast<std::size_t>(i)];
    const std::size_t nameLen = std::strlen(tab.name.data());
    const float tabW = static_cast<float>(nameLen) * 11.0f + 20.0f;
    if (tabX + tabW > layout.panelX + layout.panelW - 60.0f) {
      tabX = layout.panelX + 30.0f;
      tabY += kTabH + 5.0f;
    }
    const bool isActive = (m_activeTabIndex == i);
    const UiColor tabBg = isActive ? themeHighlight : themeSlot;
    const UiRect tabRect{{tabX, tabY}, {tabW, kTabH}};
    drawList.FillRect(layer, node, tabRect, Faded(tabBg, alpha));
    drawList.StrokeRect(layer, node, tabRect, Faded(themeBorder, alpha), 1.0f);
    drawList.Text(layer, node, std::string_view(tab.name.data(), nameLen),
                  {tabX + 10.0f, tabY + 6.0f}, 18.0f,
                  isActive ? kBlackText : Faded(themeText, alpha),
                  kGlobalFontResourceId);
    tabX += tabW + 5.0f;
  }

  // Unlock button + hover tooltip.
  if (unlockedCount < kMaxTabs) {
    const float btnW = 30.0f;
    if (tabX + btnW > layout.panelX + layout.panelW - 30.0f) {
      tabX = layout.panelX + 30.0f;
      tabY += kTabH + 5.0f;
    }
    const UiRect unlockRect{{tabX, tabY}, {btnW, kTabH}};
    drawList.FillRect(layer, node, unlockRect,
                      Faded(m_unlockButtonHovered ? themeBtnHover : themeBtn,
                            alpha));
    drawList.StrokeRect(layer, node, unlockRect, Faded(themeBorder, alpha), 1.0f);
    drawList.Text(layer, node, "+",
                  {tabX + 8.0f, tabY + 4.0f}, 20.0f,
                  Faded(themeHighlight, alpha), kGlobalFontResourceId);
    if (m_unlockButtonHovered) {
      char tooltip[64];
      std::snprintf(tooltip, sizeof(tooltip), "解锁费用: %d 金币",
                    snapshot.stash.nextUnlockCost);
      drawList.Text(layer, node, tooltip,
                    {m_unlockTooltipPos.x + 15.0f, m_unlockTooltipPos.y},
                    18.0f, kWhiteTint, kGlobalFontResourceId);
    }
  }

  // Grid (active tab). Paint iterates the full tab capacity and draws the
  // occupied cells from the snapshot (empty cells get the slot frame only).
  const int activeTab = std::min(m_activeTabIndex, unlockedCount - 1);
  if (activeTab >= 0 &&
      activeTab < static_cast<int>(snapshot.stash.tabs.size())) {
    const GameUiStashTabView& tab =
        snapshot.stash.tabs[static_cast<std::size_t>(activeTab)];
    for (int i = 0; i < kStashTabCapacity; ++i) {
      const int r = i / kGridCols;
      const int c = i % kGridCols;
      const float x = layout.gridX + static_cast<float>(c) * (kSlotSize + kSlotGap);
      const float y = layout.gridY + static_cast<float>(r) * (kSlotSize + kSlotGap);

      const GameUiStashSlotView* slot = nullptr;
      for (const GameUiStashSlotView& candidate : tab.slots) {
        if (candidate.slotIndex == i) {
          slot = &candidate;
          break;
        }
      }
      const float itemAlpha =
          (slot == nullptr || slot->matchesSearch) ? alpha : alpha * 0.3f;

      // R7: paint reads only the snapshot view model (no registry, no
      // component access).
      drawList.FillRect(layer, node, {{x, y}, {kSlotSize, kSlotSize}},
                        Faded(themeSlot, itemAlpha));
      drawList.StrokeRect(layer, node, {{x, y}, {kSlotSize, kSlotSize}},
                          Faded(themeBorder, itemAlpha), 1.0f);
      if (slot != nullptr && slot->domainId != kInvalidDomainId &&
          slot->textureId != 0) {
        const float pad = 4.0f;
        drawList.Image(layer, node, {{x + pad, y + pad},
                                     {kSlotSize - 2.0f * pad,
                                      kSlotSize - 2.0f * pad}},
                       slot->textureId, Faded(kWhiteTint, itemAlpha));
        if (slot->quantity > 1) {
          char qty[16];
          std::snprintf(qty, sizeof(qty), "%u", slot->quantity);
          drawList.Text(layer, node, qty, {x + 4.0f, y + kSlotSize - 16.0f},
                        12.0f, Faded(themeText, itemAlpha),
                        kGlobalFontResourceId);
        }
      }
    }
  }

  // Search bar.
  {
    const float searchW = 200.0f;
    const UiRect searchRect{{layout.panelX + layout.panelW - searchW - 30.0f,
                             layout.panelY + 20.0f},
                            {searchW, 28.0f}};
    drawList.FillRect(layer, node, searchRect,
                      Faded(m_isSearchFocused ? themeBtnHover : themeBtn,
                            alpha));
    drawList.StrokeRect(layer, node, searchRect,
                        Faded(m_isSearchFocused ? themeBorderHighlight
                                                : themeBorder,
                              alpha),
                        1.0f);
    const bool empty = (m_searchBuffer[0] == '\0');
    const char* text = (empty && !m_isSearchFocused) ? "搜索物品..." : m_searchBuffer;
    const UiColor textColor =
        (empty && !m_isSearchFocused) ? themeTextSecondary : themeText;
    drawList.Text(layer, node, text, {searchRect.origin.x + 5.0f,
                                      searchRect.origin.y + 4.0f},
                  18.0f, Faded(textColor, alpha), kGlobalFontResourceId);
  }

  // Footer buttons.
  const UiRect sortBtn{{layout.panelX + 30.0f, layout.footerY},
                       {kFooterBtnW, kFooterBtnH}};
  drawList.FillRect(layer, node, sortBtn, Faded(themeBtn, alpha));
  drawList.StrokeRect(layer, node, sortBtn, Faded(themeBorder, alpha), 1.0f);
  drawList.Text(layer, node, "整理标签页",
                {sortBtn.origin.x + 10.0f, sortBtn.origin.y + 8.0f},
                18.0f, Faded(themeText, alpha), kGlobalFontResourceId);

  const UiRect depositBtn{{layout.panelX + 150.0f, layout.footerY},
                          {kFooterBtnW, kFooterBtnH}};
  drawList.FillRect(layer, node, depositBtn, Faded(themeBtn, alpha));
  drawList.StrokeRect(layer, node, depositBtn, Faded(themeBorder, alpha), 1.0f);
  drawList.Text(layer, node, "存入全部",
                {depositBtn.origin.x + 10.0f, depositBtn.origin.y + 8.0f},
                18.0f, Faded(themeText, alpha), kGlobalFontResourceId);
}

UiId UIStashController::NodeId() const noexcept {
  return m_rootNodeId;
}

bool UIStashController::IsInGameplay() const noexcept {
  return m_inGameplay;
}

void UIStashController::EnqueueIntent(GameUiIntent intent) {
  if (m_uiHost != nullptr) {
    m_uiHost->EnqueueIntent(std::move(intent));
  }
}

void UIStashController::ResetSessionState() noexcept {
  m_activeType = NoMoreDay::StashType::Personal;
  m_activeTabIndex = 0;
  m_searchBuffer[0] = '\0';
  m_isSearchFocused = false;
  m_unlockButtonHovered = false;
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
