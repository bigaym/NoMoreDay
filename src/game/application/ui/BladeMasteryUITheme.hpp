#pragma once

#include <cstdint>

#include "game/foundation/data/BladeMasteryData.hpp"
#include "raylib.h"

namespace NoMoreDay {

enum class BladeMasteryBackgroundPattern : uint8_t {
  None = 0,
  DiagonalCuts,
  OrbitArcs,
  BrokenPlate,
};

enum class BladeMasteryNodeShell : uint8_t {
  None = 0,
  BeveledDiamond,
  OrbitRing,
  BrokenPlate,
};

enum class BladeMasteryLinkStyle : uint8_t {
  None = 0,
  SlashTrail,
  ArcFlow,
  InwardPulse,
};

struct BladeMasteryUIThemeProfile {
  Color primary = WHITE;
  Color secondary = LIGHTGRAY;
  Color highlight = WHITE;
  Color danger = RED;
  BladeMasteryBackgroundPattern background_pattern =
      BladeMasteryBackgroundPattern::None;
  BladeMasteryNodeShell node_shell = BladeMasteryNodeShell::None;
  BladeMasteryLinkStyle link_style = BladeMasteryLinkStyle::None;
  float idle_pulse_seconds = 2.4f;
};

[[nodiscard]] const BladeMasteryUIThemeProfile &
GetBladeMasteryUIThemeProfile(BladeMasteryId masteryId);

} // namespace NoMoreDay
