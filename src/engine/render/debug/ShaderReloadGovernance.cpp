#include "engine/render/debug/ShaderReloadGovernance.hpp"
#include "core/logging/Logger.hpp"

#include <fstream>
#include <regex>
#include <sstream>

namespace NoMoreDay::render::debug {

namespace {
constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void HashString(uint64_t &hash, const std::string &str) {
  for (char c : str) {
    hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
    hash *= kFnvPrime;
  }
}
} // namespace

ShaderReloadGovernance &ShaderReloadGovernance::Get() {
  static ShaderReloadGovernance instance;
  return instance;
}

uint64_t ShaderReloadGovernance::ComputeIncludeHash(const std::string &filePath,
                                                    std::vector<std::string> &outIncludeChain) {
  outIncludeChain.clear();
  outIncludeChain.push_back(filePath);

  uint64_t hash = kFnvOffset;
  std::vector<std::string> pending = {filePath};
  std::regex includeRegex(R"(^\s*#include\s+["<]([^">]+)[">])");

  while (!pending.empty()) {
    std::string current = pending.back();
    pending.pop_back();

    std::ifstream file(current);
    if (!file.is_open()) {
      HashString(hash, "MISSING_FILE:" + current);
      continue;
    }

    std::string line;
    std::string parentDir = current.substr(0, current.find_last_of("/\\") + 1);

    while (std::getline(file, line)) {
      HashString(hash, line);
      std::smatch match;
      if (std::regex_search(line, match, includeRegex)) {
        std::string incFile = match[1].str();
        std::string fullIncPath = parentDir + incFile;
        outIncludeChain.push_back(fullIncPath);
        pending.push_back(fullIncPath);
      }
    }
  }

  return hash;
}

void ShaderReloadGovernance::Reset() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  m_records.clear();
}

void ShaderReloadGovernance::RecordReloadAttempt(const std::string &filePath, bool success,
                                                  uint64_t hash,
                                                  const std::vector<std::string> &includeChain,
                                                  std::string_view passName,
                                                  std::string_view resourceName,
                                                  std::string_view driverError) {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  auto &rec = m_records[filePath];
  rec.filePath = filePath;
  rec.currentHash = hash;
  rec.includeChain = includeChain;
  rec.passName = passName;
  rec.resourceName = resourceName;
  rec.isLastReloadSuccess = success;

  if (success) {
    rec.lastSuccessfulHash = hash;
    rec.driverErrorMessage.clear();
    rec.retryCount = 0;
  } else {
    rec.retryCount++;
    rec.driverErrorMessage = driverError;
    LOG_ERROR("ShaderReloadGovernance: Reload failed for '{}' (pass='{}', resource='{}', retry #{}):\nDriver Log:\n{}\nInclude Chain:\n{}",
              filePath, passName, resourceName, rec.retryCount, driverError, GenerateDiagnosticReport(filePath));
  }
}

ShaderReloadRecord ShaderReloadGovernance::GetRecord(const std::string &filePath) const {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  auto it = m_records.find(filePath);
  if (it != m_records.end()) {
    return it->second;
  }
  return {};
}

std::vector<ShaderReloadRecord> ShaderReloadGovernance::GetFailedReloadRecords() const {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  std::vector<ShaderReloadRecord> failed;
  for (const auto &[path, rec] : m_records) {
    if (!rec.isLastReloadSuccess) {
      failed.push_back(rec);
    }
  }
  return failed;
}

std::string ShaderReloadGovernance::GenerateDiagnosticReport(const std::string &filePath) const {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  auto it = m_records.find(filePath);
  if (it == m_records.end()) {
    return "No reload records for " + filePath;
  }

  const auto &rec = it->second;
  std::ostringstream ss;
  ss << "=== Shader Diagnostic Report ===\n";
  ss << "File: " << rec.filePath << "\n";
  ss << "Pass Context: " << rec.passName << "\n";
  ss << "Resource Context: " << rec.resourceName << "\n";
  ss << "Last Status: " << (rec.isLastReloadSuccess ? "SUCCESS" : "FAILED") << "\n";
  ss << "Last Successful Fingerprint: " << rec.lastSuccessfulHash << "\n";
  ss << "Current Fingerprint: " << rec.currentHash << "\n";
  ss << "Retry Count: " << rec.retryCount << "\n";

  ss << "Include Chain (" << rec.includeChain.size() << "):\n";
  for (size_t i = 0; i < rec.includeChain.size(); ++i) {
    ss << "  [" << i << "] " << rec.includeChain[i] << "\n";
  }

  if (!rec.driverErrorMessage.empty()) {
    ss << "Driver Diagnostics:\n" << rec.driverErrorMessage << "\n";
  }
  ss << "================================\n";
  return ss.str();
}

} // namespace NoMoreDay::render::debug
