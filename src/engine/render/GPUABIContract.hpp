#pragma once

#include <cstdint>
#include <string>

namespace NoMoreDay::render::abi {

struct ShaderABIInfo {
  int version = -1;
  int compatMinVersion = -1;
};

[[nodiscard]] ShaderABIInfo ReadShaderABIInfo(const std::string &path);
[[nodiscard]] bool IsShaderABICompatible(const ShaderABIInfo &shaderInfo);
bool ValidateGeneratedShaderABI(const std::string &path,
                                bool hardFailOnMismatch);

} // namespace NoMoreDay::render::abi
