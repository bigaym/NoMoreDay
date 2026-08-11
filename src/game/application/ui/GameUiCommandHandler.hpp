#pragma once

// Executes validated gameplay requests on behalf of the UI (design §3.1,
// §6.2). The UI never mutates the world directly: it issues GameUiIntent and
// the handler re-validates the domain payload (entity validity, distance,
// capacity) against the live registry before touching any gameplay system.
//
// Execute is templated on the registry type so this header stays free of
// entt includes (design §3.2); the concrete entt::registry instantiation is
// explicitly instantiated in GameUiCommandHandler.cpp.

#include "game/application/ui/GameUiIntent.hpp"

namespace NoMoreDay::ui {

class GameUiCommandHandler {
public:
  // Registry is entt::registry in practice; see the explicit instantiation
  // in GameUiCommandHandler.cpp.
  template <typename Registry>
  GameUiResult Execute(Registry& registry, const GameUiIntent& intent) const;
};

} // namespace NoMoreDay::ui
