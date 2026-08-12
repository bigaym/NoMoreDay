#pragma once

#include <cstdint>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

namespace NoMoreDay::ui {

// Frame-scoped world UI bridge (U8, plan §11): replaces the UiShared static
// slots (VisibleItemCache / HoveredItem) with an object-owned, frame-scoped
// frame object.
//
// Ownership: the composition root (Game) owns one instance and binds it to
// both GameplayRenderAdapter (write side, UIWorldPass) and GameUiHost (read
// side, PrepareRender / hover / tooltip / ground pickup). The frame is
// strictly frame-scoped: BeginFrame opens a new frame with a fresh frame
// token, and consumers must not read a stale previous frame's data (see
// WorldUiFrameTests for the frame token / lifetime contract).
//
// Direction contract (design §4.1):
//  - visible item proxies: render adapter writes, UI reads;
//  - hovered item: UI writes, render adapter reads (next frame highlight).
class WorldUiFrame {
public:
  // World-space hit proxy for one visible loot/gold item, produced by the
  // render adapter's UIWorldPass and consumed by the UI hover / pickup passes.
  struct ItemProxy {
    entt::entity entity = entt::null;
    Rectangle worldRect = {};
    float depth = 0.0f; // draw/pick priority (reserved; 0 = unset)
  };

  // --- Write side (GameplayRenderAdapter::ExecuteUIWorldPass) ---
  // Opens a new frame: clears item proxies and hover, bumps the frame token.
  void BeginFrame(uint64_t frameToken);
  void AddItem(entt::entity entity, const Rectangle &worldRect,
               float depth = 0.0f);
  void SetHovered(entt::entity entity);
  void ClearHovered();

  // --- Read side (GameUiHost / TooltipController / render highlight) ---
  [[nodiscard]] uint64_t FrameToken() const noexcept { return m_frameToken; }
  [[nodiscard]] const std::vector<ItemProxy> &VisibleItems() const noexcept {
    return m_visibleItems;
  }
  [[nodiscard]] entt::entity HoveredItem() const noexcept {
    return m_hasHovered ? m_hovered : entt::null;
  }
  [[nodiscard]] bool HasHovered() const noexcept { return m_hasHovered; }
  [[nodiscard]] bool IsValid() const noexcept { return m_frameToken != 0; }

private:
  std::vector<ItemProxy> m_visibleItems;
  entt::entity m_hovered = entt::null;
  bool m_hasHovered = false;
  uint64_t m_frameToken = 0;
};

} // namespace NoMoreDay::ui
