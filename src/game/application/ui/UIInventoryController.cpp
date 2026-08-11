#include "game/application/ui/UIInventoryController.hpp"

#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/systems/item/MaterialRegistry.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstdint>

namespace NoMoreDay::ui {

namespace {

inline constexpr UiId kUIInventoryRootNode =
    static_cast<UiId>(entt::hashed_string("ui_inventory_panel").value());

} // namespace

UIInventoryController::UIInventoryController(UiRuntime& runtime)
    : m_runtime(runtime) {
  // MaterialCategory is forward-declared in the header, so the "All" default
  // is applied here instead of as an in-class initializer.
  m_selectedCategory = NoMoreDay::MaterialCategory::Count;

  UiNodeDesc desc;
  desc.id = kUIInventoryRootNode;
  desc.parent = kRootUiId;
  // Full-viewport declarative anchor. The inventory panel itself is still
  // drawn by the legacy UIInventory::Draw; this node is the host-owned root
  // that a later U7 step will render into.
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

void UIInventoryController::EnterGameplay() {
  ResetSessionState();
  m_inGameplay = true;
  SetNodeVisible(true);
}

void UIInventoryController::LeaveGameplay() {
  ResetSessionState();
  m_inGameplay = false;
  SetNodeVisible(false);
}

void UIInventoryController::Update(entt::registry& registry,
                                   const LevelManager& levelManager) {
  (void)registry;
  (void)levelManager;

  // Ported from UIInventory::Update: animate the panel alpha towards the
  // target implied by UISystem::State.showInventory. Time-driven, no static
  // state.
  const float dt = GetFrameTime();
  const float alphaSpeed = 6.0f;
  if (UISystem::State.showInventory) {
    UISystem::State.inventoryAlpha =
        std::min(1.0f, UISystem::State.inventoryAlpha + dt * alphaSpeed);
  } else {
    UISystem::State.inventoryAlpha =
        std::max(0.0f, UISystem::State.inventoryAlpha - dt * alphaSpeed);
  }
}

UiId UIInventoryController::NodeId() const noexcept {
  return m_rootNodeId;
}

bool UIInventoryController::IsInGameplay() const noexcept {
  return m_inGameplay;
}

void UIInventoryController::ResetSessionState() noexcept {
  m_inventoryPage = 0;
  m_activeTab = 0;
  m_materialScrollOffset = 0.0f;
  m_searchBuffer[0] = '\0';
  m_selectedCategory = NoMoreDay::MaterialCategory::Count;
  m_isSearchFocused = false;
}

void UIInventoryController::SetNodeVisible(bool visible) {
  if (m_rootNodeId != kInvalidUiId) {
    (void)m_runtime.SetNodeVisible(m_rootNodeId, visible);
  }
}

} // namespace NoMoreDay::ui
