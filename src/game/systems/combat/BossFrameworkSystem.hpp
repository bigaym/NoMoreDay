#pragma once

#include "game/components/EnemyComponent.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay::systems {

class BossFrameworkSystem {
public:
  static void Update(entt::registry &registry, float dt);
  static void AttachPrototype(entt::registry &registry, entt::entity boss);

  [[nodiscard]] static bool
  ApplyAilment(entt::registry &registry, entt::entity boss,
               const AilmentApplyRequest &request);

  static void OpenCounterWindow(entt::registry &registry, entt::entity boss,
                                float duration,
                                BossCounterAction expectedAction);
  [[nodiscard]] static bool TryResolveCounter(entt::registry &registry,
                                              entt::entity boss,
                                              BossCounterAction action);

  [[nodiscard]] static uint64_t GetFrameIndexForTests() noexcept;
  static void ResetForTests() noexcept;
};

} // namespace NoMoreDay::systems
