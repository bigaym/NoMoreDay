#include "doctest.h"

#include "game/application/ui/SwordIntentWidget.hpp"
#include "game/foundation/data/BladeMasteryData.hpp"

#include <fstream>
#include <string>

using NoMoreDay::systems::ui::SwordIntentWidget;

TEST_CASE("[Unit] SwordIntentWidget is headless-safe") {
  // raylib window is not open in unit tests; Draw must early-out without
  // touching GL and without hanging on resource loading.
  NoMoreDay::systems::ui::SwordIntentWidget widget;
  widget.Draw(3, 8, NoMoreDay::BladeResourceKind::SwordIntent, "Sword Intent");
  widget.Draw(0, 0, NoMoreDay::BladeResourceKind::None, "", "");
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
  // share mutable state (no static members allowed).
  NoMoreDay::systems::ui::SwordIntentWidget a;
  NoMoreDay::systems::ui::SwordIntentWidget b;
  a.Draw(1, 4, NoMoreDay::BladeResourceKind::SwordFlow, "a");
  b.Draw(2, 4, NoMoreDay::BladeResourceKind::SwordFlow, "b");
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
