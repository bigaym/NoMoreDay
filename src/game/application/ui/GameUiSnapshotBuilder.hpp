#pragma once

// Builds the frame-scoped GameUiSnapshot from the ECS (design §3.1).
//
// The Build method is templated on the registry type so this header stays
// free of entt includes (design §3.2: the Game UI core must not depend on
// entt types). The concrete entt::registry instantiation is explicitly
// instantiated in GameUiSnapshotBuilder.cpp.

#include "game/application/ui/GameUiSnapshot.hpp"

namespace NoMoreDay::ui {

class GameUiSnapshotBuilder {
public:
  // Registry is entt::registry in practice; see the explicit instantiation
  // in GameUiSnapshotBuilder.cpp.
  template <typename Registry>
  GameUiSnapshot Build(const Registry& registry) const;
};

} // namespace NoMoreDay::ui
