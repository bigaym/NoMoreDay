#pragma once

#include "core/logging/Logger.hpp"
#include "game/foundation/components/FactionComponent.hpp"
#include "game/foundation/components/NemesisComponent.hpp"
#include "game/foundation/data/MonsterAffixRegistry.hpp"
#include <array>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>


namespace NoMoreDay {

/**
 * @brief Singleton data store for Nemesis system persistence.
 */
class NemesisDataStore {
public:
  static NemesisDataStore &Get() {
    static NemesisDataStore instance;
    return instance;
  }

  // Non-copyable
  NemesisDataStore(const NemesisDataStore &) = delete;
  NemesisDataStore &operator=(const NemesisDataStore &) = delete;

  /**
   * @brief Save Nemesis data to a JSON file.
   */
  void Save(const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json j;

    // Save faction aggro
    j["faction_aggro"] = nlohmann::json::array();
    for (size_t i = 0; i < faction_aggro.size(); ++i) {
      j["faction_aggro"].push_back(faction_aggro[i]);
    }

    // Save kill affix history (Convert to string for persistence)
    j["kill_affix_history"] = nlohmann::json::array();
    for (auto type : kill_affix_history) {
      j["kill_affix_history"].push_back(std::string(MonsterAffixRegistry::GetAffixNameEn(type)));
    }

    // Save active Nemesis if present
    if (active_nemesis.has_value()) {
      const auto &nem = active_nemesis.value();
      
      nlohmann::json affList = nlohmann::json::array();
      for (auto type : nem.affixes) {
        affList.push_back(std::string(MonsterAffixRegistry::GetAffixNameEn(type)));
      }

      j["active_nemesis"] = {
          {"nemesis_id", nem.nemesis_id},
          {"faction", static_cast<int>(nem.faction)},
          {"affixes", affList},
          {"resistances", static_cast<uint64_t>(nem.resistances)},
          {"evolution_tier", nem.evolution_tier},
          {"display_name", nem.display_name},
          {"is_active", nem.is_active}};
    }

    // Save next Nemesis ID
    j["next_nemesis_id"] = next_nemesis_id;

    // Write to file
    try {
      std::filesystem::create_directories(path.parent_path());
      std::ofstream file(path);
      if (file.is_open()) {
        file << j.dump(2);
        LOG_INFO("NemesisDataStore: Saved to {}", path.string());
      }
    } catch (const std::exception &e) {
      LOG_ERROR("NemesisDataStore: Failed to save: {}", e.what());
    }
  }

  /**
   * @brief Load Nemesis data from a JSON file.
   */
  void Load(const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!std::filesystem::exists(path)) {
      LOG_INFO("NemesisDataStore: No save file found at {}", path.string());
      return;
    }

    try {
      std::ifstream file(path);
      nlohmann::json j;
      file >> j;

      // Load faction aggro
      if (j.contains("faction_aggro") && j["faction_aggro"].is_array()) {
        for (size_t i = 0;
             i < std::min(j["faction_aggro"].size(), faction_aggro.size());
             ++i) {
          faction_aggro[i] = j["faction_aggro"][i].get<float>();
        }
      }

      // Load kill affix history
      kill_affix_history.clear();
      if (j.contains("kill_affix_history") &&
          j["kill_affix_history"].is_array()) {
        for (const auto &affix_node : j["kill_affix_history"]) {
          std::string name = affix_node.get<std::string>();
          kill_affix_history.push_back(MonsterAffixRegistry::GetTypeFromName(name));
        }
      }

      // Load active Nemesis
      active_nemesis.reset();
      if (j.contains("active_nemesis") && !j["active_nemesis"].is_null()) {
        const auto& nem_j = j["active_nemesis"];
        NemesisData nem;
        nem.nemesis_id = nem_j["nemesis_id"].get<uint64_t>();
        nem.faction =
            static_cast<FactionType>(nem_j["faction"].get<int>());
        
        if (nem_j.contains("affixes") && nem_j["affixes"].is_array()) {
          for (const auto& aff_node : nem_j["affixes"]) {
            nem.affixes.push_back(MonsterAffixRegistry::GetTypeFromName(aff_node.get<std::string>()));
          }
        }
        
        nem.resistances = static_cast<Tag>(
            nem_j["resistances"].get<uint64_t>());
        nem.evolution_tier = nem_j["evolution_tier"].get<int>();
        nem.display_name =
            nem_j["display_name"].get<std::string>();
        nem.is_active = nem_j["is_active"].get<bool>();
        active_nemesis = nem;
      }

      // Load next Nemesis ID
      if (j.contains("next_nemesis_id")) {
        next_nemesis_id = j["next_nemesis_id"].get<uint64_t>();
      }

      LOG_INFO("NemesisDataStore: Loaded from {}", path.string());

    } catch (const std::exception &e) {
      LOG_ERROR("NemesisDataStore: Failed to load: {}", e.what());
    }
  }

  /**
   * @brief Record an elite affix from a kill (for Nemesis synthesis).
   */
  void RecordKillAffix(MonsterAffixType type) {
    if (type == MonsterAffixType::None) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    kill_affix_history.push_back(type);
    // Keep only last 50
    while (kill_affix_history.size() > MAX_AFFIX_HISTORY) {
      kill_affix_history.pop_front();
    }
  }

  /**
   * @brief Generate a unique Nemesis ID.
   */
  uint64_t GenerateNemesisId() { return next_nemesis_id++; }

  /**
   * @brief Get the most common affixes from kill history.
   * @param count Number of top affixes to return
   */
  std::vector<MonsterAffixType> GetTopAffixes(size_t count) const {
    std::vector<MonsterAffixType> snapshot;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      snapshot.assign(kill_affix_history.begin(), kill_affix_history.end());
    }

    if (snapshot.empty()) return {};

    // 锁外统计与排序
    std::array<int, static_cast<size_t>(MonsterAffixType::Count)> counts{};
    for (auto type : snapshot) {
      counts[static_cast<size_t>(type)]++;
    }

    std::vector<std::pair<MonsterAffixType, int>> sorted;
    sorted.reserve(static_cast<size_t>(MonsterAffixType::Count));
    for (size_t i = 1; i < static_cast<size_t>(MonsterAffixType::Count); ++i) {
      if (counts[i] > 0) {
        sorted.push_back({static_cast<MonsterAffixType>(i), counts[i]});
      }
    }

    std::sort(sorted.begin(), sorted.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    std::vector<MonsterAffixType> result;
    for (size_t i = 0; i < std::min(count, sorted.size()); ++i) {
      result.push_back(sorted[i].first);
    }
    return result;
  }

  /**
   * @brief Reset the data store (for testing).
   */
  void Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    faction_aggro.fill(0.0f);
    kill_affix_history.clear();
    active_nemesis.reset();
    next_nemesis_id = 1;
  }

  // Public data members (accessible for convenience but protected by m_mutex where needed)
  std::array<float, static_cast<size_t>(FactionType::Count)> faction_aggro{};
  std::deque<MonsterAffixType> kill_affix_history;
  std::optional<NemesisData> active_nemesis;

  static constexpr size_t MAX_AFFIX_HISTORY = 50;

  /**
   * @brief Mark the active Nemesis as defeated and evolve it.
   */
  void EvolveActiveNemesis() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (active_nemesis.has_value()) {
      active_nemesis->evolution_tier++;
      active_nemesis->is_active = false;
      LOG_INFO("NemesisDataStore: Nemesis {} evolved to Tier {}", 
               active_nemesis->nemesis_id, active_nemesis->evolution_tier);
    }
  }

private:
   NemesisDataStore() = default;

  uint64_t next_nemesis_id = 1;
  mutable std::mutex m_mutex;
};

} // namespace NoMoreDay