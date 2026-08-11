#include "game/application/ui/OverlayController.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "core/utils/FmtBuffer.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace NoMoreDay::ui {

OverlayController::OverlayController(UiRuntime& runtime) : m_runtime(runtime) {
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
    // Placeholder node only: the overlays still render through the legacy
    // mirrors during the U7 transition, so the node stays hidden until U8.
    (void)m_runtime.SetNodeVisible(m_rootNodeId, false);
  }
}

void OverlayController::DrawOverlays(entt::registry& registry) {
  // Keeps the legacy draw order: context menu, quantity popup, message box.
  DrawContextMenu(registry);
  DrawQuantityPopup(registry);
  DrawMessageBox();
}

// --- Context menu ---

void OverlayController::OpenContextMenu(entt::entity item, bool fromInventory,
                                        int inventoryIndex,
                                        NoMoreDay::EquipmentSlot slot) {
  m_contextMenuVisible = true;
  m_contextMenuItem = item;
  m_contextMenuPos = GetMousePosition(); // Screen-space menu position.
  m_isContextFromInventory = fromInventory;
  m_contextSourceInventoryIndex = inventoryIndex;
  m_contextSourceEquipmentSlot = slot;
  MirrorContextMenuToState();
}

void OverlayController::CloseContextMenu() {
  m_contextMenuVisible = false;
  MirrorContextMenuToState();
}

bool OverlayController::IsContextMenuVisible() const noexcept {
  return m_contextMenuVisible;
}

void OverlayController::DrawContextMenu(entt::registry& registry) {
  // Re-adopt visibility each frame: cross-layer writers (UIRenderer
  // right-click flow, hotbar/skill-tree opens, sibling closes) still toggle
  // the menu through UISystem::State during the U7 transition.
  m_contextMenuVisible = UISystem::State.showContextMenu;
  if (!m_contextMenuVisible) {
    return;
  }
  AdoptContextMenuFromState();
  UIRenderer::DrawContextMenu(UISystem::State.globalFont, UISystem::State,
                              registry, 1.0f);
  // UIRenderer picks menu items (and closes the menu) by writing State
  // directly; pick the close back up so the visibility flag stays truthful.
  m_contextMenuVisible = UISystem::State.showContextMenu;
}

void OverlayController::AdoptContextMenuFromState() {
  m_contextMenuItem = UISystem::State.contextMenuItem;
  m_contextMenuPos = UISystem::State.contextMenuPos;
  m_isContextFromInventory = UISystem::State.isContextFromInventory;
  m_contextSourceInventoryIndex = UISystem::State.contextSourceInventoryIndex;
  m_contextSourceEquipmentSlot = UISystem::State.contextSourceEquipmentSlot;
  m_isSkillContext = UISystem::State.isSkillContext;
  m_contextSourceSkillSlot = UISystem::State.contextSourceSkillSlot;
}

void OverlayController::MirrorContextMenuToState() {
  UISystem::State.showContextMenu = m_contextMenuVisible;
  UISystem::State.contextMenuItem = m_contextMenuItem;
  UISystem::State.contextMenuPos = m_contextMenuPos;
  UISystem::State.isContextFromInventory = m_isContextFromInventory;
  UISystem::State.contextSourceInventoryIndex = m_contextSourceInventoryIndex;
  UISystem::State.contextSourceEquipmentSlot = m_contextSourceEquipmentSlot;
  UISystem::State.isSkillContext = m_isSkillContext;
  UISystem::State.contextSourceSkillSlot = m_contextSourceSkillSlot;
}

// --- Quantity popup ---

void OverlayController::OpenQuantityPopup(entt::entity item, int actionType) {
  m_quantityVisible = true;
  m_quantityTargetItem = item;
  m_quantityActionType = actionType;
  m_quantityVal = 1;
  m_quantityMax = 1;
  m_quantityInputBuf[0] = '\0';
  MirrorQuantityToState();
}

void OverlayController::CloseQuantityPopup() {
  m_quantityVisible = false;
  m_quantityTargetItem = entt::null;
  m_quantityInputBuf[0] = '\0';
  m_isTyping = false;
  MirrorQuantityToState();
}

bool OverlayController::IsQuantityPopupVisible() const noexcept {
  return m_quantityVisible;
}

void OverlayController::DrawQuantityPopup(entt::registry& registry) {
  AdoptQuantityFromState();
  if (!m_quantityVisible) {
    return;
  }

  auto closeQuantityPopup = [&]() {
    m_quantityVisible = false;
    m_quantityTargetItem = entt::null;
    m_quantityInputBuf[0] = '\0';
    m_isTyping = false;
    MirrorQuantityToState();
  };

  m_isTyping = true;

  if (!registry.valid(m_quantityTargetItem) ||
      !registry.all_of<ItemComponent>(m_quantityTargetItem)) {
    closeQuantityPopup();
    return;
  }

  auto view = registry.view<PlayerTag>();
  if (view.begin() == view.end()) {
    closeQuantityPopup();
    return;
  }
  const entt::entity player = view.front();

  const auto &item = registry.get<ItemComponent>(m_quantityTargetItem);
  m_quantityMax = std::max(1, std::min(m_quantityMax, item.quantity));
  m_quantityVal = std::clamp(m_quantityVal, 1, m_quantityMax);

  while (int key = GetCharPressed()) {
    if (key >= '0' && key <= '9') {
      const size_t len = std::strlen(m_quantityInputBuf);
      if (len + 1 < sizeof(m_quantityInputBuf)) {
        m_quantityInputBuf[len] = (char)key;
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

  const int wheelDelta = (int)GetMouseWheelMove();
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

  const float popupW = 320.0f;
  const float popupH = 190.0f;
  const float x = (float)GetScreenWidth() * 0.5f - popupW * 0.5f;
  const float y = (float)GetScreenHeight() * 0.5f - popupH * 0.5f;

  DrawRectangle((int)x, (int)y, (int)popupW, (int)popupH, Fade(BLACK, 0.88f));
  DrawRectangleLinesEx({x, y, popupW, popupH}, 1.5f, Fade(WHITE, 0.75f));

  const char *actionLabel = m_quantityActionType == 1 ? "销毁数量" : "丢弃数量";
  DrawText(actionLabel, (int)(x + 14), (int)(y + 12), 24, WHITE);
  DrawText(item.name.c_str(), (int)(x + 14), (int)(y + 48), 20, LIGHTGRAY);

  char rangeText[64] = {0};
  utils::FormatToBuffer(rangeText, "范围: 1 - {}", m_quantityMax);
  DrawText(rangeText, (int)(x + 14), (int)(y + 78), 18, GRAY);

  char valueText[64] = {0};
  utils::FormatToBuffer(valueText, "数量: {}", m_quantityVal);
  DrawText(valueText, (int)(x + 14), (int)(y + 104), 24, GOLD);

  const Rectangle confirmRect = {x + 14.0f, y + popupH - 52.0f, 136.0f, 36.0f};
  const Rectangle cancelRect = {x + popupW - 150.0f, y + popupH - 52.0f, 136.0f, 36.0f};
  const Vector2 mouse = GetMousePosition();

  const bool confirmHovered = CheckCollisionPointRec(mouse, confirmRect);
  const bool cancelHovered = CheckCollisionPointRec(mouse, cancelRect);

  DrawRectangleRec(confirmRect, confirmHovered ? DARKGREEN : Fade(DARKGREEN, 0.8f));
  DrawRectangleRec(cancelRect, cancelHovered ? MAROON : Fade(MAROON, 0.8f));
  DrawText("确认", (int)(confirmRect.x + 48), (int)(confirmRect.y + 8), 20, WHITE);
  DrawText("取消", (int)(cancelRect.x + 48), (int)(cancelRect.y + 8), 20, WHITE);

  bool confirmAction = false;
  bool cancelAction = false;
  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
    confirmAction = true;
  }
  if (IsKeyPressed(KEY_ESCAPE)) {
    cancelAction = true;
  }
  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    if (confirmHovered) {
      confirmAction = true;
    } else if (cancelHovered ||
               !CheckCollisionPointRec(mouse, {x, y, popupW, popupH})) {
      cancelAction = true;
    }
  }

  if (confirmAction) {
    const int quantity = std::clamp(m_quantityVal, 1, m_quantityMax);
    if (m_quantityActionType == 1) {
      InventorySystem::destroyItem(registry, player, m_quantityTargetItem, quantity);
    } else {
      InventorySystem::dropItem(registry, player, m_quantityTargetItem, quantity);
    }
    closeQuantityPopup();
  } else if (cancelAction) {
    closeQuantityPopup();
  } else {
    MirrorQuantityToState();
  }
}

void OverlayController::AdoptQuantityFromState() {
  // Legacy writers (UIRenderer drop/destroy menu clicks) still open the popup
  // by writing UISystem::State directly; re-adopt visibility and data each
  // frame so the controller stays the single owner of the input/validation
  // logic during the transition.
  m_quantityVisible = UISystem::State.showQuantityPopup;
  m_quantityTargetItem = UISystem::State.quantityTargetItem;
  m_quantityActionType = UISystem::State.quantityActionType;
  m_quantityVal = UISystem::State.quantityVal;
  m_quantityMax = UISystem::State.quantityMax;
  std::memcpy(m_quantityInputBuf, UISystem::State.quantityInputBuf,
              sizeof(m_quantityInputBuf));
  m_isTyping = UISystem::State.isTyping;
}

void OverlayController::MirrorQuantityToState() {
  UISystem::State.showQuantityPopup = m_quantityVisible;
  UISystem::State.quantityTargetItem = m_quantityTargetItem;
  UISystem::State.quantityActionType = m_quantityActionType;
  UISystem::State.quantityVal = m_quantityVal;
  UISystem::State.quantityMax = m_quantityMax;
  std::memcpy(UISystem::State.quantityInputBuf, m_quantityInputBuf,
              sizeof(m_quantityInputBuf));
  UISystem::State.isTyping = m_isTyping;
}

// --- Message box ---

void OverlayController::ShowMessageBox(const char* text) {
  utils::FormatToBuffer(m_messageBoxText, "{}", text);
  m_messageBoxTimer = 2.0f;
  m_messageBoxVisible = true;
  MirrorMessageBoxToState();
}

void OverlayController::HideMessageBox() {
  m_messageBoxVisible = false;
  m_messageBoxTimer = 0.0f;
  MirrorMessageBoxToState();
}

bool OverlayController::IsMessageBoxVisible() const noexcept {
  return m_messageBoxVisible;
}

void OverlayController::UpdateMessageBox() {
  // The host compatibility bridge (and legacy panels) still open the message
  // box by writing UISystem::State directly; re-adopt the timer each frame.
  m_messageBoxVisible = UISystem::State.showMessageBox;
  if (!m_messageBoxVisible) {
    return;
  }
  AdoptMessageBoxFromState();
  m_messageBoxTimer -= GetFrameTime();
  if (m_messageBoxTimer <= 0.0f) {
    m_messageBoxTimer = 0.0f;
    m_messageBoxVisible = false;
  }
  MirrorMessageBoxToState();
}

void OverlayController::DrawMessageBox() {
  m_messageBoxVisible = UISystem::State.showMessageBox;
  if (!m_messageBoxVisible) {
    return;
  }
  AdoptMessageBoxFromState();
  UIRenderer::DrawMessageBox(UISystem::State.globalFont, UISystem::State, 1.0f);
}

void OverlayController::AdoptMessageBoxFromState() {
  m_messageBoxTimer = UISystem::State.messageBoxTimer;
  std::memcpy(m_messageBoxText, UISystem::State.messageBoxText,
              sizeof(m_messageBoxText));
}

void OverlayController::MirrorMessageBoxToState() {
  UISystem::State.showMessageBox = m_messageBoxVisible;
  std::memcpy(UISystem::State.messageBoxText, m_messageBoxText,
              sizeof(m_messageBoxText));
  UISystem::State.messageBoxTimer = m_messageBoxTimer;
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

  m_quantityVisible = false;
  m_quantityTargetItem = entt::null;
  m_quantityActionType = 0;
  m_quantityVal = 1;
  m_quantityMax = 1;
  m_quantityInputBuf[0] = '\0';
  m_isTyping = false;

  m_messageBoxVisible = false;
  m_messageBoxText[0] = '\0';
  m_messageBoxTimer = 0.0f;

  MirrorContextMenuToState();
  MirrorQuantityToState();
  MirrorMessageBoxToState();
}

UiId OverlayController::NodeId() const noexcept {
  return m_rootNodeId;
}

} // namespace NoMoreDay::ui
