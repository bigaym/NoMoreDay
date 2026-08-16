#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"

#include <memory>
#include <span>
#include <vector>

#include "raylib.h"

namespace NoMoreDay::render::lighting {

class LightManager {
public:
  struct ActiveLightRecord {
    components::GPULight gpuLight = {};
    uint8_t priority = 0;
  };
  struct ShadowMapAssignment {
    uint32_t lightIndex = 0u;
    uint32_t shadowMapIndex = 0u;
  };

  struct DebugStats {
    int ecsLights = 0;
    int transientLights = 0;
    int candidatesAfterCull = 0;
    int selectedLights = 0;
    int droppedByBudget = 0;
    int allowedLights = 0;
  };

  static LightManager &Get();

  void Initialize();
  void Shutdown();
  [[nodiscard]] bool IsInitialized() const { return m_lightBuffer != nullptr; }

  void UpdateCandidates(std::span<const components::GPULight> candidates,
                        const Camera2D &camera, int maxLights, int ecsLights);

  void Bind() const;

  [[nodiscard]] int GetActiveLightCount() const { return m_activeLightCount; }
  [[nodiscard]] uint32_t GetLightBufferId() const {
    return (m_lightBuffer != nullptr) ? m_lightBuffer->GetId() : 0u;
  }
  [[nodiscard]] const DebugStats &GetDebugStats() const { return m_debugStats; }
  [[nodiscard]] const std::vector<components::GPULight> &
  GetActiveLightsCpu() const {
    return m_stagingBuffer;
  }
  [[nodiscard]] const std::vector<ActiveLightRecord> &
  GetActiveLightRecordsCpu() const {
    return m_activeLightRecords;
  }

  void AddTransientLight(const components::GPULight &light);
  void ClearShadowMapIndices();
  void ApplyShadowMapAssignments(std::span<const ShadowMapAssignment> assignments);
  void SetDisableViewCullingForTesting(bool disabled) noexcept {
    m_disableViewCullingForTesting = disabled;
  }

private:
  LightManager() = default;

  std::unique_ptr<::NoMoreDay::core::ComputeBuffer> m_lightBuffer;
  std::vector<components::GPULight> m_stagingBuffer;
  std::vector<ActiveLightRecord> m_activeLightRecords;
  std::vector<components::GPULight> m_transientLights;
  int m_activeLightCount = 0;
  DebugStats m_debugStats = {};
  bool m_disableViewCullingForTesting = false;
};

} // namespace NoMoreDay::render::lighting
