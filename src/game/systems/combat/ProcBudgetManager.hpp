#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

#include <entt/entt.hpp>

namespace NoMoreDay {

enum class ProcBudgetType : uint8_t {
  LifeOnHit = 0,
  ManaOnHit,
  AilmentProc,
  TriggerProc,
  EventEmit,
  Count
};

struct ProcBudgetConfig {
  float life_on_hit_per_sec = 80.0f;
  float mana_on_hit_per_sec = 120.0f;
  float ailment_proc_per_sec = 24.0f;
  float trigger_proc_per_sec = 18.0f;
  uint32_t event_emit_per_frame = 640u;
};

struct ProcBudgetRuntime {
  std::array<float, 4> tokens = {0.0f, 0.0f, 0.0f, 0.0f};
  std::array<float, 4> last_update_seconds = {0.0f, 0.0f, 0.0f, 0.0f};
  std::array<bool, 4> initialized = {false, false, false, false};
};

class ProcBudgetManager {
public:
  static ProcBudgetManager &Get();

  void BeginFrame(float dt);

  [[nodiscard]] bool RequestProc(entt::entity owner, ProcBudgetType type,
                                 float cost = 1.0f);
  [[nodiscard]] bool RequestEventEmit();

  [[nodiscard]] const ProcBudgetConfig &GetConfig() const noexcept;
  [[nodiscard]] bool
  LoadConfigFromFile(const std::string &path = "assets/data/proc_budget_config.json");

  void ResetForTests();
  void SetConfigForTests(const ProcBudgetConfig &config);

private:
  struct FrameCounter {
    uint32_t requests = 0;
    uint32_t allowed = 0;
  };

  void EnsureInitialized();
  void LogFrameSummary() const;
  void RefillRuntimeBucket(ProcBudgetRuntime &runtime, size_t bucket_index,
                           float budget_per_second);
  [[nodiscard]] float GetBudgetForType(ProcBudgetType type) const;

  ProcBudgetConfig m_config;
  bool m_config_loaded = false;
  bool m_frame_started = false;
  float m_elapsed_seconds = 0.0f;
  uint64_t m_frame_index = 0;
  uint32_t m_event_emits_this_frame = 0;

  std::array<FrameCounter, static_cast<size_t>(ProcBudgetType::Count)>
      m_frame_counters{};
  std::unordered_map<uint32_t, ProcBudgetRuntime> m_runtime_by_entity;
};

} // namespace NoMoreDay
