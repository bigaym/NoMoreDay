#pragma once

#include <cstdint>
#include <string>

namespace NoMoreDay::render::abi {

struct ShaderABIInfo {
  int version = -1;
  int compatMinVersion = -1;
};

inline std::string &GetAbiManifestPathOverride() {
  static std::string s_overridePath;
  return s_overridePath;
}

[[nodiscard]] inline const char *GetGeneratedShaderABIManifest() noexcept {
  const auto &overridePath = GetAbiManifestPathOverride();
  if (!overridePath.empty()) {
    return overridePath.c_str();
  }
  return "assets/shaders/generated/gpu_abi.glslinc";
}

inline void SetGeneratedShaderABIManifestForTesting(const std::string &path) {
  GetAbiManifestPathOverride() = path;
}

inline void ResetGeneratedShaderABIManifestForTesting() {
  GetAbiManifestPathOverride().clear();
}

[[nodiscard]] ShaderABIInfo ReadShaderABIInfo(const std::string &path);
[[nodiscard]] bool IsShaderABICompatible(const ShaderABIInfo &shaderInfo);
bool ValidateGeneratedShaderABI(const std::string &path = GetGeneratedShaderABIManifest(),
                                bool hardFailOnMismatch = false);

} // namespace NoMoreDay::render::abi
