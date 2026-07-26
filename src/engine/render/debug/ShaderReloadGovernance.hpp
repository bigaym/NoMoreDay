#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace NoMoreDay::render::debug {

struct ShaderReloadRecord {
  std::string filePath;
  uint64_t lastSuccessfulHash = 0;
  uint64_t currentHash = 0;
  bool isLastReloadSuccess = true;
  std::vector<std::string> includeChain;
  std::string passName;
  std::string resourceName;
  std::string driverErrorMessage;
  uint64_t lastAttemptFrame = 0;
  size_t retryCount = 0;
};

class ShaderReloadGovernance {
public:
  static ShaderReloadGovernance &Get();

  uint64_t ComputeIncludeHash(const std::string &filePath, std::vector<std::string> &outIncludeChain);
  void Reset();
  void RecordReloadAttempt(const std::string &filePath, bool success, uint64_t hash,
                          const std::vector<std::string> &includeChain,
                          std::string_view passName, std::string_view resourceName,
                          std::string_view driverError);

  ShaderReloadRecord GetRecord(const std::string &filePath) const;
  std::vector<ShaderReloadRecord> GetFailedReloadRecords() const;
  std::string GenerateDiagnosticReport(const std::string &filePath) const;

private:
  ShaderReloadGovernance() = default;
  ~ShaderReloadGovernance() = default;

  mutable std::recursive_mutex m_mutex;
  std::map<std::string, ShaderReloadRecord> m_records;
};

} // namespace NoMoreDay::render::debug
