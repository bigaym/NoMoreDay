#pragma once

#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiViewport.hpp"

#include "raylib.h"

#include <unordered_map>
#include <vector>

namespace NoMoreDay::ui {

using UiCustomPainterFn = void (*)(void *userData, UiRect nativeBounds);

class UiRaylibBackend {
public:
  UiRaylibBackend() = default;
  ~UiRaylibBackend() = default;

  UiRaylibBackend(const UiRaylibBackend &) = delete;
  UiRaylibBackend &operator=(const UiRaylibBackend &) = delete;

  void RegisterFont(UiResourceId id, Font font);
  void RegisterTexture(UiResourceId id, Texture2D texture);
  void RegisterPainter(UiResourceId id, UiCustomPainterFn painter,
                       void *userData = nullptr);
  void Unregister(UiResourceId id);
  void UnregisterAll();

  [[nodiscard]] bool IsRegistered(UiResourceId id) const noexcept;

  [[nodiscard]] static UiRect ToNativeRect(const UiViewport &viewport,
                                           UiRect logicalRect) noexcept;
  [[nodiscard]] static UiVec2 ToNativePoint(const UiViewport &viewport,
                                            UiVec2 logicalPoint) noexcept;

  void Render(const UiViewport &viewport, const UiDrawList &drawList);

private:
  struct RegisteredPainter {
    UiCustomPainterFn function = nullptr;
    void *userData = nullptr;
  };

  void DrawCommand(const UiViewport &viewport, const UiDrawCommand &command,
                   const std::vector<UiRect> &clips, const UiDrawList &drawList);

  std::unordered_map<UiResourceId, Font> m_fonts;
  std::unordered_map<UiResourceId, Texture2D> m_textures;
  std::unordered_map<UiResourceId, RegisteredPainter> m_painters;
};

} // namespace NoMoreDay::ui
