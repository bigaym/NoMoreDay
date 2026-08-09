#include "TestCommon.hpp"

#include "game/application/ui/BladeMasteryUITheme.hpp"

using namespace NoMoreDay;

TEST_CASE("[Unit] Blade Mastery UI Theme - exposes shared chrome profile fields") {
  const BladeMasteryUIThemeProfile &theme =
      GetBladeMasteryUIThemeProfile(BladeMasteryId::SwordSaint);

  CHECK(theme.primary.a == 255);
  CHECK(theme.secondary.a == 255);
  CHECK(theme.highlight.a == 255);
  CHECK(theme.danger.a == 255);
  CHECK(theme.background_pattern ==
        BladeMasteryBackgroundPattern::DiagonalCuts);
  CHECK(theme.node_shell == BladeMasteryNodeShell::BeveledDiamond);
  CHECK(theme.link_style == BladeMasteryLinkStyle::SlashTrail);
  CHECK(theme.idle_pulse_seconds == doctest::Approx(2.4f));
}

TEST_CASE("[Unit] Blade Mastery UI Theme - preserves intended mastery directions") {
  const BladeMasteryUIThemeProfile &swordSaint =
      GetBladeMasteryUIThemeProfile(BladeMasteryId::SwordSaint);
  const BladeMasteryUIThemeProfile &heavenlySword =
      GetBladeMasteryUIThemeProfile(BladeMasteryId::HeavenlySword);
  const BladeMasteryUIThemeProfile &demonBlade =
      GetBladeMasteryUIThemeProfile(BladeMasteryId::DemonBlade);

  CHECK(swordSaint.background_pattern ==
        BladeMasteryBackgroundPattern::DiagonalCuts);
  CHECK(swordSaint.node_shell == BladeMasteryNodeShell::BeveledDiamond);
  CHECK(swordSaint.link_style == BladeMasteryLinkStyle::SlashTrail);

  CHECK(heavenlySword.background_pattern ==
        BladeMasteryBackgroundPattern::OrbitArcs);
  CHECK(heavenlySword.node_shell == BladeMasteryNodeShell::OrbitRing);
  CHECK(heavenlySword.link_style == BladeMasteryLinkStyle::ArcFlow);

  CHECK(demonBlade.background_pattern ==
        BladeMasteryBackgroundPattern::BrokenPlate);
  CHECK(demonBlade.node_shell == BladeMasteryNodeShell::BrokenPlate);
  CHECK(demonBlade.link_style == BladeMasteryLinkStyle::InwardPulse);

  CHECK(swordSaint.primary.r > swordSaint.primary.b);
  CHECK(heavenlySword.primary.b > heavenlySword.primary.r);
  CHECK(demonBlade.danger.r > demonBlade.secondary.r);
  CHECK(demonBlade.idle_pulse_seconds < heavenlySword.idle_pulse_seconds);
}

TEST_CASE("[Unit] Blade Mastery UI Theme - only None uses neutral fallback") {
  const BladeMasteryUIThemeProfile &noneTheme =
      GetBladeMasteryUIThemeProfile(BladeMasteryId::None);

  CHECK(noneTheme.background_pattern == BladeMasteryBackgroundPattern::None);
  CHECK(noneTheme.node_shell == BladeMasteryNodeShell::None);
  CHECK(noneTheme.link_style == BladeMasteryLinkStyle::None);

  const auto lookupUnknownTheme = []() {
    return GetBladeMasteryUIThemeProfile(static_cast<BladeMasteryId>(255));
  };
  CHECK_THROWS_AS(lookupUnknownTheme(), std::invalid_argument);
}
