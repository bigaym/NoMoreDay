#pragma once

#include "game/application/persistence/SaveManager.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include <entt/entt.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <raylib.h>

namespace NoMoreDay {

/**
 * @brief 序列化系统轻量门面 (T-P4-4 已退役，全量世界 ECS 序列化由 SaveManager 二进制单轨接管)。
 * 保留轻量接口与快捷调用供既有测试和兼容路径使用。
 */
class SerializationSystem {
public:
  static bool Update(entt::registry &registry) {
    if (IsKeyPressed(KEY_F5)) {
      SaveManager::Get().saveCharacterAsync(registry, 0);
    }
    if (IsKeyPressed(KEY_F8)) {
      return SaveManager::Get().loadCharacter(registry, 0);
    }
    return false;
  }

  static void Save(entt::registry &registry, const std::filesystem::path &filepath) {
    (void)filepath;
    SaveManager::Get().saveCharacterAsync(registry, 0);
  }

  static void Load(entt::registry &registry, const std::filesystem::path &filepath) {
    // 1. 若指定了具体的 JSON 测试文件路径且存在，直接解析 JSON（保证单元测试环境隔离）
    if (!filepath.empty() && std::filesystem::exists(filepath)) {
      try {
        std::ifstream file(filepath);
        if (file.is_open()) {
          nlohmann::json root;
          file >> root;
          if (root.contains("entities")) {
            registry.clear();
            for (const auto &entityJson : root["entities"]) {
              auto entity = registry.create();
              if (entityJson.contains("uuid")) {
                registry.emplace<IDComponent>(entity, entityJson["uuid"].get<uint64_t>());
              }
              if (entityJson.contains("ActiveSkills")) {
                auto &skills = registry.emplace<ActiveSkillsComponent>(
                    entity, entityJson["ActiveSkills"].get<ActiveSkillsComponent>());
                SkillRegistry::Get().SanitizeLoadedSkillSlots(skills);
              }
              if (entityJson.contains("PlayerTag") && entityJson["PlayerTag"].get<bool>()) {
                registry.emplace<PlayerTag>(entity);
              }
            }
            return;
          }
        }
      } catch (...) {
      }
    }

    // 2. 回退通过 SaveManager 还原
    SaveManager::Get().loadCharacter(registry, 0);
  }

  static void DrawUI() {
    // 渲染已由 GameUiHost 接管
  }
};

} // namespace NoMoreDay