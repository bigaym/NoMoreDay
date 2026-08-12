#pragma once

#include "game/application/ui/UiRuntimeTypes.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace NoMoreDay::ui {

enum class UiDrawLayer : std::int32_t {
  Hud = 0,
  Panels = 1,
  DragPreview = 2,
  Modal = 3,
  Tooltip = 4,
  Debug = 5,
};

enum class UiDrawKind : std::uint8_t {
  FillRect,
  StrokeRect,
  Line,
  Text,
  Image,
  Custom,
};

// Horizontal text alignment (R5): resolved by the backend with a font
// measurement so the paint path stays allocation-free (no MeasureText on the
// controller side; the draw list only carries the requested alignment).
enum class UiTextAlign : std::uint8_t {
  Left = 0,
  Center = 1,
  Right = 2,
};

inline constexpr std::uint32_t kNoClipIndex = 0xFFFFFFFFu;

// A single draw command. Commands are appended in paint order and finalized
// (sorted by layer / node / append sequence) before the backend submits them.
//
// Text is NOT owned by the command: it references the draw list's fixed-capacity
// text arena through (textOffset, textLength). The arena bytes are valid until
// the next Clear(). This keeps every command trivially copyable and the steady
// frame path allocation-free (C-01 remediation).
struct UiDrawCommand {
  UiDrawKind kind = UiDrawKind::FillRect;
  UiDrawLayer layer = UiDrawLayer::Panels;
  UiId nodeId = kInvalidUiId;
  std::uint32_t clipIndex = kNoClipIndex;
  UiRect rect{};
  UiRect sourceRect{};   // Image-only: texture crop (empty = full texture).
  UiColor color{};
  UiResourceId resourceId = kInvalidUiResourceId;
  std::uint32_t textOffset = 0;   // into the draw-list text arena
  std::uint16_t textLength = 0;   // 0 = no text payload
  UiTextAlign textAlign = UiTextAlign::Left; // backend-resolved alignment
  std::uint32_t appendSequence = 0;  // monotonic append order (stable tiebreak)
  float strokeThickness = 1.0f;
};

// Default capacities (commands + clips) when the host does not reserve larger
// buffers. The host overrides these at Initialize time; capacities are
// measurable through the accessors below and overflow is recorded as telemetry
// instead of growing on the hot path.
inline constexpr std::size_t kUiDrawListDefaultCommandCapacity = 64;
inline constexpr std::size_t kUiDrawListDefaultClipCapacity = 16;
inline constexpr std::size_t kUiDrawListDefaultTextCapacity = 4096;

class UiDrawList {
public:
  // Pre-allocates the default capacities (see kUiDrawListDefault*Capacity) so
  // an unreserved list is still usable; the host overrides with Reserve /
  // ReserveText at Initialize time.
  UiDrawList()
      : UiDrawList(kUiDrawListDefaultCommandCapacity,
                   kUiDrawListDefaultClipCapacity,
                   kUiDrawListDefaultTextCapacity) {}

  // Explicit capacities. Used by tests to drive overflow telemetry with tiny
  // buffers and by alternative hosts; Reserve / ReserveText still grow from
  // here. Capacity is measurable through the accessors below.
  UiDrawList(std::size_t commandCapacity, std::size_t clipCapacity,
             std::size_t textCapacity) {
    m_commands.reserve(commandCapacity);
    m_clips.reserve(clipCapacity);
    m_textArena.resize(textCapacity);
  }

  void Clear();
  // Reserves capacity for commands and clips. Overflow beyond the reserved
  // capacity is dropped and recorded (no reallocation on the hot path).
  void Reserve(std::size_t capacity);
  // Fixes the text arena size (bytes). Text payloads that do not fit are
  // dropped and recorded (no reallocation on the hot path).
  void ReserveText(std::size_t byteCapacity);
  // Sorts commands into the total draw order (layer, nodeId, appendSequence).
  // In-place, allocation-free; the backend must only submit after Finalize.
  void Finalize();
  // True after Finalize() until the next Clear() or append. The backend
  // requires a finalized list before submitting.
  [[nodiscard]] bool IsFinalized() const noexcept;
  [[nodiscard]] std::size_t CommandCount() const noexcept;
  [[nodiscard]] bool IsEmpty() const noexcept;

  void FillRect(UiDrawLayer layer, UiId nodeId, UiRect rect, UiColor color);
  void StrokeRect(UiDrawLayer layer, UiId nodeId, UiRect rect, UiColor color,
                  float thickness = 1.0f);
  void Line(UiDrawLayer layer, UiId nodeId, UiVec2 from, UiVec2 to,
            UiColor color, float thickness = 1.0f);
  // Copies `text` into the draw list's text arena. The copy is valid until the
  // next Clear() and is read back through TextAt(command). Horizontal alignment
  // is resolved by the backend (measurement stays out of the paint path).
  void Text(UiDrawLayer layer, UiId nodeId, std::string_view text,
            UiVec2 position, float fontSize, UiColor color,
            UiResourceId fontId = kInvalidUiResourceId,
            UiTextAlign align = UiTextAlign::Left);
  // Draws a texture. `sourceRect` crops the texture (empty = full texture);
  // the destination is `rect` (logical space, scaled by the backend).
  void Image(UiDrawLayer layer, UiId nodeId, UiRect rect,
             UiResourceId textureId, UiColor tint,
             UiRect sourceRect = {});
  void Custom(UiDrawLayer layer, UiId nodeId, UiRect bounds,
              UiResourceId painterId);

  void PushClip(UiRect clipRect);
  void PopClip();
  [[nodiscard]] std::uint32_t CurrentClipIndex() const noexcept;
  [[nodiscard]] bool ClipBalanced() const noexcept;

  [[nodiscard]] const std::vector<UiDrawCommand> &Commands() const noexcept;
  [[nodiscard]] const std::vector<UiRect> &Clips() const noexcept;

  // Returns the arena text referenced by `command` ("" when the command has no
  // text payload). Valid until the next Clear().
  [[nodiscard]] const char *TextAt(const UiDrawCommand &command) const noexcept;

  // Capacity and overflow telemetry (measurable in tests).
  [[nodiscard]] std::size_t CommandCapacity() const noexcept;
  [[nodiscard]] std::size_t ClipCapacity() const noexcept;
  [[nodiscard]] std::size_t TextCapacity() const noexcept;
  [[nodiscard]] std::size_t TextBytesUsed() const noexcept;
  [[nodiscard]] std::size_t CommandOverflow() const noexcept;
  [[nodiscard]] std::size_t ClipOverflow() const noexcept;
  [[nodiscard]] std::size_t TextOverflow() const noexcept;

private:
  void AppendCommand(UiDrawCommand command);

  std::vector<UiDrawCommand> m_commands;
  std::vector<UiRect> m_clips;
  std::vector<char> m_textArena;
  std::size_t m_textCursor = 0;
  std::uint32_t m_appendSequence = 0;
  std::uint32_t m_clipDepth = 0;
  bool m_clipUnderflow = false;
  bool m_finalized = false;
  std::size_t m_commandOverflow = 0;
  std::size_t m_clipOverflow = 0;
  std::size_t m_textOverflow = 0;
};

} // namespace NoMoreDay::ui
