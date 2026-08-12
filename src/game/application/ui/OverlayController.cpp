#include "game/application/ui/OverlayController.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "core/utils/FmtBuffer.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace NoMoreDay::ui {

namespace {

// R6: raylib Color (UITheme) -> draw-list UiColor.
inline UiColor ToUiColor(::Color color) noexcept {
  return UiColor{color.r, color.g, color.b, color.a};
}

} // namespace

OverlayController::OverlayController(UiRuntime& runtime, GameUiHost* uiHost)
    : m_runtime(runtime), m_uiHost(uiHost) {
  UiNodeDesc desc;
  desc.id = entt::hashed_string("ui_overlay");
  desc.parent = kRootUiId;
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.visible = true;
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
    // R4: the node is the message box carrier, not a placeholder. It starts
    // hidden and ReconcileRuntime() drives its visibility each Update from the
    // message box state (visible only while the message box is up).
    (void)m_runtime.SetNodeVisible(m_rootNodeId, m_messageBoxVisible);
  }
}

// --- Context menu ---

void OverlayController::OpenContextMenu(entt::entity item, bool fromInventory,
                                        int inventoryIndex,
                                        NoMoreDay::EquipmentSlot slot) {
  m_contextMenuVisible = true;
  m_contextMenuItem = item;
  // R6: the menu position is kept in logical space (the interaction and the
  // paint both run in logical coordinates through the viewport transform).
  m_contextMenuPos = UISystem::GetMousePositionLogic();
  m_isContextFromInventory = fromInventory;
  m_contextSourceInventoryIndex = inventoryIndex;
  m_contextSourceEquipmentSlot = slot;
  // Item menus are not skill menus; reset the skill-menu markers so a stale
  // skill context can never leak into an item menu.
  m_isSkillContext = false;
  m_contextSourceSkillSlot = -1;
  m_contextItemValid = false;
  m_contextEntryCount = 0;
}

void OverlayController::OpenSkillContextMenu(int skillSlot) {
  m_contextMenuVisible = true;
  m_isSkillContext = true;
  m_contextSourceSkillSlot = skillSlot;
  m_contextMenuPos = UISystem::GetMousePositionLogic();
  m_contextMenuItem = entt::null;
  m_isContextFromInventory = false;
}

void OverlayController::CloseContextMenu() {
  m_contextMenuVisible = false;
  m_contextItemValid = false;
  m_contextEntryCount = 0;
  m_contextHoverEntry = -1;
  m_skillHoverIndex = -1;
}

bool OverlayController::IsContextMenuVisible() const noexcept {
  return m_contextMenuVisible;
}

int OverlayController::ContextMenuButtonCount() const noexcept {
  return m_contextEntryCount;
}

void OverlayController::ActivateContextMenuButton(int index) {
  if (!m_contextMenuVisible || m_isSkillContext || index < 0 ||
      index >= m_contextEntryCount) {
    return;
  }
  const ContextMenuEntry& entry = m_contextEntries[static_cast<std::size_t>(index)];
  switch (entry.action) {
    case OverlayContextAction::Equip: {
      // Bag items route through the handler's bag branch (equipBag to the
      // first empty bag slot, legacy fallback semantics); other equipment
      // types go through equipItem. The handler re-validates ownership and
      // slot constraints in the next Update phase.
      if (m_uiHost != nullptr) {
        GameUiIntent intent;
        intent.sourceNode = m_rootNodeId;
        intent.kind = GameUiIntentKind::EquipItem;
        intent.payload.sourceDomainId = ContextMenuItemDomainId();
        intent.payload.itemSource =
            static_cast<std::uint8_t>(GameUiItemSource::Inventory);
        m_uiHost->EnqueueIntent(std::move(intent));
      }
      break;
    }
    case OverlayContextAction::Use: {
      if (m_uiHost != nullptr) {
        GameUiIntent intent;
        intent.sourceNode = m_rootNodeId;
        intent.kind = GameUiIntentKind::UseItem;
        intent.payload.sourceDomainId = ContextMenuItemDomainId();
        m_uiHost->EnqueueIntent(std::move(intent));
      }
      break;
    }
    case OverlayContextAction::Unequip: {
      if (m_uiHost != nullptr) {
        GameUiIntent intent;
        intent.sourceNode = m_rootNodeId;
        intent.kind = GameUiIntentKind::UnequipItem;
        intent.payload.equipmentSlot =
            static_cast<std::uint8_t>(m_contextSourceEquipmentSlot);
        m_uiHost->EnqueueIntent(std::move(intent));
      }
      break;
    }
    case OverlayContextAction::Craft: {
      // UI-local action: select the crafting target on the host-owned
      // crafting panel (the SharedContext callback route used by the legacy
      // DrawContextMenu is owned by the host channel).
      if (m_uiHost != nullptr) {
        m_uiHost->CraftingSetTargetItem(m_contextMenuItem);
      }
      break;
    }
    case OverlayContextAction::Drop: {
      if (m_isContextFromInventory && m_contextItemQuantity > 1) {
        // Stacked item: open the quantity popup (UI session state only); the
        // actual drop intent is enqueued when the user confirms.
        OpenQuantityPopup(m_contextMenuItem, 0, m_contextItemQuantity);
      } else if (m_uiHost != nullptr) {
        // Whole stack (single item or non-inventory source): drop the full
        // stack through the intent; the handler clears the domain id when the
        // stack is consumed.
        GameUiIntent intent;
        intent.sourceNode = m_rootNodeId;
        intent.kind = GameUiIntentKind::DropItem;
        intent.payload.sourceDomainId = ContextMenuItemDomainId();
        intent.payload.quantity = std::max(1, m_contextItemQuantity);
        m_uiHost->EnqueueIntent(std::move(intent));
      }
      break;
    }
    case OverlayContextAction::Lock: {
      if (m_uiHost != nullptr) {
        GameUiIntent intent;
        intent.sourceNode = m_rootNodeId;
        intent.kind = m_contextItemLocked ? GameUiIntentKind::UnlockItem
                                          : GameUiIntentKind::LockItem;
        intent.payload.sourceDomainId = ContextMenuItemDomainId();
        m_uiHost->EnqueueIntent(std::move(intent));
      }
      break;
    }
    case OverlayContextAction::Cancel:
      break;
  }
  CloseContextMenu();
}

// --- Quantity popup ---

void OverlayController::OpenQuantityPopup(entt::entity item, int actionType,
                                          int quantityMax) {
  m_quantityVisible = true;
  m_quantityTargetItem = item;
  m_quantityActionType = actionType;
  m_quantityVal = 1;
  m_quantityMax = std::max(1, quantityMax);
  m_quantityInputBuf[0] = '\0';
  // R6: the popup owns the typing gate for the whole popup lifetime (the
  // legacy code set this flag only while DrawQuantityPopup executed).
  m_isTyping = true;
}

void OverlayController::CloseQuantityPopup() {
  m_quantityVisible = false;
  m_quantityTargetItem = entt::null;
  m_quantityInputBuf[0] = '\0';
  m_quantityItemName[0] = '\0';
  m_isTyping = false;
  m_popupConfirmHovered = false;
  m_popupCancelHovered = false;
}

bool OverlayController::IsQuantityPopupVisible() const noexcept {
  return m_quantityVisible;
}

void OverlayController::ConfirmQuantityPopup() {
  if (!m_quantityVisible) {
    return;
  }
  const int quantity = std::clamp(m_quantityVal, 1, m_quantityMax);
  if (m_uiHost != nullptr) {
    GameUiIntent intent;
    intent.sourceNode = m_rootNodeId;
    intent.kind = m_quantityActionType == 1 ? GameUiIntentKind::DestroyItem
                                            : GameUiIntentKind::DropItem;
    intent.payload.sourceDomainId = entt::to_integral(m_quantityTargetItem);
    intent.payload.quantity = quantity;
    m_uiHost->EnqueueIntent(std::move(intent));
  }
  // R6: the popup UI state is a session state; the execution happens in the
  // next Update phase handler stage (design §3.1 frame order).
  CloseQuantityPopup();
}

void OverlayController::CancelQuantityPopup() {
  CloseQuantityPopup();
}

// --- Message box ---

void OverlayController::ShowMessageBox(const char* text) {
  utils::FormatToBuffer(m_messageBoxText, "{}", text);
  m_messageBoxTimer = 2.0f;
  m_messageBoxVisible = true;
}

void OverlayController::HideMessageBox() {
  m_messageBoxVisible = false;
  m_messageBoxTimer = 0.0f;
}

bool OverlayController::IsMessageBoxVisible() const noexcept {
  return m_messageBoxVisible;
}

void OverlayController::UpdateMessageBox() {
  if (!m_messageBoxVisible) {
    return;
  }
  m_messageBoxTimer -= GetFrameTime();
  if (m_messageBoxTimer <= 0.0f) {
    m_messageBoxTimer = 0.0f;
    m_messageBoxVisible = false;
  }
}

void OverlayController::ReconcileRuntime() {
  // R4 (remediation, design §3.1/§3.4): reconcile the retained overlay root
  // node with the session state before the runtime UpdateInput/Arrange steps.
  // The message box is the R4 migrated surface: the node is visible exactly
  // while the message box is up. The context menu and quantity popup are
  // hit-test-invisible surfaces carried on the same node (R6).
  if (m_rootNodeId != kInvalidUiId) {
    (void)m_runtime.SetNodeVisible(m_rootNodeId, m_messageBoxVisible);
  }
}

// --- R6 interaction phase (host Update, before HandleEscape) ---

void OverlayController::UpdateOverlays(entt::registry& registry,
                                       const UiViewport& viewport) {
  if (m_contextMenuVisible) {
    UpdateContextMenuInteraction(registry, viewport);
  }
  if (m_quantityVisible) {
    UpdateQuantityPopupInteraction(registry, viewport);
  }
}

void OverlayController::RefreshContextMenuDisplay(entt::registry& registry) {
  m_contextItemValid = false;
  m_contextItemType = 0;
  m_contextItemLocked = false;
  m_contextItemQuantity = 0;
  if (!registry.valid(m_contextMenuItem) ||
      !registry.all_of<ItemComponent>(m_contextMenuItem)) {
    return;
  }
  const auto& item = registry.get<ItemComponent>(m_contextMenuItem);
  m_contextItemValid = true;
  m_contextItemType = static_cast<std::uint8_t>(item.type);
  m_contextItemLocked = item.isLocked;
  m_contextItemQuantity = std::max(0, item.quantity);
}

void OverlayController::BuildContextMenuEntries() {
  // Mirrors the legacy UIRenderer::DrawContextMenu button set (see the R6
  // design for the mapping). Labels are static literals: no per-frame
  // allocation.
  const UITheme& theme = UIRenderer::GetTheme();
  m_contextEntryCount = 0;
  auto push = [&](OverlayContextAction action, const char* label, UiColor color) {
    if (m_contextEntryCount < static_cast<int>(m_contextEntries.size())) {
      m_contextEntries[static_cast<std::size_t>(m_contextEntryCount)] =
          ContextMenuEntry{action, label, color, UiRect{}};
      ++m_contextEntryCount;
    }
  };

  const bool fromInventory = m_isContextFromInventory;
  const ItemType type = static_cast<ItemType>(m_contextItemType);
  if (fromInventory &&
      (type == ItemType::Weapon || type == ItemType::Armor ||
       type == ItemType::Shield || type == ItemType::Jewelry ||
       type == ItemType::Bag)) {
    push(OverlayContextAction::Equip, "\xe8\xa3\x85\xe5\xa4\x87",
         ToUiColor(theme.textPrimary));
  } else if (fromInventory && type == ItemType::Consumable) {
    push(OverlayContextAction::Use, "\xe4\xbd\xbf\xe7\x94\xa8",
         ToUiColor(theme.textPrimary));
  }
  if (!fromInventory &&
      m_contextSourceEquipmentSlot != NoMoreDay::EquipmentSlot::None) {
    push(OverlayContextAction::Unequip, "\xe5\x8d\xb8\xe4\xb8\x8b",
         ToUiColor(theme.textPrimary));
  }
  if (type == ItemType::Weapon || type == ItemType::Armor ||
      type == ItemType::Shield || type == ItemType::Jewelry) {
    push(OverlayContextAction::Craft, "\xe5\x88\xb6\xe4\xbd\x9c",
         ToUiColor(GOLD));
  }
  push(OverlayContextAction::Drop, "\xe4\xb8\xa2\xe5\xbc\x83",
       ToUiColor(theme.danger));
  push(OverlayContextAction::Lock,
       m_contextItemLocked ? "\xe8\xa7\xa3\xe9\x94\x81 (Unlock)"
                           : "\xe9\x94\x81\xe5\xae\x9a (Lock)",
       ToUiColor(m_contextItemLocked ? GREEN : GOLD));
  // Cancel is always the last entry (mirrors the legacy menu).
  push(OverlayContextAction::Cancel, "\xe5\x8f\x96\xe6\xb6\x88",
       ToUiColor(theme.textSecondary));
}

void OverlayController::UpdateContextMenuInteraction(
    entt::registry& registry, const UiViewport& viewport) {
  const Vector2 mouse = UISystem::GetMousePositionLogic();

  if (m_isSkillContext) {
    // Skill assignment menu: every registered skill (id != 0) is a row.
    // The layout math must stay in sync with PaintContextMenu.
    constexpr float kMenuW = 220.0f;
    constexpr float kBtnH = 40.0f;
    const auto& allSkills = SkillRegistry::Get().GetAllSkills();
    const float menuH = static_cast<float>(allSkills.size()) * kBtnH + 20.0f;
    const UiVec2 logicalSize = viewport.LogicalSize();

    float sx = m_contextMenuPos.x;
    float sy = m_contextMenuPos.y;
    if (sx + kMenuW > logicalSize.x) {
      sx -= kMenuW;
    }
    if (sy + menuH > logicalSize.y) {
      sy -= menuH;
    }
    m_skillMenuRect = UiRect{{sx, sy}, {kMenuW, menuH}};
    m_skillEntryHeight = kBtnH;

    m_skillHoverIndex = -1;
    float curSY = sy + 10.0f;
    int index = 0;
    for (const auto& [id, skill] : allSkills) {
      if (id == 0) {
        continue;
      }
      const UiRect entry{{sx + 5.0f, curSY}, {kMenuW - 10.0f, kBtnH}};
      if (mouse.x >= entry.origin.x && mouse.x <= entry.Right() &&
          mouse.y >= entry.origin.y && mouse.y <= entry.Bottom()) {
        m_skillHoverIndex = index;
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
          // R8: the hotbar assignment routes through the command handler as a
          // SkillAssign intent (was the R6 transitional direct write of
          // ActiveSkillsComponent.slots[].id — the last direct gameplay write
          // in the overlay; the handler re-validates the slot and the player
          // in the next Update phase). Headless (no host) degrades to a closed
          // menu with no gameplay effect, matching the other intent sinks.
          if (m_uiHost != nullptr && m_contextSourceSkillSlot >= 0 &&
              m_contextSourceSkillSlot <
                  static_cast<int>(SkillConstants::MAX_SKILL_SLOTS)) {
            GameUiIntent intent;
            intent.sourceNode = kInvalidUiId;
            intent.kind = GameUiIntentKind::SkillAssign;
            intent.payload.skillId = id;
            intent.payload.skillTarget =
                static_cast<std::uint8_t>(GameUiSkillTarget::Hotbar);
            intent.payload.sourceSlot = m_contextSourceSkillSlot;
            m_uiHost->EnqueueIntent(std::move(intent));
            LOG_INFO("Assigned skill {} to hotbar slot {} via context menu "
                     "(intent)",
                     id, m_contextSourceSkillSlot);
          }
          CloseContextMenu();
        }
        break;
      }
      curSY += kBtnH;
      ++index;
    }

    // Click outside the menu closes it (legacy: LMB press outside).
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        (mouse.x < m_skillMenuRect.origin.x ||
         mouse.x > m_skillMenuRect.Right() ||
         mouse.y < m_skillMenuRect.origin.y ||
         mouse.y > m_skillMenuRect.Bottom())) {
      CloseContextMenu();
    }
    return;
  }

  // Item menu: refresh display + rebuild the button model, then hit-test.
  RefreshContextMenuDisplay(registry);
  if (!m_contextItemValid) {
    // Legacy DrawContextMenu self-closed when the item went invalid.
    CloseContextMenu();
    return;
  }
  BuildContextMenuEntries();

  constexpr float kMenuW = 180.0f;
  constexpr float kBtnH = 36.0f;
  const float menuH = static_cast<float>(m_contextEntryCount) * kBtnH + 20.0f;
  const UiVec2 logicalSize = viewport.LogicalSize();

  float sx = m_contextMenuPos.x;
  float sy = m_contextMenuPos.y;
  if (sx + kMenuW > logicalSize.x) {
    sx -= kMenuW;
  }
  if (sy + menuH > logicalSize.y) {
    sy -= menuH;
  }

  m_contextHoverEntry = -1;
  float curSY = sy + 10.0f;
  for (int i = 0; i < m_contextEntryCount; ++i) {
    m_contextEntries[static_cast<std::size_t>(i)].rect =
        UiRect{{sx + 5.0f, curSY}, {kMenuW - 10.0f, kBtnH}};
    curSY += kBtnH;
  }
  const UiRect menuBox{{sx, sy}, {kMenuW, menuH}};

  for (int i = 0; i < m_contextEntryCount; ++i) {
    const UiRect& entry = m_contextEntries[static_cast<std::size_t>(i)].rect;
    if (mouse.x >= entry.origin.x && mouse.x <= entry.Right() &&
        mouse.y >= entry.origin.y && mouse.y <= entry.Bottom()) {
      m_contextHoverEntry = i;
      if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ActivateContextMenuButton(i);
      }
      break;
    }
  }

  // Click outside the menu closes it (legacy: LMB or RMB press outside).
  if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
       IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) &&
      (mouse.x < menuBox.origin.x || mouse.x > menuBox.Right() ||
       mouse.y < menuBox.origin.y || mouse.y > menuBox.Bottom())) {
    CloseContextMenu();
  }
}

void OverlayController::RefreshQuantityTarget(entt::registry& registry) {
  m_quantityItemName[0] = '\0';
  if (!registry.valid(m_quantityTargetItem) ||
      !registry.all_of<ItemComponent>(m_quantityTargetItem)) {
    // Legacy DrawQuantityPopup self-closed when the target went invalid.
    CloseQuantityPopup();
    return;
  }
  const auto& item = registry.get<ItemComponent>(m_quantityTargetItem);
  m_quantityMax = std::max(1, std::min(m_quantityMax, item.quantity));
  m_quantityVal = std::clamp(m_quantityVal, 1, m_quantityMax);
  // Cache the item name for the paint (registry-free paint path).
  utils::FormatToBuffer(m_quantityItemName, "{}", item.name);
}

void OverlayController::UpdateQuantityPopupInteraction(
    entt::registry& registry, const UiViewport& viewport) {
  RefreshQuantityTarget(registry);
  if (!m_quantityVisible) {
    return;
  }

  // Text input (digits only) + wheel + arrow keys (mirrors the legacy
  // DrawQuantityPopup input handling; now runs in the Update phase).
  while (int key = GetCharPressed()) {
    if (key >= '0' && key <= '9') {
      const size_t len = std::strlen(m_quantityInputBuf);
      if (len + 1 < sizeof(m_quantityInputBuf)) {
        m_quantityInputBuf[len] = static_cast<char>(key);
        m_quantityInputBuf[len + 1] = '\0';
      }
    }
  }
  if (IsKeyPressed(KEY_BACKSPACE)) {
    const size_t len = std::strlen(m_quantityInputBuf);
    if (len > 0) {
      m_quantityInputBuf[len - 1] = '\0';
    }
  }
  if (m_quantityInputBuf[0] != '\0') {
    const int parsed = std::atoi(m_quantityInputBuf);
    m_quantityVal = std::clamp(parsed, 1, m_quantityMax);
  }
  const int wheelDelta = static_cast<int>(GetMouseWheelMove());
  if (wheelDelta != 0) {
    m_quantityVal = std::clamp(m_quantityVal + wheelDelta, 1, m_quantityMax);
    utils::FormatToBuffer(m_quantityInputBuf, "{}", m_quantityVal);
  }
  if (IsKeyPressed(KEY_UP)) {
    m_quantityVal = std::min(m_quantityVal + 1, m_quantityMax);
    utils::FormatToBuffer(m_quantityInputBuf, "{}", m_quantityVal);
  }
  if (IsKeyPressed(KEY_DOWN)) {
    m_quantityVal = std::max(m_quantityVal - 1, 1);
    utils::FormatToBuffer(m_quantityInputBuf, "{}", m_quantityVal);
  }

  // Mouse interaction on the logical-space popup geometry.
  constexpr float kPopupW = 320.0f;
  constexpr float kPopupH = 190.0f;
  const UiVec2 logicalSize = viewport.LogicalSize();
  const float x = logicalSize.x * 0.5f - kPopupW * 0.5f;
  const float y = logicalSize.y * 0.5f - kPopupH * 0.5f;

  const Rectangle confirmRect = {x + 14.0f, y + kPopupH - 52.0f, 136.0f, 36.0f};
  const Rectangle cancelRect = {x + kPopupW - 150.0f, y + kPopupH - 52.0f,
                                136.0f, 36.0f};
  const Vector2 mouse = UISystem::GetMousePositionLogic();

  m_popupConfirmHovered = CheckCollisionPointRec(mouse, confirmRect);
  m_popupCancelHovered = CheckCollisionPointRec(mouse, cancelRect);

  bool confirmAction = false;
  bool cancelAction = false;
  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
    confirmAction = true;
  }
  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    if (m_popupConfirmHovered) {
      confirmAction = true;
    } else if (m_popupCancelHovered ||
               !CheckCollisionPointRec(mouse, {x, y, kPopupW, kPopupH})) {
      cancelAction = true;
    }
  }
  // KEY_ESCAPE is handled by the host HandleEscape chain (R3), not here.

  if (confirmAction) {
    ConfirmQuantityPopup();
  } else if (cancelAction) {
    CancelQuantityPopup();
  }
}

// --- R6 paint phase (host PrepareRender, before Finalize) ---

void OverlayController::Paint(UiDrawList& drawList,
                              const UiViewport& viewport) const {
  // Paint order matches the legacy draw order: context menu, quantity popup,
  // message box (the message box was moved to the draw list in R4).
  if (m_contextMenuVisible) {
    PaintContextMenu(drawList, viewport);
  }
  if (m_quantityVisible) {
    PaintQuantityPopup(drawList, viewport);
  }
  PaintMessageBox(drawList, viewport);
}

void OverlayController::PaintContextMenu(UiDrawList& drawList,
                                         const UiViewport& viewport) const {
  if (m_rootNodeId == kInvalidUiId) {
    return;
  }
  const UITheme& theme = UIRenderer::GetTheme();

  if (m_isSkillContext) {
    // Skill menu geometry was computed by UpdateContextMenuInteraction; the
    // row iteration order matches the interaction phase (same SkillRegistry
    // container, no per-frame allocation).
    const UiRect& box = m_skillMenuRect;
    drawList.FillRect(UiDrawLayer::Modal, m_rootNodeId, box,
                      ToUiColor(Fade(theme.panelBackground, 0.98f)));
    drawList.StrokeRect(UiDrawLayer::Modal, m_rootNodeId, box,
                        ToUiColor(Fade(theme.panelBorder, 1.0f)), 1.0f);
    drawList.Line(UiDrawLayer::Modal, m_rootNodeId,
                  UiVec2{box.origin.x, box.origin.y},
                  UiVec2{box.Right(), box.origin.y},
                  ToUiColor(Fade(GOLD, 1.0f)), 2.0f);

    const auto& allSkills = SkillRegistry::Get().GetAllSkills();
    float curSY = box.origin.y + 10.0f;
    int index = 0;
    for (const auto& [id, skill] : allSkills) {
      if (id == 0) {
        continue;
      }
      const UiRect entry{{box.origin.x + 5.0f, curSY},
                         {box.size.x - 10.0f, m_skillEntryHeight}};
      if (index == m_skillHoverIndex) {
        drawList.FillRect(UiDrawLayer::Modal, m_rootNodeId, entry,
                          ToUiColor(Fade(theme.buttonHover, 0.5f)));
      }
      // Icon (identity-mapped asset id, registered by the host at Initialize).
      if (skill.icon_id != 0) {
        const float iconSize = 32.0f;
        drawList.Image(UiDrawLayer::Modal, m_rootNodeId,
                       UiRect{{box.origin.x + 10.0f,
                               entry.origin.y + (m_skillEntryHeight - iconSize) * 0.5f},
                              {iconSize, iconSize}},
                       skill.icon_id, UiColor{255, 255, 255, 255});
      }
      drawList.Text(UiDrawLayer::Modal, m_rootNodeId, skill.name_key.c_str(),
                    UiVec2{box.origin.x + 15.0f + 32.0f,
                           entry.origin.y + (m_skillEntryHeight - 18.0f) * 0.5f},
                    18.0f, UiColor{255, 255, 255, 255}, kGlobalFontResourceId);
      curSY += m_skillEntryHeight;
      ++index;
    }
    return;
  }

  // Item menu.
  const float menuW = 180.0f;
  const float btnH = 36.0f;
  const float menuH = static_cast<float>(m_contextEntryCount) * btnH + 20.0f;
  float sx = m_contextMenuPos.x;
  float sy = m_contextMenuPos.y;
  const UiVec2 logicalSize = viewport.LogicalSize();
  if (sx + menuW > logicalSize.x) {
    sx -= menuW;
  }
  if (sy + menuH > logicalSize.y) {
    sy -= menuH;
  }
  const UiRect box{{sx, sy}, {menuW, menuH}};

  drawList.FillRect(UiDrawLayer::Modal, m_rootNodeId, box,
                    ToUiColor(Fade(theme.panelBackground, 0.98f)));
  drawList.StrokeRect(UiDrawLayer::Modal, m_rootNodeId, box,
                      ToUiColor(Fade(theme.panelBorder, 1.0f)), 1.0f);
  drawList.Line(UiDrawLayer::Modal, m_rootNodeId,
                UiVec2{box.origin.x, box.origin.y},
                UiVec2{box.Right(), box.origin.y},
                ToUiColor(Fade(theme.panelBorderHighlight, 1.0f)), 2.0f);

  for (int i = 0; i < m_contextEntryCount; ++i) {
    const ContextMenuEntry& entry = m_contextEntries[static_cast<std::size_t>(i)];
    const UiRect& rect = entry.rect;
    if (i == m_contextHoverEntry) {
      drawList.FillRect(UiDrawLayer::Modal, m_rootNodeId, rect,
                        ToUiColor(Fade(theme.buttonHover, 0.5f)));
      drawList.StrokeRect(UiDrawLayer::Modal, m_rootNodeId, rect,
                          ToUiColor(Fade(theme.panelBorder, 0.5f)), 1.0f);
    }
    // Centered label (18px, same as the legacy DrawTextUI path).
    drawList.Text(UiDrawLayer::Modal, m_rootNodeId, entry.label,
                  UiVec2{rect.origin.x + 5.0f,
                         rect.origin.y + (rect.size.y - 18.0f) * 0.5f},
                  18.0f, entry.color, kGlobalFontResourceId);
  }
}

void OverlayController::PaintQuantityPopup(UiDrawList& drawList,
                                           const UiViewport& viewport) const {
  if (m_rootNodeId == kInvalidUiId) {
    return;
  }
  constexpr float kPopupW = 320.0f;
  constexpr float kPopupH = 190.0f;
  const UiVec2 logicalSize = viewport.LogicalSize();
  const float x = logicalSize.x * 0.5f - kPopupW * 0.5f;
  const float y = logicalSize.y * 0.5f - kPopupH * 0.5f;
  const UiRect box{{x, y}, {kPopupW, kPopupH}};

  drawList.FillRect(UiDrawLayer::Modal, m_rootNodeId, box,
                    UiColor{0, 0, 0, 224});
  drawList.StrokeRect(UiDrawLayer::Modal, m_rootNodeId, box,
                      UiColor{255, 255, 255, 191}, 1.5f);

  const char* actionLabel = m_quantityActionType == 1 ? "\xe9\x94\x80\xe6\xaf\x81\xe6\x95\xb0\xe9\x87\x8f" : "\xe4\xb8\xa2\xe5\xbc\x83\xe6\x95\xb0\xe9\x87\x8f";
  drawList.Text(UiDrawLayer::Modal, m_rootNodeId, actionLabel,
                UiVec2{x + 14.0f, y + 12.0f}, 24.0f, UiColor{255, 255, 255, 255},
                kGlobalFontResourceId);
  if (m_quantityItemName[0] != '\0') {
    drawList.Text(UiDrawLayer::Modal, m_rootNodeId, m_quantityItemName,
                  UiVec2{x + 14.0f, y + 48.0f}, 20.0f,
                  UiColor{211, 211, 211, 255}, kGlobalFontResourceId);
  }

  char rangeText[64] = {0};
  utils::FormatToBuffer(rangeText, "\xe8\x8c\x83\xe5\x9b\xb4: 1 - {}",
                        m_quantityMax);
  drawList.Text(UiDrawLayer::Modal, m_rootNodeId, rangeText,
                UiVec2{x + 14.0f, y + 78.0f}, 18.0f, UiColor{128, 128, 128, 255},
                kGlobalFontResourceId);

  char valueText[64] = {0};
  utils::FormatToBuffer(valueText, "\xe6\x95\xb0\xe9\x87\x8f: {}", m_quantityVal);
  drawList.Text(UiDrawLayer::Modal, m_rootNodeId, valueText,
                UiVec2{x + 14.0f, y + 104.0f}, 24.0f, UiColor{255, 215, 0, 255},
                kGlobalFontResourceId);

  const UiRect confirmRect{{x + 14.0f, y + kPopupH - 52.0f}, {136.0f, 36.0f}};
  const UiRect cancelRect{{x + kPopupW - 150.0f, y + kPopupH - 52.0f},
                          {136.0f, 36.0f}};
  // Hover state was computed by the Update phase (UpdateQuantityPopupInteraction);
  // the paint reads the cached flags instead of polling input.
  drawList.FillRect(UiDrawLayer::Modal, m_rootNodeId, confirmRect,
                    m_popupConfirmHovered ? UiColor{0, 100, 0, 255}
                                          : UiColor{0, 100, 0, 204});
  drawList.FillRect(UiDrawLayer::Modal, m_rootNodeId, cancelRect,
                    m_popupCancelHovered ? UiColor{128, 0, 0, 255}
                                         : UiColor{128, 0, 0, 204});
  drawList.Text(UiDrawLayer::Modal, m_rootNodeId,
                "\xe7\xa1\xae\xe8\xae\xa4",
                UiVec2{confirmRect.origin.x + 48.0f,
                       confirmRect.origin.y + 8.0f},
                20.0f, UiColor{255, 255, 255, 255}, kGlobalFontResourceId);
  drawList.Text(UiDrawLayer::Modal, m_rootNodeId,
                "\xe5\x8f\x96\xe6\xb6\x88",
                UiVec2{cancelRect.origin.x + 48.0f, cancelRect.origin.y + 8.0f},
                20.0f, UiColor{255, 255, 255, 255}, kGlobalFontResourceId);
}

void OverlayController::PaintMessageBox(UiDrawList& drawList,
                                        const UiViewport& viewport) const {
  if (!m_messageBoxVisible || m_messageBoxText[0] == '\0') {
    return;
  }
  // R4 (remediation, design §3.4): the message box is the first real panel
  // painted through the draw list. Geometry is deterministic fixed logical
  // size, centered in the logical viewport; the legacy UIRenderer message box
  // hugged its text (font measurement), which required raylib in the Draw
  // layer — R4 documents the fixed box as a transitional visual change and R6
  // may restore text-fitted sizing through a backend measurement API.
  constexpr float kBoxWidth = 460.0f;
  constexpr float kBoxHeight = 60.0f;
  constexpr float kFontSize = 20.0f;
  constexpr float kTextHMargin = 12.0f;
  constexpr UiColor kFrameTint{255, 255, 255, 255};
  constexpr UiColor kTextColor{0, 0, 0, 255};

  const UiVec2 logicalSize = viewport.LogicalSize();
  const UiRect box{{(logicalSize.x - kBoxWidth) * 0.5f,
                    (logicalSize.y - kBoxHeight) * 0.5f},
                   {kBoxWidth, kBoxHeight}};
  drawList.Image(UiDrawLayer::Modal, m_rootNodeId, box,
                 kMessageBoxTextureResourceId, kFrameTint);
  const UiVec2 textPos{box.origin.x + kTextHMargin,
                       box.origin.y + (kBoxHeight - kFontSize) * 0.5f};
  drawList.Text(UiDrawLayer::Modal, m_rootNodeId, m_messageBoxText, textPos,
                kFontSize, kTextColor, kGlobalFontResourceId);
}

// --- Session scoping ---

void OverlayController::EnterGameplay() {
  ResetOverlays();
}

void OverlayController::LeaveGameplay() {
  ResetOverlays();
}

void OverlayController::ResetOverlays() {
  m_contextMenuVisible = false;
  m_contextMenuItem = entt::null;
  m_contextMenuPos = {0.0f, 0.0f};
  m_isContextFromInventory = false;
  m_contextSourceInventoryIndex = -1;
  m_contextSourceEquipmentSlot = NoMoreDay::EquipmentSlot::None;
  m_isSkillContext = false;
  m_contextSourceSkillSlot = -1;
  m_contextItemValid = false;
  m_contextItemLocked = false;
  m_contextItemQuantity = 0;
  m_contextEntryCount = 0;
  m_contextHoverEntry = -1;
  m_skillHoverIndex = -1;

  m_quantityVisible = false;
  m_quantityTargetItem = entt::null;
  m_quantityActionType = 0;
  m_quantityVal = 1;
  m_quantityMax = 1;
  m_quantityInputBuf[0] = '\0';
  m_quantityItemName[0] = '\0';
  m_isTyping = false;
  m_popupConfirmHovered = false;
  m_popupCancelHovered = false;

  m_messageBoxVisible = false;
  m_messageBoxText[0] = '\0';
  m_messageBoxTimer = 0.0f;
}

UiId OverlayController::NodeId() const noexcept {
  return m_rootNodeId;
}

} // namespace NoMoreDay::ui
