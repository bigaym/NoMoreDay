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

  // 重置内部容器与版本缓存（用于场景切换或测试重置）
  void InvalidateCache() noexcept {
    m_hasContainerCache = false;
    m_lastItemStoreVersion = 0;
    m_lastPlayerDomainId = 0;
    m_lastPlayerGold = 0;
    m_lastInventoryUsed = 0;
    m_lastUnlockedTabs = 0;
    m_lastOptions = {};
  }

private:
  // Monotonically increasing frame revision; bumped on every Build. The UI
  // uses it to detect "this frame produced no gameplay change".
  std::uint64_t m_revision = 0;

  // 容器视图增量缓存 (T-P5-2: 纯容器轨道增量缓存，绝不阻塞每帧动态 HUD/冷却/Buff 更新)
  bool m_hasContainerCache = false;
  std::uint64_t m_lastItemStoreVersion = 0;
  std::uint64_t m_lastPlayerDomainId = 0;
  std::int32_t m_lastPlayerGold = 0;
  std::int32_t m_lastInventoryUsed = 0;
  int m_lastUnlockedTabs = 0;
  GameUiSnapshotOptions m_lastOptions{};

  GameUiInventoryView m_cachedInventory{};
  std::vector<GameUiEquippedSlotView> m_cachedEquipment{};
  GameUiStashView m_cachedStash{};
  std::vector<GameUiMaterialView> m_cachedMaterials{};
};

} // namespace NoMoreDay::ui
