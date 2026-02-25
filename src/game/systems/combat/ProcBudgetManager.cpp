#include "game/systems/combat/ProcBudgetManager.hpp"

#include "spdlog/spdlog.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <optional>

#include <nlohmann/json.hpp>

namespace NoMoreDay {
namespace {

constexpr float kBudgetEpsilon = 0.0001f;
constexpr size_t kPerSecondBucketCount = 4;

constexpr size_t ToIndex(const ProcBudgetType type) {
  return static_cast<size_t>(type);
}

const char *BudgetTypeToString(const ProcBudgetType type) {
  switch (type) {
  case ProcBudgetType::LifeOnHit:
    return "life_on_hit_per_sec";
  case ProcBudgetType::ManaOnHit:
    return "mana_on_hit_per_sec";
  case ProcBudgetType::AilmentProc:
    return "ailment_proc_per_sec";
  case ProcBudgetType::TriggerProc:
    return "trigger_proc_per_sec";
  case ProcBudgetType::EventEmit:
    return "event_emit_per_frame";
  case ProcBudgetType::Count:
    break;
  }
  return "unknown";
}

float ClampNonNegative(const float value) {
  return (std::max)(0.0f, value);
}

uint32_t ClampEventCap(const uint32_t value) {
  constexpr uint32_t kMaxEventCap = 1'000'000u;
  return (std::min)(value, kMaxEventCap);
}

ProcBudgetConfig SanitizeConfig(ProcBudgetConfig config) {
  config.life_on_hit_per_sec = ClampNonNegative(config.life_on_hit_per_sec);
  config.mana_on_hit_per_sec = ClampNonNegative(config.mana_on_hit_per_sec);
  config.ailment_proc_per_sec = ClampNonNegative(config.ailment_proc_per_sec);
  config.trigger_proc_per_sec = ClampNonNegative(config.trigger_proc_per_sec);
  config.event_emit_per_frame = ClampEventCap(config.event_emit_per_frame);
  return config;
}

std::optional<float> ReadOptionalFloat(const nlohmann::json &root,
                                       const char *key) {
  const auto it = root.find(key);
  if (it == root.end() || !it->is_number()) {
    return std::nullopt;
  }
  return it->get<float>();
}

std::optional<uint32_t> ReadOptionalUInt(const nlohmann::json &root,
                                         const char *key) {
  const auto it = root.find(key);
  if (it == root.end()) {
    return std::nullopt;
  }
  if (it->is_number_unsigned()) {
    return it->get<uint32_t>();
  }
  if (it->is_number_integer()) {
    return static_cast<uint32_t>((std::max)(0, it->get<int>()));
  }
  return std::nullopt;
}

} // namespace

ProcBudgetManager &ProcBudgetManager::Get() {
  static ProcBudgetManager instance;
  return instance;
}

void ProcBudgetManager::EnsureInitialized() {
  if (!m_config_loaded) {
    (void)LoadConfigFromFile();
  }
}

float ProcBudgetManager::GetBudgetForType(const ProcBudgetType type) const {
  switch (type) {
  case ProcBudgetType::LifeOnHit:
    return m_config.life_on_hit_per_sec;
  case ProcBudgetType::ManaOnHit:
    return m_config.mana_on_hit_per_sec;
  case ProcBudgetType::AilmentProc:
    return m_config.ailment_proc_per_sec;
  case ProcBudgetType::TriggerProc:
    return m_config.trigger_proc_per_sec;
  case ProcBudgetType::EventEmit:
  case ProcBudgetType::Count:
    return 0.0f;
  }
  return 0.0f;
}

void ProcBudgetManager::RefillRuntimeBucket(ProcBudgetRuntime &runtime,
                                            const size_t bucket_index,
                                            const float budget_per_second) {
  if (bucket_index >= kPerSecondBucketCount) {
    return;
  }

  if (!runtime.initialized[bucket_index]) {
    runtime.tokens[bucket_index] = budget_per_second;
    runtime.last_update_seconds[bucket_index] = m_elapsed_seconds;
    runtime.initialized[bucket_index] = true;
    return;
  }

  const float elapsed =
      m_elapsed_seconds - runtime.last_update_seconds[bucket_index];
  if (elapsed > 0.0f) {
    runtime.tokens[bucket_index] =
        (std::min)(budget_per_second,
                   runtime.tokens[bucket_index] + elapsed * budget_per_second);
    runtime.last_update_seconds[bucket_index] = m_elapsed_seconds;
  }
}

void ProcBudgetManager::LogFrameSummary() const {
  for (size_t idx = 0; idx < m_frame_counters.size(); ++idx) {
    const auto &counter = m_frame_counters[idx];
    if (counter.requests == 0 || counter.allowed >= counter.requests) {
      continue;
    }

    const uint32_t denied = counter.requests - counter.allowed;
    const float downsample_rate =
        static_cast<float>(denied) / static_cast<float>(counter.requests) *
        100.0f;

    const ProcBudgetType type = static_cast<ProcBudgetType>(idx);
    if (type == ProcBudgetType::EventEmit) {
      spdlog::info(
          "[ProcBudget] frame={} dim={} allowed={} denied={} drop_rate={:.2f}% "
          "cap={}",
          m_frame_index, BudgetTypeToString(type), counter.allowed, denied,
          downsample_rate, m_config.event_emit_per_frame);
    } else {
      spdlog::info(
          "[ProcBudget] frame={} dim={} allowed={} denied={} drop_rate={:.2f}% "
          "budget_per_sec={:.2f}",
          m_frame_index, BudgetTypeToString(type), counter.allowed, denied,
          downsample_rate, GetBudgetForType(type));
    }
  }
}

void ProcBudgetManager::BeginFrame(const float dt) {
  EnsureInitialized();

  m_elapsed_seconds += (std::max)(0.0f, dt);

  if (m_frame_started) {
    LogFrameSummary();
  }

  for (auto &counter : m_frame_counters) {
    counter.requests = 0;
    counter.allowed = 0;
  }

  m_event_emits_this_frame = 0;
  ++m_frame_index;
  m_frame_started = true;
}

bool ProcBudgetManager::RequestProc(const entt::entity owner,
                                    const ProcBudgetType type,
                                    const float cost) {
  EnsureInitialized();

  if (!m_frame_started) {
    return true;
  }

  if (type == ProcBudgetType::EventEmit) {
    return RequestEventEmit();
  }

  const size_t idx = ToIndex(type);
  auto &counter = m_frame_counters[idx];
  ++counter.requests;

  if (owner == entt::null) {
    ++counter.allowed;
    return true;
  }

  const float sanitized_cost = ClampNonNegative(cost);
  if (sanitized_cost <= kBudgetEpsilon) {
    ++counter.allowed;
    return true;
  }

  const float budget_per_second = GetBudgetForType(type);
  if (budget_per_second <= kBudgetEpsilon) {
    return false;
  }

  auto &runtime = m_runtime_by_entity[static_cast<uint32_t>(owner)];
  RefillRuntimeBucket(runtime, idx, budget_per_second);

  if (runtime.tokens[idx] + kBudgetEpsilon < sanitized_cost) {
    return false;
  }

  runtime.tokens[idx] = (std::max)(0.0f, runtime.tokens[idx] - sanitized_cost);
  ++counter.allowed;
  return true;
}

bool ProcBudgetManager::RequestEventEmit() {
  EnsureInitialized();

  if (!m_frame_started) {
    return true;
  }

  const size_t idx = ToIndex(ProcBudgetType::EventEmit);
  auto &counter = m_frame_counters[idx];
  ++counter.requests;

  if (m_config.event_emit_per_frame == 0u) {
    return false;
  }

  if (m_event_emits_this_frame >= m_config.event_emit_per_frame) {
    return false;
  }

  ++m_event_emits_this_frame;
  ++counter.allowed;
  return true;
}

const ProcBudgetConfig &ProcBudgetManager::GetConfig() const noexcept {
  return m_config;
}

bool ProcBudgetManager::LoadConfigFromFile(const std::string &path) {
  ProcBudgetConfig loaded = m_config;
  loaded = SanitizeConfig(loaded);

  std::ifstream file(path);
  if (!file.is_open()) {
    m_config = loaded;
    m_config_loaded = true;
    spdlog::warn("[ProcBudget] config '{}' not found, using defaults.", path);
    return false;
  }

  nlohmann::json root;
  try {
    file >> root;
  } catch (const std::exception &e) {
    m_config = loaded;
    m_config_loaded = true;
    spdlog::warn("[ProcBudget] failed to parse '{}': {}. Using defaults.", path,
                 e.what());
    return false;
  }

  if (const auto value = ReadOptionalFloat(root, "life_on_hit_per_sec");
      value.has_value()) {
    loaded.life_on_hit_per_sec = *value;
  }
  if (const auto value = ReadOptionalFloat(root, "mana_on_hit_per_sec");
      value.has_value()) {
    loaded.mana_on_hit_per_sec = *value;
  }
  if (const auto value = ReadOptionalFloat(root, "ailment_proc_per_sec");
      value.has_value()) {
    loaded.ailment_proc_per_sec = *value;
  }
  if (const auto value = ReadOptionalFloat(root, "trigger_proc_per_sec");
      value.has_value()) {
    loaded.trigger_proc_per_sec = *value;
  }
  if (const auto value = ReadOptionalUInt(root, "event_emit_per_frame");
      value.has_value()) {
    loaded.event_emit_per_frame = *value;
  }

  m_config = SanitizeConfig(loaded);
  m_config_loaded = true;
  return true;
}

void ProcBudgetManager::ResetForTests() {
  m_config = ProcBudgetConfig{};
  m_config = SanitizeConfig(m_config);
  m_config_loaded = true;
  m_frame_started = false;
  m_elapsed_seconds = 0.0f;
  m_frame_index = 0;
  m_event_emits_this_frame = 0;
  for (auto &counter : m_frame_counters) {
    counter.requests = 0;
    counter.allowed = 0;
  }
  m_runtime_by_entity.clear();
}

void ProcBudgetManager::SetConfigForTests(const ProcBudgetConfig &config) {
  m_config = SanitizeConfig(config);
  m_config_loaded = true;
  m_runtime_by_entity.clear();
  m_event_emits_this_frame = 0;
  for (auto &counter : m_frame_counters) {
    counter.requests = 0;
    counter.allowed = 0;
  }
}

} // namespace NoMoreDay
