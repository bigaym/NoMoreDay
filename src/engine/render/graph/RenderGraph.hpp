#pragma once

#include "engine/render/graph/RenderPass.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace NoMoreDay::render::graph {

constexpr uint32_t RENDERGRAPH_CONTRACT_VERSION = 2;

enum class RenderResourceTag : uint8_t {
  Custom = 0,
  SceneHdrColor,
  SceneDepth,
  PostProcessLdrColor,
  DistortionLdrColor,
  FinalOutputColor,
};

enum class RenderOwnerTag : uint8_t {
  Unknown = 0,
  Scene,
  Lighting,
  HeightShadow,
  Volumetric,
  VFX,
  UIWorld,
  PostProcess,
  Distortion,
  Composite,
};

constexpr const char *ToResourceName(RenderResourceTag resourceTag) {
  switch (resourceTag) {
  case RenderResourceTag::SceneHdrColor:
    return "SceneColor";
  case RenderResourceTag::SceneDepth:
    return "SceneDepth";
  case RenderResourceTag::PostProcessLdrColor:
    return "PostProcessColor";
  case RenderResourceTag::DistortionLdrColor:
    return "DistortionColor";
  case RenderResourceTag::FinalOutputColor:
    return "BackBuffer";
  case RenderResourceTag::Custom:
  default:
    return "";
  }
}

constexpr RenderResourceTag ToResourceTag(std::string_view resourceName) {
  if (resourceName == "SceneColor") {
    return RenderResourceTag::SceneHdrColor;
  }
  if (resourceName == "SceneDepth") {
    return RenderResourceTag::SceneDepth;
  }
  if (resourceName == "PostProcessColor") {
    return RenderResourceTag::PostProcessLdrColor;
  }
  if (resourceName == "DistortionColor") {
    return RenderResourceTag::DistortionLdrColor;
  }
  if (resourceName == "BackBuffer") {
    return RenderResourceTag::FinalOutputColor;
  }
  return RenderResourceTag::Custom;
}

constexpr const char *ToOwnerName(RenderOwnerTag ownerTag) {
  switch (ownerTag) {
  case RenderOwnerTag::Scene:
    return "Scene";
  case RenderOwnerTag::Lighting:
    return "Lighting";
  case RenderOwnerTag::HeightShadow:
    return "HeightShadow";
  case RenderOwnerTag::Volumetric:
    return "Volumetric";
  case RenderOwnerTag::VFX:
    return "VFX";
  case RenderOwnerTag::UIWorld:
    return "UIWorld";
  case RenderOwnerTag::PostProcess:
    return "PostProcess";
  case RenderOwnerTag::Distortion:
    return "Distortion";
  case RenderOwnerTag::Composite:
    return "Composite";
  case RenderOwnerTag::Unknown:
  default:
    return "Unknown";
  }
}

struct ResourceAccess {
  enum class Type {
    Read,
    Write,
  };

  std::string resourceName;
  Type type = Type::Read;
  RenderResourceTag resourceTag = RenderResourceTag::Custom;
  RenderOwnerTag ownerTag = RenderOwnerTag::Unknown;
};

class RenderGraphBuilder {
public:
  void Read(const std::string &resourceName);
  void Write(const std::string &resourceName);
  void Read(RenderResourceTag resourceTag, RenderOwnerTag ownerTag);
  void Write(RenderResourceTag resourceTag, RenderOwnerTag ownerTag);

  const std::vector<ResourceAccess> &GetAccesses() const { return m_accesses; }

private:
  std::vector<ResourceAccess> m_accesses;
};

struct RenderContext;

class RenderGraph {
public:
  struct ValidationDiagnostic {
    enum class Severity {
      Warning,
      Error,
    };

    Severity severity = Severity::Warning;
    size_t passIndex = 0;
    std::string passName;
    std::string resourceName;
    std::string message;
  };

  void AddPass(std::shared_ptr<RenderPass> pass);
  void Clear();
  void Build();
  void Execute(RenderContext &context);
  static void SetValidationEnabled(bool enabled);
  static bool IsValidationEnabled();

  size_t GetPassCount() const { return m_nodes.size(); }
  const std::vector<ValidationDiagnostic> &GetValidationDiagnostics() const {
    return m_validationDiagnostics;
  }
  bool HasValidationErrors() const { return m_hasValidationErrors; }

private:
  struct Node {
    std::shared_ptr<RenderPass> pass;
    std::string passName;
    size_t passIndex = 0;
    std::vector<ResourceAccess> accesses;
  };

  void ValidateBuildContracts();
  void AddValidationDiagnostic(ValidationDiagnostic::Severity severity,
                               size_t passIndex,
                               const std::string &passName,
                               const std::string &resourceName,
                               const std::string &message);

  std::vector<Node> m_nodes;
  std::vector<ValidationDiagnostic> m_validationDiagnostics;
  bool m_hasValidationErrors = false;
  bool m_isBuilt = false;
  static bool s_validationEnabled;
};

} // namespace NoMoreDay::render::graph
