#include "doctest.h"

#include "game/application/ui/SwordIntentWidget.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UiViewport.hpp"
#include "game/foundation/data/BladeMasteryData.hpp"

#include <fstream>
#include <string>

using NoMoreDay::systems::ui::SwordIntentWidget;

TEST_CASE("[Unit] SwordIntentWidget is headless-safe") {
  // R5 adaptation: Draw() is gone; the widget caches state via Update and
  // emits draw-list commands via Paint. With no registered icon resource the
  // paint path must early-out without touching GL.
  NoMoreDay::systems::ui::SwordIntentWidget widget;
  widget.Update(3, 8, NoMoreDay::BladeResourceKind::SwordIntent, "Sword Intent",
                "", 1.0f, 1.0f / 60.0f);
  NoMoreDay::ui::UiDrawList drawList;
  const NoMoreDay::ui::UiViewport viewport =
      NoMoreDay::ui::UiViewport::Fit({2560, 1440});
  widget.Paint(drawList, viewport); // kInvalidUiResourceId: no commands
  CHECK(drawList.CommandCount() == 0);

  widget.Update(0, 0, NoMoreDay::BladeResourceKind::None, "", "", 1.0f,
                1.0f / 60.0f);
  widget.Paint(drawList, viewport);
  CHECK(drawList.CommandCount() == 0);
}

TEST_CASE("[Unit] SwordIntentWidget threshold resolution is pure") {
  using Kind = NoMoreDay::BladeResourceKind;
  CHECK(SwordIntentWidget::ResolveThresholdTier(Kind::SwordIntent, 99, 10) == 0);
  CHECK(SwordIntentWidget::ResolveThresholdTier(Kind::SwordFlow, 3, 10) == 0);
  CHECK(SwordIntentWidget::ResolveThresholdTier(Kind::SwordFlow, 5, 10) == 1);
  CHECK(SwordIntentWidget::ResolveThresholdTier(Kind::SwordFlow, 8, 10) == 2);
  CHECK(SwordIntentWidget::ResolveThresholdTier(Kind::SwordFlow, 10, 10) == 3);
  CHECK(SwordIntentWidget::ResolveThresholdTier(Kind::SpiritBladeTier, 7, 10) == 2);
  CHECK(SwordIntentWidget::ResolveThresholdTier(Kind::Bloodthirst, 6, 8) == 1);

  CHECK(std::string(SwordIntentWidget::ResolveThresholdText(Kind::SwordFlow, 10, 10)) == "满流");
  CHECK(std::string(SwordIntentWidget::ResolveThresholdText(Kind::SpiritBladeTier, 5, 10)) == "灵剑成阵");
  CHECK(std::string(SwordIntentWidget::ResolveThresholdText(Kind::Bloodthirst, 5, 8)) == "血意沸腾");
  CHECK(std::string(SwordIntentWidget::ResolveThresholdText(Kind::SwordIntent, 5, 8)) == "");
  CHECK(std::string(SwordIntentWidget::ResolveSwordFlowThresholdText(5, 8)) == "剑流·启");
  CHECK(std::string(SwordIntentWidget::ResolveSwordFlowThresholdText(8, 8)) == "满流");
}

TEST_CASE("[Unit] SwordIntentWidget instances are independent") {
  // U7 cleanup: the widget must be an instance type. Two instances must not
  // share mutable state (no static members allowed). R5: Update carries the
  // per-instance animation state (glow lerp).
  NoMoreDay::systems::ui::SwordIntentWidget a;
  NoMoreDay::systems::ui::SwordIntentWidget b;
  a.Update(1, 4, NoMoreDay::BladeResourceKind::SwordFlow, "a", "", 1.0f,
           1.0f / 60.0f);
  b.Update(2, 4, NoMoreDay::BladeResourceKind::SwordFlow, "b", "", 2.0f,
           1.0f / 60.0f);
  CHECK(a.CurrentStacks() == 1);
  CHECK(b.CurrentStacks() == 2);
}

TEST_CASE("[Unit] SwordIntentWidget emits a glow halo at max stacks (R10)") {
  // R10 (收尾): the raylib ui_shine shader glow was dropped in R5; the paint
  // path now approximates it with a low-alpha gold halo behind every active
  // icon, gated by the same m_glowIntensity ramp as legacy. At max stacks the
  // glow ramp saturates to 1.0, so every active icon must produce a halo
  // Image command in addition to its normal icon Image command.
  NoMoreDay::systems::ui::SwordIntentWidget widget;
  widget.SetIconResourceId(NoMoreDay::ui::kSwordIntentIconResourceId);
  // dt = 1.0s drives the dt*3 lerp to saturation in one update.
  widget.Update(4, 4, NoMoreDay::BladeResourceKind::SwordIntent, "Sword Intent",
                "", 0.5f, 1.0f);

  NoMoreDay::ui::UiDrawList drawList;
  const NoMoreDay::ui::UiViewport viewport =
      NoMoreDay::ui::UiViewport::Fit({2560, 1440});
  widget.Paint(drawList, viewport);

  // 4 halos + 4 icons + 1 label text (SwordIntent kind emits no threshold).
  REQUIRE(drawList.CommandCount() == 9);
  int haloCount = 0;
  int iconCount = 0;
  for (const auto& cmd : drawList.Commands()) {
    if (cmd.kind == NoMoreDay::ui::UiDrawKind::Image) {
      // Halo = low-alpha enlarged image (alpha <= ~77); icons stay full
      // alpha. Sizes alone would be ambiguous for the pulsing last icon.
      if (cmd.color.a < 200u) {
        ++haloCount;
      } else {
        ++iconCount;
      }
    }
  }
  CHECK(haloCount == 4);
  CHECK(iconCount == 4);
}

TEST_CASE("[Unit] SwordIntentWidget header holds no static mutable state") {
  const std::string path = "src/game/application/ui/SwordIntentWidget.hpp";
  std::ifstream in(path);
  REQUIRE_MESSAGE(in.good(), "cannot open ", path);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

  const std::string needle = "static ";
  const std::string classNeedle = "class SwordIntentWidget";
  std::string::size_type classPos = contents.find(classNeedle);
  REQUIRE_MESSAGE(classPos != std::string::npos, "class declaration not found");
  const std::string body = contents.substr(classPos);

  std::string::size_type pos = body.find(needle);
  while (pos != std::string::npos) {
    const char after = body[pos + needle.size()];
    // Pure static member functions are fine; static data members are not.
    CHECK_MESSAGE(
        std::isalpha(static_cast<unsigned char>(after)) != 0,
        "static data member must not exist in SwordIntentWidget: '",
        body.substr(pos, body.find_first_of(";{}", pos) - pos + 1), "'");
    pos = body.find(needle, pos + needle.size());
  }
}
