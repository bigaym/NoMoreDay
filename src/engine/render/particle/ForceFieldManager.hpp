#pragma once

#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/GPUData.hpp"

#include <cstdint>
#include <vector>

namespace NoMoreDay::render {

class ForceFieldManager {
public:
  static ForceFieldManager &Get();

  void Init(int maxForceFields = NoMoreDay::Constants::GPU::MAX_FORCE_FIELDS);
  void Shutdown();

  int AddForceField(const components::GPUForceField &field);
  void RemoveForceField(int id);
  void ClearAll();

  void SyncToGPU();
  void BindSSBO(uint32_t bindingPoint) const;

  [[nodiscard]] int GetActiveCount() const { return m_activeCount; }
  [[nodiscard]] bool IsInitialized() const { return m_initialized; }

private:
  void RecountActiveFields();
  [[nodiscard]] bool IsValidId(int id) const;

  NoMoreDay::core::ComputeBuffer m_ssbo;
  std::vector<components::GPUForceField> m_fields;
  int m_maxFields = NoMoreDay::Constants::GPU::MAX_FORCE_FIELDS;
  int m_activeCount = 0;
  bool m_initialized = false;
  bool m_dirty = false;
};

} // namespace NoMoreDay::render
