#include "engine/render/GPUABIContract.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUData.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace NoMoreDay::render::abi {
namespace {

constexpr const char *kVersionMacro = "#define GPU_ABI_VERSION";
constexpr const char *kCompatMinMacro = "#define GPU_ABI_COMPAT_MIN_VERSION";

int ParseMacroInt(const std::string &line, const char *macroName) {
  const std::string macro(macroName);
  const size_t pos = line.find(macro);
  if (pos == std::string::npos) {
    return -1;
  }
  std::istringstream stream(line.substr(pos + macro.size()));
  int value = -1;
  stream >> value;
  return value;
}

std::string BuildMismatchMessage(const ShaderABIInfo &shaderInfo,
                                 const std::string &path) {
  std::ostringstream oss;
  oss << "GPU ABI incompatibility detected for '" << path
      << "': CPU version=" << GPU_ABI_VERSION
      << " (supports [" << GPU_ABI_COMPAT_MIN_VERSION << ", "
      << GPU_ABI_VERSION << "])"
      << ", shader version=" << shaderInfo.version
      << " (compatMin=" << shaderInfo.compatMinVersion << ")";
  return oss.str();
}

} // namespace

ShaderABIInfo ReadShaderABIInfo(const std::string &path) {
  ShaderABIInfo info = {};
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("GPU ABI: failed to open generated include '{}'", path);
    return info;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (info.version < 0) {
      const int parsed = ParseMacroInt(line, kVersionMacro);
      if (parsed >= 0) {
        info.version = parsed;
      }
    }
    if (info.compatMinVersion < 0) {
      const int parsed = ParseMacroInt(line, kCompatMinMacro);
      if (parsed >= 0) {
        info.compatMinVersion = parsed;
      }
    }
    if (info.version >= 0 && info.compatMinVersion >= 0) {
      break;
    }
  }

  if (info.compatMinVersion < 0 && info.version >= 0) {
    // Keep backward compatibility for older generated files.
    info.compatMinVersion = info.version;
  }

  return info;
}

bool IsShaderABICompatible(const ShaderABIInfo &shaderInfo) {
  if (shaderInfo.version < 0 || shaderInfo.compatMinVersion < 0) {
    return false;
  }
  if (shaderInfo.compatMinVersion > shaderInfo.version) {
    return false;
  }

  const int cpuVersion = static_cast<int>(GPU_ABI_VERSION);
  const int cpuCompatMin = static_cast<int>(GPU_ABI_COMPAT_MIN_VERSION);
  return shaderInfo.version >= cpuCompatMin && shaderInfo.version <= cpuVersion;
}

bool ValidateGeneratedShaderABI(const std::string &path,
                                bool hardFailOnMismatch) {
  const ShaderABIInfo shaderInfo = ReadShaderABIInfo(path);
  if (IsShaderABICompatible(shaderInfo)) {
    if (shaderInfo.version != static_cast<int>(GPU_ABI_VERSION)) {
      LOG_WARN("GPU ABI: loaded compatible previous shader ABI version {} from {}",
               shaderInfo.version, path);
    }
    return true;
  }

  const std::string message = BuildMismatchMessage(shaderInfo, path);
  LOG_ERROR(message);
  if (hardFailOnMismatch) {
    throw std::logic_error(message);
  }
  return false;
}

} // namespace NoMoreDay::render::abi

