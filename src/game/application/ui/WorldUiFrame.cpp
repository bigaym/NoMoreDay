#include "game/application/ui/WorldUiFrame.hpp"

namespace NoMoreDay::ui {

void WorldUiFrame::BeginFrame(uint64_t frameToken) {
  m_frameToken = frameToken;
  m_visibleItems.clear();
  ClearHovered();
}

void WorldUiFrame::AddItem(entt::entity entity, const Rectangle &worldRect,
                           float depth) {
  m_visibleItems.push_back({entity, worldRect, depth});
}

void WorldUiFrame::SetHovered(entt::entity entity) {
  m_hovered = entity;
  m_hasHovered = true;
}

void WorldUiFrame::ClearHovered() {
  m_hovered = entt::null;
  m_hasHovered = false;
}

} // namespace NoMoreDay::ui
