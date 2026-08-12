#pragma once

#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/foundation/data/BladeMasteryData.hpp"

#include <cstdint>
#include <string_view>

namespace NoMoreDay::systems::ui {

// R5: the widget lives in systems::ui while the draw-list types live in
// NoMoreDay::ui; alias them here so the migrated paint path reads cleanly.
using NoMoreDay::ui::UiDrawList;
using NoMoreDay::ui::UiDrawLayer;
using NoMoreDay::ui::UiId;
using NoMoreDay::ui::UiResourceId;
using NoMoreDay::ui::UiTextAlign;
using NoMoreDay::ui::UiViewport;
using NoMoreDay::ui::UiColor;
using NoMoreDay::ui::UiRect;
using NoMoreDay::ui::kGlobalFontResourceId;
using NoMoreDay::ui::kInvalidUiResourceId;

// HUD blade-resource widget (sword intent stacks row).
//
// R5 migration: the widget no longer owns raylib resources and no longer draws
// through immediate-mode raylib. The host registers the sword icon texture
// under kSwordIntentIconResourceId; this widget holds only the resource id and
// paints through the draw list (Hud layer). Text measurement is resolved by
// the backend via UiTextAlign::Center, so the paint path stays allocation-free.
class SwordIntentWidget {
public:
  // --- Static threshold resolution (kept: tech tests assert the text) ---
  static int ResolveThresholdTier(BladeResourceKind kind, int currentStacks,
                                  int maxStacks);
  static const char* ResolveThresholdText(BladeResourceKind kind,
                                          int currentStacks, int maxStacks);
  static int ResolveSwordFlowThresholdTier(int currentStacks, int maxStacks);
  static const char* ResolveSwordFlowThresholdText(int currentStacks,
                                                   int maxStacks);

  SwordIntentWidget() = default;
  ~SwordIntentWidget() = default;

  SwordIntentWidget(const SwordIntentWidget&) = delete;
  SwordIntentWidget& operator=(const SwordIntentWidget&) = delete;

  // Sets the shared icon resource id (host registers the texture with the
  // backend at Initialize time).
  void SetIconResourceId(UiResourceId resourceId) noexcept {
    m_iconResourceId = resourceId;
  }

  // Time-driven state update: caches the frame's values and advances the
  // glow/pulse animation with the provided frame time (no raylib calls).
  void Update(int currentStacks, int maxStacks, BladeResourceKind kind,
              std::string_view label, std::string_view detailText,
              float timeSeconds, float deltaSeconds);

  // Paints the widget (label/threshold/detail text + stack icons) into the
  // draw list at the HUD layer.
  void Paint(UiDrawList& drawList, const UiViewport& viewport) const;

  // Exposed for tests (UITests [Tech] SkillUI asserts static resolution).
  [[nodiscard]] int CurrentStacks() const noexcept { return m_currentStacks; }
  [[nodiscard]] int MaxStacks() const noexcept { return m_maxStacks; }

private:
  UiResourceId m_iconResourceId = kInvalidUiResourceId;
  int m_currentStacks = 0;
  int m_maxStacks = 0;
  BladeResourceKind m_kind = BladeResourceKind::None;
  // Labels are views into controller-owned buffers (PlayerHudController
  // caches the resolved strings); valid for the lifetime of that frame's
  // controller state.
  std::string_view m_label;
  std::string_view m_detailText;
  float m_glowIntensity = 0.0f;
  float m_timeSeconds = 0.0f;
};

} // namespace NoMoreDay::systems::ui
