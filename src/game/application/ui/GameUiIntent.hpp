#pragma once

// UI -> gameplay request contract (design §6.2).
//
// An intent carries the source UI node and a domain payload, never gameplay
// object references. GameUiCommandHandler (U6b) validates the domain id again
// (entity validity, distance, capacity) and only then mutates the world
// during the Update phase. This header is pure data: standard library and
// UiRuntimeTypes value types only.

#include <cstdint>
#include <string>

#include "game/application/ui/UiRuntimeTypes.hpp"

namespace NoMoreDay::ui {

enum class GameUiIntentKind {
  PickupItem,
  // Reserved for later panel migrations; do not add payload fields until a
  // consuming panel actually needs them.
  EquipItem,
  UseItem,
};

struct GameUiIntent {
  UiId sourceNode = kInvalidUiId;       // UI node that issued the intent.
  GameUiIntentKind kind = GameUiIntentKind::PickupItem;
  std::uint64_t domainId = 0; // Stable domain identifier (numeric item entity id).
};

// Outcome of executing an intent. Notifications carry a user-facing message
// on failure (or other notable results); success leaves it empty.
struct GameUiResult {
  bool success = false;
  std::string notification;
};

} // namespace NoMoreDay::ui
