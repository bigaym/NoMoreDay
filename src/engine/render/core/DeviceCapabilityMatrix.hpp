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

class DeviceCapabilityMatrix {
public:
  static DeviceCapabilityMatrix &Get();

  CapabilityReport ProbeCapabilities();
  const CapabilityReport &GetCachedReport() const { return m_cachedReport; }

private:
  DeviceCapabilityMatrix() = default;
  ~DeviceCapabilityMatrix() = default;

  CapabilityReport m_cachedReport;
  bool m_probed = false;
};

} // namespace NoMoreDay::render::core
