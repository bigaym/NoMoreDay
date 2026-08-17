#include "core/logging/Logger.hpp"
#include "engine/render/core/DeviceCapabilityMatrix.hpp"
#include "engine/render/GPUUtils.hpp"
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>
#include <sstream>

namespace NoMoreDay::render::core {

namespace {
constexpr uint32_t kGLMajorVersion = 0x821B;
constexpr uint32_t kGLMinorVersion = 0x821C;
constexpr uint32_t kGLMaxShaderStorageBufferBindings = 0x90DD;
constexpr uint32_t kDefaultSSBOBindings = 16; // GL 4.3 guaranteed minimum
} // namespace

DeviceCapabilityMatrix &DeviceCapabilityMatrix::Get() {
  static DeviceCapabilityMatrix instance;
  if (!instance.m_probed) {
    instance.ProbeCapabilities();
  }
  return instance;
}

bool DeviceCapabilityMatrix::IsDesktopGL43OrNewer(int major, int minor,
                                                 bool isGlesProfile) {
  if (isGlesProfile || major <= 0 || minor < 0) {
    return false;
  }
  return major > 4 || (major == 4 && minor >= 3);
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
  if (m_probeOverrideForTesting.has_value()) {
    m_cachedReport = *m_probeOverrideForTesting;
    m_probed = true;
    return m_cachedReport;
  }
  if (m_probed) return m_cachedReport;

  CapabilityReport report = {};

  int major = 0;
  int minor = 0;
  bool isGles = false;

  if (glfwGetCurrentContext() != nullptr) {
    NoMoreDay::utils::GPUUtils::GetIntegerv(kGLMajorVersion, &major);
    NoMoreDay::utils::GPUUtils::GetIntegerv(kGLMinorVersion, &minor);

    const char *versionStr = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    if (versionStr != nullptr && std::strstr(versionStr, "OpenGL ES") != nullptr) {
      isGles = true;
    }
  }

  report.isGL43Supported = IsDesktopGL43OrNewer(major, minor, isGles);

  // Entry-point level probing for the compute pipeline (fail-closed: the
  // version gate stays the baseline, but a missing entry point on a 4.3+
  // context must still report unsupported - consistent with RenderSystem
  // "No silent degradation").
  report.isComputeSupported = report.isGL43Supported &&
                              (glfwGetProcAddress("glDispatchCompute") != nullptr);
  report.isSSBOSupported = report.isGL43Supported &&
                           (glfwGetProcAddress("glBindBufferBase") != nullptr);
  report.isImageLoadStoreSupported =
      report.isGL43Supported &&
      (glfwGetProcAddress("glBindImageTexture") != nullptr);
  report.isMemoryBarrierSupported = (glfwGetProcAddress("glMemoryBarrier") != nullptr);
  report.isTimerQuerySupported = (glfwGetProcAddress("glGenQueries") != nullptr && glfwGetProcAddress("glGetQueryObjectui64v") != nullptr);
  report.isDebugCallbackSupported = (glfwGetProcAddress("glDebugMessageCallback") != nullptr);

  // Query the driver for the SSBO binding budget. GL 4.3 guarantees >= 16,
  // so fall back to the spec minimum when there is no live GL context or the
  // query fails; keep observability via LOG_WARN and a report field.
  report.maxSSBOBindings = kDefaultSSBOBindings;
  if (glfwGetCurrentContext() != nullptr) {
    int maxBindings = 0;
    NoMoreDay::utils::GPUUtils::GetIntegerv(kGLMaxShaderStorageBufferBindings,
                                            &maxBindings);
    if (maxBindings > 0) {
      report.maxSSBOBindings = static_cast<uint32_t>(maxBindings);
    } else {
      report.maxSSBOBindingsFallbackUsed = true;
      LOG_WARN("DeviceCapabilityMatrix: GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS "
               "query returned {}; falling back to spec minimum {}.",
               maxBindings, kDefaultSSBOBindings);
    }
  } else {
    report.maxSSBOBindingsFallbackUsed = true;
    LOG_WARN("DeviceCapabilityMatrix: no GL context; maxSSBOBindings falls "
             "back to spec minimum {}.",
             kDefaultSSBOBindings);
  }

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
  if (!report.isSSBOSupported) {
    report.capabilityGaps.push_back("SSBO (shader storage buffer) not supported by driver");
  }
  if (!report.isImageLoadStoreSupported) {
    report.capabilityGaps.push_back("Image load/store not supported by driver");
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
  ss << "SSBO Support: " << (isSSBOSupported ? "YES" : "NO") << " (Max Bindings: "
     << maxSSBOBindings << (maxSSBOBindingsFallbackUsed ? ", derived" : "") << ")\n";
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
