#include "game/application/ui/UiDrawList.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

// R10 (B-R0-1): Tracy instrumentation, active only when TRACY_PROFILING=ON
// (no-op macros otherwise).
#include <tracy/Tracy.hpp>

namespace NoMoreDay::ui {

namespace {

// Total draw order: layer, then stable node id, then the monotonic append
// sequence. The append sequence makes the comparator a strict total order, so
// Finalize() is deterministic across identical builds without relying on
// std::sort stability (C-01 remediation: replaces the per-command O(n)
// upper_bound/insert with a single in-place sort).
bool TotalDrawOrderLess(const UiDrawCommand &lhs, const UiDrawCommand &rhs) {
  if (lhs.layer != rhs.layer) {
    return lhs.layer < rhs.layer;
  }
  if (lhs.nodeId != rhs.nodeId) {
    return lhs.nodeId < rhs.nodeId;
  }
  return lhs.appendSequence < rhs.appendSequence;
}

} // namespace

void UiDrawList::Clear() {
  m_commands.clear();
  m_clips.clear();
  m_textCursor = 0;  // arena bytes are reused, never reallocated
  m_clipDepth = 0;
  m_clipUnderflow = false;
  m_finalized = false;
}

void UiDrawList::Reserve(std::size_t capacity) {
  m_commands.reserve(capacity);
  m_clips.reserve(capacity);
}

void UiDrawList::ReserveText(std::size_t byteCapacity) {
  m_textArena.resize(byteCapacity);
  m_textCursor = 0;
}

void UiDrawList::Finalize() {
  std::sort(m_commands.begin(), m_commands.end(), TotalDrawOrderLess);
  m_finalized = true;
}

bool UiDrawList::IsFinalized() const noexcept { return m_finalized; }

std::size_t UiDrawList::CommandCount() const noexcept {
  return m_commands.size();
}

bool UiDrawList::IsEmpty() const noexcept { return m_commands.empty(); }

void UiDrawList::AppendCommand(UiDrawCommand command) {
  ZoneScopedN("UiDrawList::AppendCommand");
  command.appendSequence = m_appendSequence++;
  m_finalized = false;
  // Hard capacity: overflow is recorded as telemetry and the command is
  // dropped. The steady frame path never reallocates (host reserves at
  // Initialize time; capacity is measurable and tunable).
  if (m_commands.size() >= m_commands.capacity()) {
    ++m_commandOverflow;
    return;
  }
  m_commands.push_back(std::move(command));
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

void UiDrawList::Text(UiDrawLayer layer, UiId nodeId, std::string_view text,
                      UiVec2 position, float fontSize, UiColor color,
                      UiResourceId fontId, UiTextAlign align) {
  UiDrawCommand command;
  command.kind = UiDrawKind::Text;
  command.layer = layer;
  command.nodeId = nodeId;
  command.clipIndex = CurrentClipIndex();
  command.rect = {position, {fontSize, fontSize}};
  command.color = color;
  command.resourceId = fontId;
  command.textAlign = align;

  if (!text.empty() &&
      text.size() <= static_cast<std::size_t>(std::numeric_limits<
                                              std::uint16_t>::max())) {
    // One extra byte is required for the NUL terminator: the arena is a
    // recycled buffer, so a C-string read (backend DrawTextEx, tests) must
    // never run into stale bytes from a previous frame.
    const std::size_t available = m_textArena.size() - m_textCursor;
    if (text.size() + 1 <= available) {
      std::memcpy(m_textArena.data() + m_textCursor, text.data(), text.size());
      m_textArena[m_textCursor + text.size()] = '\0';
      command.textOffset = static_cast<std::uint32_t>(m_textCursor);
      command.textLength = static_cast<std::uint16_t>(text.size());
      // Advance past the payload AND its NUL terminator: the arena is a
      // recycled buffer, so every C-string read (backend DrawTextEx, tests)
      // must see the terminator immediately after its own payload. Without
      // the +1 the next Text() memcpy would overwrite this NUL, making every
      // command but the last read into the following payloads.
      m_textCursor += text.size() + 1;
    } else {
      // Overflow: recorded as telemetry, no reallocation. The command is kept
      // with an empty text payload so the paint path stays intact.
      ++m_textOverflow;
    }
  } else if (text.size() >
             static_cast<std::size_t>(std::numeric_limits<
                                      std::uint16_t>::max())) {
    ++m_textOverflow;
  }

  AppendCommand(std::move(command));
}

void UiDrawList::Image(UiDrawLayer layer, UiId nodeId, UiRect rect,
                       UiResourceId textureId, UiColor tint,
                       UiRect sourceRect) {
  UiDrawCommand command;
  command.kind = UiDrawKind::Image;
  command.layer = layer;
  command.nodeId = nodeId;
  command.clipIndex = CurrentClipIndex();
  command.rect = rect;
  command.sourceRect = sourceRect;
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
  if (m_clips.size() >= m_clips.capacity()) {
    ++m_clipOverflow;
    // Keep the depth balanced; commands painted inside the overflowed clip
    // clamp to the last valid clip region. Overflow is loud in telemetry.
    ++m_clipDepth;
    return;
  }
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

const char *UiDrawList::TextAt(const UiDrawCommand &command) const noexcept {
  if (command.textLength == 0) {
    return "";
  }
  const std::size_t end = static_cast<std::size_t>(command.textOffset) +
                          static_cast<std::size_t>(command.textLength);
  if (end > m_textArena.size()) {
    return "";
  }
  return m_textArena.data() + command.textOffset;
}

std::size_t UiDrawList::CommandCapacity() const noexcept {
  return m_commands.capacity();
}

std::size_t UiDrawList::ClipCapacity() const noexcept {
  return m_clips.capacity();
}

std::size_t UiDrawList::TextCapacity() const noexcept {
  return m_textArena.size();
}

std::size_t UiDrawList::TextBytesUsed() const noexcept {
  return m_textCursor;
}

std::size_t UiDrawList::CommandOverflow() const noexcept {
  return m_commandOverflow;
}

std::size_t UiDrawList::ClipOverflow() const noexcept {
  return m_clipOverflow;
}

std::size_t UiDrawList::TextOverflow() const noexcept {
  return m_textOverflow;
}

} // namespace NoMoreDay::ui
