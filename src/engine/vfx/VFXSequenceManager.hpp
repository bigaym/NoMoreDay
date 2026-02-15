#pragma once

#include "engine/vfx/VFXPlayerComponent.hpp"
#include "engine/vfx/VFXTypes.hpp"

#include <entt/entt.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::vfx {

class VFXSequenceManager {
public:
  static VFXSequenceManager &Get();

  void Initialize();
  void Shutdown();

  int LoadFromJson(const std::string &path);
  void TryHotReload();

  [[nodiscard]] const VFXSequenceAsset *GetSequence(int id) const;
  [[nodiscard]] const VFXSequenceAsset *GetSequence(const std::string &name) const;
  [[nodiscard]] int GetSequenceId(const std::string &name) const;
  [[nodiscard]] int GetSequenceCount() const {
    return static_cast<int>(m_sequences.size());
  }

  void Play(entt::registry &registry, entt::entity entity,
            const std::string &sequenceName, entt::entity target = entt::null,
            bool loop = false, float targetWorldX = 0.0f,
            float targetWorldY = 0.0f, bool hasTargetWorld = false);
  void Stop(entt::registry &registry, entt::entity entity);

  static constexpr int MAX_SEQUENCES = 256;
  static constexpr int VFX_SCHEMA_VERSION = 1;

private:
  VFXSequenceManager() = default;

  std::vector<VFXSequenceAsset> m_sequences;
  std::unordered_map<std::string, int> m_nameToId;
  std::string m_assetDir;
  std::unordered_map<std::string, std::filesystem::file_time_type> m_fileTimestamps;
  bool m_initialized = false;
};

} // namespace NoMoreDay::vfx
