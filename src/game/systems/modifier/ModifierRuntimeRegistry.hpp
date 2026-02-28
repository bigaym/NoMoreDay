#pragma once

#include "game/systems/modifier/ModifierRuntimeTypes.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {

class ModifierRuntimeRegistry {
public:
  [[nodiscard]] static ModifierRuntimeRegistry &Get();

  [[nodiscard]] bool EnsureLoaded(
      std::string_view path = "assets/generated/modifier_runtime_v2.bin");
  [[nodiscard]] bool LoadFromBytes(std::span<const uint8_t> bytes);
  [[nodiscard]] uint32_t RecordCount() const;
  [[nodiscard]] std::span<const ModifierRuntimeRecord> GetRecords() const;
  [[nodiscard]] const ModifierRuntimeRecord *FindRecordById(uint32_t id) const;
  [[nodiscard]] const ModifierRuntimeFilter *GetFilter(const ModifierRuntimeRecord &record) const;
  [[nodiscard]] std::span<const ModifierRuntimeOp> GetOps(const ModifierRuntimeRecord &record) const;
  [[nodiscard]] std::span<const uint32_t>
  GetSkillWhitelist(const ModifierRuntimeFilter &filter) const;
  [[nodiscard]] std::span<const uint32_t>
  GetNodeWhitelist(const ModifierRuntimeFilter &filter) const;

private:
  void Clear();

  ModifierRuntimeHeader m_header{};
  std::vector<ModifierRuntimeRecord> m_records;
  std::vector<ModifierRuntimeFilter> m_filters;
  std::vector<ModifierRuntimeOp> m_ops;
  std::vector<uint32_t> m_index;
  std::unordered_map<uint32_t, uint32_t> m_recordIndexById;
  bool m_loaded = false;
};

} // namespace NoMoreDay
