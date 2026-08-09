#pragma once
#include "game/foundation/data/SaveData.hpp"
#include <entt/entt.hpp>
#include <future>
#include <string>
#include <taskflow/taskflow.hpp>

namespace NoMoreDay {

/**
 * @brief Manages the persistence of game saves (Serialization, Asynchronous
 * I/O).
 */
class SaveManager {
public:
  static SaveManager &Get() {
    static SaveManager instance;
    return instance;
  }

  void Initialize(tf::Executor *executor) { m_executor = executor; }
  bool IsInitialized() const { return m_executor != nullptr; }

  /**
   * @brief Snapshots the current game state and writes it to a file
   * asynchronously.
   */
  std::future<bool> saveCharacterAsync(entt::registry &registry, int slotIndex);

  /**
   * @brief Loads a character save from a file and restores it into the
   * registry.
   */
  bool loadCharacter(entt::registry &registry, int slotIndex);

  /**
   * @brief Creates a deep-copy DTO of the current character state.
   */
  CharacterSaveData createSnapshot(entt::registry &registry);

  /**
   * @brief Restores the ECS world from a SaveData DTO.
   */
  void restoreFromSnapshot(entt::registry &registry,
                           const CharacterSaveData &data);

  // Global Save
  bool loadGlobal(entt::registry& registry);
  std::future<bool> saveGlobalAsync(entt::registry& registry);

private:
  SaveManager() = default;
  tf::Executor *m_executor = nullptr;

  std::string getSavePath(int slotIndex) const;
  std::string getTempPath(int slotIndex) const;
};

} // namespace NoMoreDay
