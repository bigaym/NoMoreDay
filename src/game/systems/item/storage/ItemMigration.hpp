#pragma once

#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemStorageTypes.hpp"
#include <cstddef>
#include <cstdint>

namespace NoMoreDay {

/**
 * @brief Token representing an active scene transition item migration.
 * Encapsulates pre-migration counts and version to guarantee zero item loss,
 * zero handle leaks, and generational validity across level transitions.
 */
struct SceneMigrationToken {
  uint64_t migrationId = 0;
  size_t preservedItemCount = 0;
  uint64_t sourceVersion = 0;

  constexpr bool operator==(const SceneMigrationToken &other) const noexcept = default;
  constexpr bool operator!=(const SceneMigrationToken &other) const noexcept = default;

  [[nodiscard]] constexpr bool isValid() const noexcept {
    return migrationId != 0;
  }
};

/**
 * @brief Manages item and storage state migration across scene transitions.
 * Eradicates temporary suspend/resume JSON serialization and explicitly recycles GroundPending items.
 */
class ItemMigration {
public:
  ItemMigration() = delete;

  /**
   * @brief Begins a scene switch migration.
   * - Traverses all persistent containers (Inventory, Equipment, BagSlots,
   *   PersonalStash, SharedStash, HeirloomVault) and any socketed items within them.
   * - Batch-destroys and recycles all temporary unpicked ground item handles in GroundPending.
   * - Returns a migration token capturing migrationId, preserved item count, and source version.
   *
   * @param service Central item storage service.
   * @return SceneMigrationToken for validation upon transition completion.
   */
  static SceneMigrationToken beginSceneSwitch(ItemStorageService &service) noexcept;

  /**
   * @brief Concludes and verifies a scene switch migration.
   * - Verifies that the migration token is valid.
   * - Validates that all persistent container item handles and their sockets remain active and generationally valid.
   * - Asserts that the total active items in ItemStore exactly equals the preserved item count (no leaked or destroyed handles).
   * - Confirms GroundPending is empty.
   *
   * @param service Central item storage service.
   * @param token Migration token produced by beginSceneSwitch.
   * @return true if migration succeeded with 100% integrity, false otherwise.
   */
  static bool endSceneSwitch(ItemStorageService &service,
                             const SceneMigrationToken &token) noexcept;

  /**
   * @brief Helper to count active items across all persistent containers and their sockets.
   */
  static size_t countPreservedItems(const ItemStorageService &service) noexcept;
};

} // namespace NoMoreDay
