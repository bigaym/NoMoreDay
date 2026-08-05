#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace NoMoreDay::render::core {

struct FormatSupportInfo {
  bool r8 = false;
  bool rg16f = false;
  bool rgba16f = false;
  bool r32f = false;
  bool depth24stencil8 = false;
};

struct CapabilityReport {
  bool isGL43Supported = false;
  bool isComputeSupported = false;
  bool isSSBOSupported = false;
  bool isImageLoadStoreSupported = false;
  bool isMemoryBarrierSupported = false;
  bool isTimerQuerySupported = false;
  bool isDebugCallbackSupported = false;
  uint32_t maxSSBOBindings = 0;
  FormatSupportInfo formatSupport;

  bool isFullyCompatible = false;
  std::vector<std::string> capabilityGaps;

  std::string DumpReport() const;
};

// Phase F (RG-4): result of evaluating a CapabilityReport against the
// production-critical feature set. Missing any required feature fails closed -
// the renderer must report loudly instead of silently degrading.
struct ProductionCapabilityCheck {
  bool passed = false;
  std::vector<std::string> missingRequirements;
};

class DeviceCapabilityMatrix {
public:
  static DeviceCapabilityMatrix &Get();

  CapabilityReport ProbeCapabilities();
  const CapabilityReport &GetCachedReport() const { return m_cachedReport; }

  // Evaluates a report against the production-critical requirements:
  // GL 4.3 core profile, compute shaders, SSBO, image load/store and
  // glMemoryBarrier. Pure (no GL), so it is unit-testable with a fabricated
  // report.
  static ProductionCapabilityCheck CheckProductionRequirements(
      const CapabilityReport &report);

private:
  DeviceCapabilityMatrix() = default;
  ~DeviceCapabilityMatrix() = default;

  CapabilityReport m_cachedReport;
  bool m_probed = false;
};

} // namespace NoMoreDay::render::core
