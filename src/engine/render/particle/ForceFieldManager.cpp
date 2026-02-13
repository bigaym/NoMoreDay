#include "engine/render/particle/ForceFieldManager.hpp"

#include "core/logging/Logger.hpp"

#include <algorithm>
#include <cmath>

namespace NoMoreDay::render {
namespace {
constexpr float kInactiveEpsilon = 1e-5f;
}

ForceFieldManager &ForceFieldManager::Get() {
  static ForceFieldManager instance;
  return instance;
}

void ForceFieldManager::Init(int maxForceFields) {
  if (m_initialized) {
    return;
  }

  m_maxFields =
      std::clamp(maxForceFields, 1, NoMoreDay::Constants::GPU::MAX_FORCE_FIELDS);
  m_fields.assign(static_cast<size_t>(m_maxFields), components::GPUForceField{});
  for (auto &field : m_fields) {
    field.radius = 0.0f;
    field.strength = 0.0f;
  }

  m_ssbo.Create(static_cast<size_t>(m_maxFields) *
                sizeof(components::GPUForceField));
  m_ssbo.Update(m_fields.data(),
                static_cast<size_t>(m_maxFields) *
                    sizeof(components::GPUForceField));

  m_activeCount = 0;
  m_dirty = false;
  m_initialized = true;

  LOG_INFO("ForceFieldManager: Initialized with {} slots.", m_maxFields);
}

void ForceFieldManager::Shutdown() {
  m_ssbo.Release();
  m_fields.clear();
  m_activeCount = 0;
  m_initialized = false;
  m_dirty = false;
}

int ForceFieldManager::AddForceField(const components::GPUForceField &field) {
  if (!m_initialized) {
    return -1;
  }

  for (int i = 0; i < m_maxFields; ++i) {
    auto &slot = m_fields[static_cast<size_t>(i)];
    const bool inactive =
        (slot.radius <= kInactiveEpsilon) ||
        (std::fabs(slot.strength) <= kInactiveEpsilon);
    if (!inactive) {
      continue;
    }

    slot = field;
    slot.radius = std::max(0.0f, slot.radius);
    m_dirty = true;
    RecountActiveFields();
    return i;
  }

  return -1;
}

void ForceFieldManager::RemoveForceField(int id) {
  if (!IsValidId(id)) {
    return;
  }

  auto &slot = m_fields[static_cast<size_t>(id)];
  slot.strength = 0.0f;
  slot.radius = 0.0f;
  m_dirty = true;
  RecountActiveFields();
}

void ForceFieldManager::ClearAll() {
  if (!m_initialized) {
    return;
  }

  for (auto &field : m_fields) {
    field.strength = 0.0f;
    field.radius = 0.0f;
  }

  m_activeCount = 0;
  m_dirty = true;
}

void ForceFieldManager::SyncToGPU() {
  if (!m_initialized || !m_dirty) {
    return;
  }

  m_ssbo.Update(m_fields.data(),
                static_cast<size_t>(m_maxFields) *
                    sizeof(components::GPUForceField));
  m_dirty = false;
}

void ForceFieldManager::BindSSBO(uint32_t bindingPoint) const {
  if (!m_initialized) {
    return;
  }
  m_ssbo.BindBase(bindingPoint);
}

void ForceFieldManager::RecountActiveFields() {
  m_activeCount = static_cast<int>(std::count_if(
      m_fields.begin(), m_fields.end(), [](const components::GPUForceField &f) {
        return (f.radius > kInactiveEpsilon) &&
               (std::fabs(f.strength) > kInactiveEpsilon);
      }));
}

bool ForceFieldManager::IsValidId(int id) const {
  return m_initialized && id >= 0 && id < m_maxFields;
}

} // namespace NoMoreDay::render
