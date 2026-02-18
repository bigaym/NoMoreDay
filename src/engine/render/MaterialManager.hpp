#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/MaterialDefs.hpp"
#include "engine/render/RenderConstants.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace NoMoreDay::render {

class MaterialManager {
public:
  static MaterialManager &Get();

  void Initialize();
  void Shutdown();

  int RegisterMaterial(const MaterialInstance &mat, const std::string &name = "");
  int LoadFromJson(const std::string &path);
  void TryHotReload();

  [[nodiscard]] const MaterialInstance &GetMaterial(int materialId) const;
  [[nodiscard]] int GetMaterialId(const std::string &name) const;
  [[nodiscard]] int GetMaterialCount() const { return m_materialCount; }
  void SetRuntimePhaseShift(float roughnessScale, float specularScale,
                            float emissiveScale);
  void ResetRuntimePhaseShift();
  [[nodiscard]] bool HasRuntimePhaseShift() const {
    return m_runtimePhaseShiftActive;
  }
  [[nodiscard]] const components::GPUMaterialDataV2 &
  GetGpuMaterialForTesting(int materialId) const;

  void SyncToGPU();
  void BindSSBO(NoMoreDay::RenderConstants::Binding binding) const;

  static constexpr int MAX_MATERIALS = 256;
  static constexpr int PRESET_RESERVE = 8;
  static constexpr int MATERIAL_SCHEMA_MIN_VERSION = 1;
  static constexpr int MATERIAL_SCHEMA_VERSION = 2;

private:
  MaterialManager() = default;

  int RegisterPresetMaterial(int id, const MaterialInstance &mat,
                             const std::string &name);
  void RegisterPresets();
  [[nodiscard]] components::GPUMaterialDataV2 ToGpuData(
      const MaterialInstance &material) const;
  void MarkSlot(int id, const MaterialInstance &material, const std::string &name);
  void RebuildGpuBufferCache();

  std::array<MaterialInstance, MAX_MATERIALS> m_materials{};
  std::array<components::GPUMaterialDataV2, MAX_MATERIALS> m_gpuMaterials{};
  std::array<bool, MAX_MATERIALS> m_registered{};
  std::unordered_map<std::string, int> m_nameToId;
  std::unordered_set<std::string> m_v1WarnedAssets;

  int m_materialCount = 0;
  int m_nextDynamicId = PRESET_RESERVE;
  int m_gpuUploadCount = 0;
  bool m_dirty = true;
  bool m_initialized = false;
  bool m_runtimePhaseShiftActive = false;
  float m_runtimeRoughnessScale = 1.0f;
  float m_runtimeSpecularScale = 1.0f;
  float m_runtimeEmissiveScale = 1.0f;

  ::NoMoreDay::core::ComputeBuffer m_ssbo;
  std::string m_jsonPath;
  std::filesystem::file_time_type m_lastModified{};
};

} // namespace NoMoreDay::render
