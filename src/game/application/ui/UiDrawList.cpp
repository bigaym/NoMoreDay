#include "game/application/ui/UiDrawList.hpp"

#include <algorithm>
#include <utility>

namespace NoMoreDay::ui {

namespace {

bool LayerThenNodeLess(const UiDrawCommand &lhs, const UiDrawCommand &rhs) {
  if (lhs.layer != rhs.layer) {
    return lhs.layer < rhs.layer;
  }
  return lhs.nodeId < rhs.nodeId;
}

} // namespace

void UiDrawList::Clear() {
  m_commands.clear();
  m_clips.clear();
  m_clipDepth = 0;
  m_clipUnderflow = false;
}

void UiDrawList::Reserve(std::size_t capacity) {
  m_commands.reserve(capacity);
  m_clips.reserve(capacity);
}

std::size_t UiDrawList::CommandCount() const noexcept {
  return m_commands.size();
}

bool UiDrawList::IsEmpty() const noexcept { return m_commands.empty(); }

void UiDrawList::AppendCommand(UiDrawCommand command) {
  const auto insertPos = std::upper_bound(m_commands.begin(), m_commands.end(),
                                          command, LayerThenNodeLess);
  m_commands.insert(insertPos, std::move(command));
}

void UiDrawList::FillRect(UiDrawLayer layer, UiId nodeId, UiRect rect,
                          UiColor color) {
  UiDrawCommand command;
  command.kind = UiDrawKind::FillRect;
  command.layer = layer;
  command.nodeId = nodeId;
  command.clipIndex = CurrentClipIndex();
  command.rect = rect;
  command.color = color;
  AppendCommand(std::move(command));
}

void UiDrawList::StrokeRect(UiDrawLayer layer, UiId nodeId, UiRect rect,
                            UiColor color, float thickness) {
  UiDrawCommand command;
  command.kind = UiDrawKind::StrokeRect;
  command.layer = layer;
  command.nodeId = nodeId;
  command.clipIndex = CurrentClipIndex();
  command.rect = rect;
  command.color = color;
  command.strokeThickness = thickness;
  AppendCommand(std::move(command));
}

void UiDrawList::Line(UiDrawLayer layer, UiId nodeId, UiVec2 from, UiVec2 to,
                      UiColor color, float thickness) {
  UiDrawCommand command;
  command.kind = UiDrawKind::Line;
  command.layer = layer;
  command.nodeId = nodeId;
  command.clipIndex = CurrentClipIndex();
  command.rect = {from, {to.x - from.x, to.y - from.y}};
  command.color = color;
  command.strokeThickness = thickness;
  AppendCommand(std::move(command));
}

void UiDrawList::Text(UiDrawLayer layer, UiId nodeId, std::string text,
                      UiVec2 position, float fontSize, UiColor color,
                      UiResourceId fontId) {
  UiDrawCommand command;
  command.kind = UiDrawKind::Text;
  command.layer = layer;
  command.nodeId = nodeId;
  command.clipIndex = CurrentClipIndex();
  command.rect = {position, {fontSize, fontSize}};
  command.color = color;
  command.resourceId = fontId;
  command.text = std::move(text);
  AppendCommand(std::move(command));
}

void UiDrawList::Image(UiDrawLayer layer, UiId nodeId, UiRect rect,
                       UiResourceId textureId, UiColor tint) {
  UiDrawCommand command;
  command.kind = UiDrawKind::Image;
  command.layer = layer;
  command.nodeId = nodeId;
  command.clipIndex = CurrentClipIndex();
  command.rect = rect;
  command.color = tint;
  command.resourceId = textureId;
  AppendCommand(std::move(command));
}

void UiDrawList::Custom(UiDrawLayer layer, UiId nodeId, UiRect bounds,
                        UiResourceId painterId) {
  UiDrawCommand command;
  command.kind = UiDrawKind::Custom;
  command.layer = layer;
  command.nodeId = nodeId;
  command.clipIndex = CurrentClipIndex();
  command.rect = bounds;
  command.resourceId = painterId;
  AppendCommand(std::move(command));
}

void UiDrawList::PushClip(UiRect clipRect) {
  m_clips.push_back(clipRect);
  ++m_clipDepth;
}

void UiDrawList::PopClip() {
  if (m_clipDepth == 0) {
    m_clipUnderflow = true;
    return;
  }
  --m_clipDepth;
}

std::uint32_t UiDrawList::CurrentClipIndex() const noexcept {
  if (m_clipDepth == 0) {
    return kNoClipIndex;
  }
  return static_cast<std::uint32_t>(m_clips.size() - 1);
}

bool UiDrawList::ClipBalanced() const noexcept {
  return m_clipDepth == 0 && !m_clipUnderflow;
}

const std::vector<UiDrawCommand> &UiDrawList::Commands() const noexcept {
  return m_commands;
}

const std::vector<UiRect> &UiDrawList::Clips() const noexcept {
  return m_clips;
}

} // namespace NoMoreDay::ui
