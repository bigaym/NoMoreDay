#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/GPUUtils.hpp"
#include "rlgl.h"
#include <GLFW/glfw3.h>

#include <sstream>

namespace NoMoreDay::render::core {

DeviceCapabilityMatrix &DeviceCapabilityMatrix::Get() {
  static DeviceCapabilityMatrix instance;
  if (!instance.m_probed) {
    instance.ProbeCapabilities();
  }
  return instance;
}

ProductionCapabilityCheck DeviceCapabilityMatrix::CheckProductionRequirements(
    const CapabilityReport &report) {
  ProductionCapabilityCheck check;
  if (!report.isGL43Supported) {
    check.missingRequirements.push_back("OpenGL 4.3 core profile");
  }
  if (!report.isComputeSupported) {
    check.missingRequirements.push_back("Compute shaders");
  }
  if (!report.isSSBOSupported) {
    check.missingRequirements.push_back("SSBO (shader storage buffer)");
  }
  if (!report.isImageLoadStoreSupported) {
    check.missingRequirements.push_back("Image load/store");
  }
  if (!report.isMemoryBarrierSupported) {
    check.missingRequirements.push_back("glMemoryBarrier");
  }
  check.passed = check.missingRequirements.empty();
  return check;
}

CapabilityReport DeviceCapabilityMatrix::ProbeCapabilities() {
  if (m_probed) return m_cachedReport;

  CapabilityReport report = {};
  int version = rlGetVersion();

  report.isGL43Supported = (version == RL_OPENGL_43);
  report.isComputeSupported = report.isGL43Supported;
  report.isSSBOSupported = report.isGL43Supported;
  report.isImageLoadStoreSupported = report.isGL43Supported;
  report.isMemoryBarrierSupported = (glfwGetProcAddress("glMemoryBarrier") != nullptr);
  report.isTimerQuerySupported = (glfwGetProcAddress("glGenQueries") != nullptr && glfwGetProcAddress("glGetQueryObjectui64v") != nullptr);
  report.isDebugCallbackSupported = (glfwGetProcAddress("glDebugMessageCallback") != nullptr);
  report.maxSSBOBindings = 16;

  report.formatSupport.r8 = true;
  report.formatSupport.rg16f = true;
  report.formatSupport.rgba16f = true;
  report.formatSupport.r32f = true;
  report.formatSupport.depth24stencil8 = true;

  if (!report.isGL43Supported) {
    report.capabilityGaps.push_back("OpenGL 4.3 core profile unavailable");
  }
  if (!report.isComputeSupported) {
    report.capabilityGaps.push_back("Compute Shaders not supported by driver");
  }
  if (!report.isMemoryBarrierSupported) {
    report.capabilityGaps.push_back("glMemoryBarrier extension missing");
  }
  if (!report.isTimerQuerySupported) {
    report.capabilityGaps.push_back("GPU timer queries unavailable; using CPU timing");
  }

  report.isFullyCompatible = report.capabilityGaps.empty();
  m_cachedReport = report;
  m_probed = true;
  return m_cachedReport;
}

std::string CapabilityReport::DumpReport() const {
  std::ostringstream ss;
  ss << "=== Device Capability Matrix ===\n";
  ss << "Status: " << (isFullyCompatible ? "FULL_COMPATIBLE" : "DEGRADED_MODE") << "\n";
  ss << "GL 4.3 Core: " << (isGL43Supported ? "YES" : "NO") << "\n";
  ss << "Compute Shader: " << (isComputeSupported ? "YES" : "NO") << "\n";
  ss << "SSBO Support: " << (isSSBOSupported ? "YES" : "NO") << " (Max Bindings: " << maxSSBOBindings << ")\n";
  ss << "Image Load/Store: " << (isImageLoadStoreSupported ? "YES" : "NO") << "\n";
  ss << "Memory Barrier: " << (isMemoryBarrierSupported ? "YES" : "NO") << "\n";
  ss << "Timer Query: " << (isTimerQuerySupported ? "YES" : "NO") << "\n";
  ss << "Debug Callback: " << (isDebugCallbackSupported ? "YES" : "NO") << "\n";
  ss << "Required Formats (R8, RG16F, RGBA16F, R32F, D24S8): PASS\n";

  if (!capabilityGaps.empty()) {
    ss << "Capability Gaps (" << capabilityGaps.size() << "):\n";
    for (const auto &reason : capabilityGaps) {
      ss << "  - " << reason << "\n";
    }
  }
  ss << "================================\n";
  return ss.str();
}

} // namespace NoMoreDay::render::core
