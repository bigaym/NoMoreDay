#include "game/application/ui/UiRaylibBackend.hpp"

#include <cassert>
#include <cstdint>

namespace NoMoreDay::ui {
namespace {

constexpr std::int32_t kFirstLayerValue =
    static_cast<std::int32_t>(UiDrawLayer::Hud);
constexpr std::int32_t kLastLayerValue =
    static_cast<std::int32_t>(UiDrawLayer::Debug);

Color ToRaylibColor(UiColor color) noexcept {
  return Color{color.r, color.g, color.b, color.a};
}

} // namespace

void UiRaylibBackend::RegisterFont(UiResourceId id, Font font) {
  m_fonts[id] = font;
}

void UiRaylibBackend::RegisterTexture(UiResourceId id, Texture2D texture) {
  m_textures[id] = texture;
}

void UiRaylibBackend::RegisterPainter(UiResourceId id,
                                      UiCustomPainterFn painter,
                                      void *userData) {
  m_painters[id] = RegisteredPainter{painter, userData};
}

void UiRaylibBackend::Unregister(UiResourceId id) {
  m_fonts.erase(id);
  m_textures.erase(id);
  m_painters.erase(id);
}

void UiRaylibBackend::UnregisterAll() {
  m_fonts.clear();
  m_textures.clear();
  m_painters.clear();
}

bool UiRaylibBackend::IsRegistered(UiResourceId id) const noexcept {
  return m_fonts.contains(id) || m_textures.contains(id) ||
         m_painters.contains(id);
}

UiRect UiRaylibBackend::ToNativeRect(const UiViewport &viewport,
                                     UiRect logicalRect) noexcept {
  return viewport.ToPixel(logicalRect);
}

UiVec2 UiRaylibBackend::ToNativePoint(const UiViewport &viewport,
                                      UiVec2 logicalPoint) noexcept {
  return viewport.ToPixel(logicalPoint);
}

void UiRaylibBackend::Render(const UiViewport &viewport,
                             const UiDrawList &drawList) {
  if (drawList.IsEmpty()) {
    return;
  }

  assert(drawList.ClipBalanced());

  const std::vector<UiDrawCommand> &commands = drawList.Commands();
  const std::vector<UiRect> &clips = drawList.Clips();

  for (std::int32_t layerValue = kFirstLayerValue;
       layerValue <= kLastLayerValue; ++layerValue) {
    const UiDrawLayer layer = static_cast<UiDrawLayer>(layerValue);
    for (const UiDrawCommand &command : commands) {
      if (command.layer == layer) {
        DrawCommand(viewport, command, clips);
      }
    }
  }
}

void UiRaylibBackend::DrawCommand(const UiViewport &viewport,
                                  const UiDrawCommand &command,
                                  const std::vector<UiRect> &clips) {
  const bool scissored = command.clipIndex != kNoClipIndex;
  if (scissored) {
    assert(command.clipIndex < clips.size());
    if (command.clipIndex >= clips.size()) {
      return;
    }
    const UiRect nativeClip = ToNativeRect(viewport, clips[command.clipIndex]);
    BeginScissorMode(static_cast<int>(nativeClip.origin.x),
                     static_cast<int>(nativeClip.origin.y),
                     static_cast<int>(nativeClip.size.x),
                     static_cast<int>(nativeClip.size.y));
  }

  switch (command.kind) {
  case UiDrawKind::FillRect: {
    const UiRect nativeRect = ToNativeRect(viewport, command.rect);
    DrawRectangleRec({nativeRect.origin.x, nativeRect.origin.y,
                      nativeRect.size.x, nativeRect.size.y},
                     ToRaylibColor(command.color));
    break;
  }
  case UiDrawKind::StrokeRect: {
    const UiRect nativeRect = ToNativeRect(viewport, command.rect);
    DrawRectangleLinesEx({nativeRect.origin.x, nativeRect.origin.y,
                          nativeRect.size.x, nativeRect.size.y},
                         command.strokeThickness,
                         ToRaylibColor(command.color));
    break;
  }
  case UiDrawKind::Line: {
    // UiDrawCommand::rect encodes the segment: origin = start point,
    // size = end - start.
    const UiRect nativeRect = ToNativeRect(viewport, command.rect);
    const Vector2 start{static_cast<float>(nativeRect.origin.x),
                        static_cast<float>(nativeRect.origin.y)};
    const Vector2 end{static_cast<float>(nativeRect.Right()),
                      static_cast<float>(nativeRect.Bottom())};
    DrawLineEx(start, end, command.strokeThickness,
               ToRaylibColor(command.color));
    break;
  }
  case UiDrawKind::Text: {
    // UiDrawCommand::rect encodes the text: origin = position, size.x =
    // font size.
    const auto fontIt = m_fonts.find(command.resourceId);
    if (fontIt == m_fonts.end() &&
        command.resourceId != kInvalidUiResourceId) {
      break;
    }
    const Font font =
        fontIt == m_fonts.end() ? GetFontDefault() : fontIt->second;
    const UiVec2 nativePosition =
        ToNativePoint(viewport, command.rect.origin);
    DrawTextEx(font, command.text.c_str(),
               {nativePosition.x, nativePosition.y}, command.rect.size.x,
               1.0f, ToRaylibColor(command.color));
    break;
  }
  case UiDrawKind::Image: {
    const auto textureIt = m_textures.find(command.resourceId);
    if (textureIt == m_textures.end()) {
      break;
    }
    const Texture2D &texture = textureIt->second;
    if (texture.id == 0) {
      break;
    }
    const UiRect nativeRect = ToNativeRect(viewport, command.rect);
    const Rectangle source{0.0f, 0.0f,
                           static_cast<float>(texture.width),
                           static_cast<float>(texture.height)};
    const Rectangle dest{nativeRect.origin.x, nativeRect.origin.y,
                         nativeRect.size.x, nativeRect.size.y};
    DrawTexturePro(texture, source, dest, {0.0f, 0.0f}, 0.0f,
                   ToRaylibColor(command.color));
    break;
  }
  case UiDrawKind::Custom: {
    const auto painterIt = m_painters.find(command.resourceId);
    if (painterIt == m_painters.end()) {
      break;
    }
    const RegisteredPainter &painter = painterIt->second;
    if (painter.function != nullptr) {
      painter.function(painter.userData,
                       ToNativeRect(viewport, command.rect));
    }
    break;
  }
  }

  if (scissored) {
    EndScissorMode();
  }
}

} // namespace NoMoreDay::ui
