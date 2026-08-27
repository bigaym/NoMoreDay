#pragma once
#include "game/foundation/data/SaveData.hpp"
#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include <entt/entt.hpp>
#include <future>
#include <string>
#include <taskflow/taskflow.hpp>

namespace NoMoreDay {

/**
 * @brief Manages the persistence of game saves (Serialization, Asynchronous I/O).
 * 统一承载二进制持久化 (.nmd) 与旧版 JSON 存档兼容迁移。
 */
class SaveManager {
public:
  static SaveManager &Get() {
    static SaveManager instance;
    return instance;
  }

  void Initialize(tf::Executor *executor, ItemStorageService *itemStorage = nullptr) {
    m_executor = executor;
    if (itemStorage != nullptr) {
      m_itemStorage = itemStorage;
    }
  }
  bool IsInitialized() const { return m_executor != nullptr; }

  void SetItemStorageService(ItemStorageService *service) noexcept {
    m_itemStorage = service;
  }

  ItemStorageService *GetItemStorageService(entt::registry *registry = nullptr) noexcept;

  /**
   * @brief Snapshots the current game state and writes it to a file asynchronously (.nmd binary).
   */
  std::future<bool> saveCharacterAsync(entt::registry &registry, int slotIndex);

  /**
   * @brief Loads a character save from a file (.nmd binary, with .bak or JSON migration support) and restores it into the registry.
   */
  bool loadCharacter(entt::registry &registry, int slotIndex);

  /**
   * @brief Creates a deep-copy DTO of the current character state.
   */
  CharacterSaveData createSnapshot(entt::registry &registry);

  /**
   * @brief Restores the ECS world from a SaveData DTO.
   * @param registry ECS 注册表
   * @param data 角色存档数据
   * @param restoreItems 是否从 SaveData 中恢复物品到 ItemStorageService（从旧 JSON 读取时为 true；从 .nmd 二进制加载时为 false，因 decode 已直接灌入 storage）
   */
  void restoreFromSnapshot(entt::registry &registry,
                           const CharacterSaveData &data,
                           bool restoreItems = true);

  static void MigrateSaveDataV3toV4(CharacterSaveData &data);

  static nlohmann::json BuildProgressionJson(const CharacterSaveData &charData);
  static CharacterSaveData ParseProgressionJson(const nlohmann::json &progJson);

  // Global Save
  bool loadGlobal(entt::registry &registry);
  std::future<bool> saveGlobalAsync(entt::registry &registry);

private:
  SaveManager() = default;
  tf::Executor *m_executor = nullptr;
  ItemStorageService *m_itemStorage = nullptr;
  std::atomic<bool> m_isSaving{false};

  std::string getSavePath(int slotIndex) const;
  std::string getTempPath(int slotIndex) const;
  std::string getBackupPath(int slotIndex) const;
  std::string getJsonSavePath(int slotIndex) const;
};

} // namespace NoMoreDay
