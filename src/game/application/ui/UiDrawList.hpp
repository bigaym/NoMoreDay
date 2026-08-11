#pragma once

#include "game/application/ui/UiRuntimeTypes.hpp"

#include <cstdint>
#include <string>
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

inline constexpr std::uint32_t kNoClipIndex = 0xFFFFFFFFu;

struct UiDrawCommand {
  UiDrawKind kind = UiDrawKind::FillRect;
  UiDrawLayer layer = UiDrawLayer::Panels;
  UiId nodeId = kInvalidUiId;
  std::uint32_t clipIndex = kNoClipIndex;
  UiRect rect{};
  UiColor color{};
  UiResourceId resourceId = kInvalidUiResourceId;
  std::string text{};
  float strokeThickness = 1.0f;
};

class UiDrawList {
public:
  void Clear();
  void Reserve(std::size_t capacity);
  [[nodiscard]] std::size_t CommandCount() const noexcept;
  [[nodiscard]] bool IsEmpty() const noexcept;

  void FillRect(UiDrawLayer layer, UiId nodeId, UiRect rect, UiColor color);
  void StrokeRect(UiDrawLayer layer, UiId nodeId, UiRect rect, UiColor color,
                  float thickness = 1.0f);
  void Line(UiDrawLayer layer, UiId nodeId, UiVec2 from, UiVec2 to,
            UiColor color, float thickness = 1.0f);
  void Text(UiDrawLayer layer, UiId nodeId, std::string text, UiVec2 position,
            float fontSize, UiColor color,
            UiResourceId fontId = kInvalidUiResourceId);
  void Image(UiDrawLayer layer, UiId nodeId, UiRect rect,
             UiResourceId textureId, UiColor tint);
  void Custom(UiDrawLayer layer, UiId nodeId, UiRect bounds,
              UiResourceId painterId);

  void PushClip(UiRect clipRect);
  void PopClip();
  [[nodiscard]] std::uint32_t CurrentClipIndex() const noexcept;
  [[nodiscard]] bool ClipBalanced() const noexcept;

  [[nodiscard]] const std::vector<UiDrawCommand> &Commands() const noexcept;
  [[nodiscard]] const std::vector<UiRect> &Clips() const noexcept;

private:
  void AppendCommand(UiDrawCommand command);

  std::vector<UiDrawCommand> m_commands;
  std::vector<UiRect> m_clips;
  std::uint32_t m_clipDepth = 0;
  bool m_clipUnderflow = false;
};

} // namespace NoMoreDay::ui
