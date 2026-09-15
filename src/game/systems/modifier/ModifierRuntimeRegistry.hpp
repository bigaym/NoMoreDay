#pragma once

#include "game/systems/modifier/ModifierRuntimeTypes.hpp"

#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {

class ModifierRuntimeRegistry {
public:
  [[nodiscard]] static ModifierRuntimeRegistry &Get();

  // 已加载且路径一致时直接返回 true；路径不同则重新加载，避免返回与请求
  // 路径不符的旧数据。经 LoadFromBytes 注入的合成数据视为通配来源。
  [[nodiscard]] bool EnsureLoaded(
      std::string_view path = "assets/generated/modifier_runtime_v2.bin");
  // 无条件重新读取：用于测试在同一进程内覆盖单例状态。
  // 打开或解析失败会打印 LOG_ERROR（含路径与失败类别），不再静默返回。
  [[nodiscard]] bool Reload(std::string_view path);
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
  // 热路径（EnsureLoaded）与 Reload 可能跨线程读写，故用原子布尔避免数据竞争。
  std::atomic<bool> m_loaded{false};
  // 当前数据来源路径；Reload 成功后写入。经 LoadFromBytes 注入的合成数据
  // 不携带路径，此处保持为空，EnsureLoaded 将其视为通配来源。
  std::string m_loadedPath;
};

} // namespace NoMoreDay
