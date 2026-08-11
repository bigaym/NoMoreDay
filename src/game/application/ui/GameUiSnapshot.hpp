#pragma once

// Frame-scoped, read-only panel view model for the Game UI (design §3.3).
//
// Hard constraints (design §3.2):
//  - This header is pure data: no entt::registry / entt::entity, no raylib.
//  - Snapshot values are plain POD / std::vector; the UI never receives a
//    registry reference and never copies the whole registry.
//  - Gameplay changes are only performed by GameUiCommandHandler during the
//    Update phase (U6b); this snapshot is built by GameUiSnapshotBuilder.

#include <cstdint>
#include <string>
#include <vector>

namespace NoMoreDay::ui {

// Where a pickup candidate comes from. World items are discovered by the
// snapshot builder; UiNode targets are introduced by later panel migrations.
enum class GameUiPickupSource : std::uint8_t {
  World,  // Ground item entity found in the world.
  UiNode, // Item handle surfaced through a UI tree node (reserved).
};

// A single pickup candidate shown to the UI.
// domainId is the stable domain identifier (the numeric form of the item
// entity id); it must be re-validated (entity validity, distance, capacity)
// by the command handler before any gameplay change (design §6.2).
struct GameUiPickupSnapshot {
  std::uint64_t domainId = 0;
  float distance = 0.0f; // World-space distance from the player.
  GameUiPickupSource source = GameUiPickupSource::World;
};

// Read-only player panel fields, extracted from the player entity components
// (HealthComponent / PlayerStats / InventoryComponent).
struct GameUiPlayerSnapshot {
  bool hasPlayer = false;
  float health = 0.0f;
  float maxHealth = 0.0f;
  int level = 1;
  int inventoryUsed = 0;     // Occupied inventory slots.
  int inventoryCapacity = 0; // Total inventory capacity.
  int gold = 0;
};

// A message for the UI to surface (pickup failures, errors, ...). Produced by
// the command handler in later slices; the builder leaves the queue empty.
struct GameUiNotification {
  std::string message;
};

// Aggregated frame-scoped read model handed to the UI each Update.
struct GameUiSnapshot {
  GameUiPlayerSnapshot player;
  std::vector<GameUiPickupSnapshot> pickups;
  std::vector<GameUiNotification> notifications;
};

} // namespace NoMoreDay::ui
