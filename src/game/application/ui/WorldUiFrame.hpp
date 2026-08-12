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

  // Token-bound read view (R3 remediation, design §3.5). Acquired from a frame
  // via AcquireView(); IsValid() is true only while the frame's current token
  // still equals the token captured at acquisition — i.e. the view still
  // describes the very pass that produced it. A never-opened frame yields an
  // invalid view; a BeginFrame that rotates the token invalidates every view
  // acquired before it. Readers must treat an invalid view as "no world
  // target": the accessors below degrade to empty/no-hover so no stale
  // previous-pass vector can ever be consumed.
  class View {
  public:
    View() = default;
    View(const WorldUiFrame *frame, uint64_t expectedToken) noexcept
        : m_frame(frame), m_expectedToken(expectedToken) {}

    // Valid only when the frame is bound, a pass has opened it, and the
    // current frame token matches the one captured at acquisition.
    [[nodiscard]] bool IsValid() const noexcept {
      return m_frame != nullptr && m_expectedToken != 0 &&
             m_frame->FrameToken() == m_expectedToken;
    }
    [[nodiscard]] uint64_t FrameToken() const noexcept {
      return m_expectedToken;
    }
    // Empty when the view is invalid (explicit empty-view degradation).
    [[nodiscard]] const std::vector<ItemProxy> &VisibleItems() const noexcept {
      static const std::vector<ItemProxy> kEmpty;
      return IsValid() ? m_frame->VisibleItems() : kEmpty;
    }
    [[nodiscard]] entt::entity HoveredItem() const noexcept {
      return IsValid() ? m_frame->HoveredItem() : entt::null;
    }
    [[nodiscard]] bool HasHovered() const noexcept {
      return IsValid() && m_frame->HasHovered();
    }

  private:
    const WorldUiFrame *m_frame = nullptr;
    uint64_t m_expectedToken = 0;
  };

  // --- Write side (GameplayRenderAdapter::ExecuteUIWorldPass) ---
  // Opens a new frame: clears item proxies and hover, bumps the frame token.
  // Must be the first call of every pass branch (incl. GPU loot) so no reader
  // can observe a stale previous-pass frame.
  void BeginFrame(uint64_t frameToken);
  void AddItem(entt::entity entity, const Rectangle &worldRect,
               float depth = 0.0f);
  void SetHovered(entt::entity entity);
  void ClearHovered();

  // --- Read side (GameUiHost / TooltipController / render highlight) ---
  [[nodiscard]] uint64_t FrameToken() const noexcept { return m_frameToken; }
  // Captures the current pass token; readers hold the view across their
  // consume step and re-validate it against the frame every access.
  [[nodiscard]] View AcquireView() const noexcept {
    return View(this, m_frameToken);
  }
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
