// src/game/systems/item/HeirloomVault.hpp
// 传家宝宝库管理系统 - 处理跨存档的传家宝存储与加载
#pragma once

#include "core/logging/Logger.hpp"
#include "game/foundation/components/HeirloomComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/systems/item/HeirloomScaling.hpp"
#include <concepts>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>


namespace NoMoreDay {

/// @brief 传家宝数据结构 (用于持久化存储)
/// @details 包含 ItemComponent 的核心数据和 HeirloomComponent 的元数据
struct HeirloomData {
  ItemComponent item;
  HeirloomComponent heirloom;

  // 用于 UI 显示的预计算字段
  float effective_power_at_level_1{0.0f};
  float effective_power_at_level_50{0.0f};
};

// JSON 序列化支持
inline void to_json(nlohmann::json &j, const HeirloomData &h) {
  j = nlohmann::json{
      {"item", h.item},
      {"heirloom",
       {{"tier", h.heirloom.tier},
        {"original_level_requirement", h.heirloom.original_level_requirement},
        {"created_timestamp", h.heirloom.created_timestamp},
        {"display_name", h.heirloom.display_name},
        {"original_rarity", h.heirloom.original_rarity}}}};
}

inline void from_json(const nlohmann::json &j, HeirloomData &h) {
  j.at("item").get_to(h.item);

  const auto &hj = j.at("heirloom");
  h.heirloom.tier = hj.value("tier", uint8_t{1});
  h.heirloom.original_level_requirement =
      hj.value("original_level_requirement", uint8_t{1});
  h.heirloom.created_timestamp = hj.value("created_timestamp", int64_t{0});
  h.heirloom.display_name = hj.value("display_name", std::string{});
  h.heirloom.original_rarity = hj.value("original_rarity", uint8_t{0});
  h.heirloom.is_active_this_run = false; // 每次加载重置

  // 预计算有效战力
  h.effective_power_at_level_1 =
      HeirloomScaling::calculateEffectivePowerPercent(
          1, h.heirloom.original_level_requirement);
  h.effective_power_at_level_50 =
      HeirloomScaling::calculateEffectivePowerPercent(
          50, h.heirloom.original_level_requirement);
}

/// @brief 传家宝宝库管理器 (Singleton)
/// @details 管理跨存档的传家宝持久化存储
class HeirloomVault {
public:
  /// 获取单例实例
  [[nodiscard]] static HeirloomVault &Get() {
    static HeirloomVault instance;
    return instance;
  }

  /// 最大传家宝数量
  static constexpr size_t kMaxHeirlooms = 20;

  /// 默认存储路径
  static constexpr std::string_view kDefaultVaultPath =
      "saves/heirloom_vault.json";

  /// 加载宝库数据
  /// @param path 存储路径 (默认使用 kDefaultVaultPath)
  /// @return 是否加载成功
  bool load(std::string_view path = kDefaultVaultPath) {
    const std::filesystem::path filepath{path};

    if (!std::filesystem::exists(filepath)) {
      LOG_INFO("[HeirloomVault] No vault file found at '{}', starting fresh.",
               path);
      m_heirlooms.clear();
      return true;
    }

    try {
      std::ifstream file(filepath);
      if (!file.is_open()) {
        LOG_ERROR("[HeirloomVault] Failed to open vault file: {}", path);
        return false;
      }

      nlohmann::json j;
      file >> j;

      m_heirlooms.clear();
      if (j.contains("heirlooms") && j["heirlooms"].is_array()) {
        for (const auto &hj : j["heirlooms"]) {
          HeirloomData data = hj.get<HeirloomData>();
          m_heirlooms.push_back(std::move(data));
        }
      }

      LOG_INFO("[HeirloomVault] Loaded {} heirlooms from '{}'",
               m_heirlooms.size(), path);
      return true;

    } catch (const std::exception &e) {
      LOG_ERROR("[HeirloomVault] Failed to parse vault file: {}", e.what());
      return false;
    }
  }

  /// 保存宝库数据
  /// @param path 存储路径
  /// @return 是否保存成功
  bool save(std::string_view path = kDefaultVaultPath) const {
    const std::filesystem::path filepath{path};

    // 确保目录存在
    if (filepath.has_parent_path()) {
      std::filesystem::create_directories(filepath.parent_path());
    }

    try {
      nlohmann::json j;
      j["version"] = 1;
      j["heirlooms"] = m_heirlooms;

      std::ofstream file(filepath);
      if (!file.is_open()) {
        LOG_ERROR("[HeirloomVault] Failed to create vault file: {}", path);
        return false;
      }

      file << j.dump(2); // Pretty print with 2-space indent
      LOG_INFO("[HeirloomVault] Saved {} heirlooms to '{}'", m_heirlooms.size(),
               path);
      return true;

    } catch (const std::exception &e) {
      LOG_ERROR("[HeirloomVault] Failed to save vault file: {}", e.what());
      return false;
    }
  }

  /// 添加传家宝
  /// @param item 物品数据
  /// @param level_requirement 等级要求
  /// @param rarity 稀有度
  /// @return 是否添加成功 (可能因数量限制失败)
  bool addHeirloom(const ItemComponent &item, uint8_t level_requirement,
                   Rarity rarity) {
    if (m_heirlooms.size() >= kMaxHeirlooms) {
      LOG_WARN("[HeirloomVault] Vault is full ({} items), cannot add more.",
               kMaxHeirlooms);
      return false;
    }

    HeirloomData data;
    data.item = item;
    data.heirloom.tier = calculateTier(rarity);
    data.heirloom.original_level_requirement = level_requirement;
    data.heirloom.created_timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    data.heirloom.display_name = item.name;
    data.heirloom.original_rarity = static_cast<uint8_t>(rarity);

    // 预计算有效战力
    data.effective_power_at_level_1 =
        HeirloomScaling::calculateEffectivePowerPercent(1, level_requirement);
    data.effective_power_at_level_50 =
        HeirloomScaling::calculateEffectivePowerPercent(50, level_requirement);

    m_heirlooms.push_back(std::move(data));
    LOG_INFO("[HeirloomVault] Added heirloom: {} (Tier {})", item.name,
             data.heirloom.tier);

    return true;
  }

  /// 移除传家宝
  /// @param index 宝库中的索引
  /// @return 是否移除成功
  bool removeHeirloom(size_t index) {
    if (index >= m_heirlooms.size()) {
      return false;
    }

    LOG_INFO("[HeirloomVault] Removed heirloom: {}",
             m_heirlooms[index].item.name);
    m_heirlooms.erase(m_heirlooms.begin() + static_cast<ptrdiff_t>(index));
    return true;
  }

  /// 获取所有传家宝 (只读)
  [[nodiscard]] std::span<const HeirloomData> getHeirlooms() const noexcept {
    return m_heirlooms;
  }

  /// 获取传家宝数量
  [[nodiscard]] size_t size() const noexcept { return m_heirlooms.size(); }

  /// 检查宝库是否已满
  [[nodiscard]] bool isFull() const noexcept {
    return m_heirlooms.size() >= kMaxHeirlooms;
  }

  /// 获取指定传家宝
  [[nodiscard]] const HeirloomData *getHeirloom(size_t index) const noexcept {
    if (index >= m_heirlooms.size()) {
      return nullptr;
    }
    return &m_heirlooms[index];
  }

private:
  HeirloomVault() = default;

  /// 根据稀有度计算传家宝等级
  [[nodiscard]] static uint8_t calculateTier(Rarity rarity) noexcept {
    switch (rarity) {
    case Rarity::Mythic:
      return 3;
    case Rarity::Legendary:
      return 2;
    default:
      return 1;
    }
  }

  std::vector<HeirloomData> m_heirlooms;
};

} // namespace NoMoreDay
