#pragma once

// Builds the frame-scoped GameUiSnapshot from the ECS (design §3.1).
//
// The Build method is templated on the registry type so this header stays
// free of entt includes (design §3.2: the Game UI core must not depend on
// entt types). The concrete entt::registry instantiation is explicitly
// instantiated in GameUiSnapshotBuilder.cpp.
//
// Build is the ONLY read of gameplay state for the UI: it runs after every
// gameplay write of the frame (intents included, see the GameplayState update
// order) and produces a revision-bumped, value-only view model. UI session
// display requests (hover/drag/crafting targets/active stash tab) are fed in
// through GameUiSnapshotOptions; they never enter the snapshot itself.

#include "game/application/ui/GameUiSnapshot.hpp"

#include <cstdint>

namespace NoMoreDay::ui {

class GameUiSnapshotBuilder {
public:
  GameUiSnapshotBuilder() = default;

  // Registry is entt::registry in practice; see the explicit instantiation
  // in GameUiSnapshotBuilder.cpp.
  template <typename Registry>
  GameUiSnapshot Build(const Registry& registry,
                       const GameUiSnapshotOptions& options = {});

private:
  // Monotonically increasing frame revision; bumped on every Build. The UI
  // uses it to detect "this frame produced no gameplay change".
  std::uint64_t m_revision = 0;
};

} // namespace NoMoreDay::ui
