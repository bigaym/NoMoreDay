#include "game/systems/ui/BladeMasteryUITheme.hpp"

#include <stdexcept>

namespace NoMoreDay {
namespace {

const BladeMasteryUIThemeProfile kFallbackTheme = {
    .primary = Color{201, 201, 201, 255},
    .secondary = Color{108, 113, 122, 255},
    .highlight = Color{241, 227, 168, 255},
    .danger = Color{178, 76, 76, 255},
    .background_pattern = BladeMasteryBackgroundPattern::None,
    .node_shell = BladeMasteryNodeShell::None,
    .link_style = BladeMasteryLinkStyle::None,
    .idle_pulse_seconds = 2.4f,
};

const BladeMasteryUIThemeProfile kSwordSaintTheme = {
    .primary = Color{232, 224, 206, 255},
    .secondary = Color{118, 126, 138, 255},
    .highlight = Color{250, 244, 228, 255},
    .danger = Color{190, 78, 78, 255},
    .background_pattern = BladeMasteryBackgroundPattern::DiagonalCuts,
    .node_shell = BladeMasteryNodeShell::BeveledDiamond,
    .link_style = BladeMasteryLinkStyle::SlashTrail,
    .idle_pulse_seconds = 2.4f,
};

const BladeMasteryUIThemeProfile kHeavenlySwordTheme = {
    .primary = Color{112, 186, 247, 255},
    .secondary = Color{83, 108, 171, 255},
    .highlight = Color{232, 244, 255, 255},
    .danger = Color{196, 108, 76, 255},
    .background_pattern = BladeMasteryBackgroundPattern::OrbitArcs,
    .node_shell = BladeMasteryNodeShell::OrbitRing,
    .link_style = BladeMasteryLinkStyle::ArcFlow,
    .idle_pulse_seconds = 3.1f,
};

const BladeMasteryUIThemeProfile kDemonBladeTheme = {
    .primary = Color{148, 96, 104, 255},
    .secondary = Color{82, 58, 64, 255},
    .highlight = Color{210, 160, 154, 255},
    .danger = Color{226, 84, 92, 255},
    .background_pattern = BladeMasteryBackgroundPattern::BrokenPlate,
    .node_shell = BladeMasteryNodeShell::BrokenPlate,
    .link_style = BladeMasteryLinkStyle::InwardPulse,
    .idle_pulse_seconds = 2.1f,
};

} // namespace

const BladeMasteryUIThemeProfile &
GetBladeMasteryUIThemeProfile(BladeMasteryId masteryId) {
  switch (masteryId) {
  case BladeMasteryId::SwordSaint:
    return kSwordSaintTheme;
  case BladeMasteryId::HeavenlySword:
    return kHeavenlySwordTheme;
  case BladeMasteryId::DemonBlade:
    return kDemonBladeTheme;
  case BladeMasteryId::None:
    return kFallbackTheme;
  }

  throw std::invalid_argument("Blade mastery UI theme is not defined");
}

} // namespace NoMoreDay
