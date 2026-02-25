#pragma once

#include "game/systems/combat/CombatEvents.hpp"
#include <array>
#include <atomic>
#include <cstdint>

namespace NoMoreDay {

#ifndef COMBAT_TELEMETRY_ENABLED
#define COMBAT_TELEMETRY_ENABLED 1
#endif

class CombatTelemetry {
public:
  static CombatTelemetry &Get();

  void BeginFrame(float dt) noexcept;

  void SetRuntimeEnabled(bool enabled) noexcept;
  [[nodiscard]] bool IsRuntimeEnabled() const noexcept;

  void SetOutputEnabled(bool enabled) noexcept;
  [[nodiscard]] bool IsOutputEnabled() const noexcept;
  void SetOutputIntervalSeconds(float seconds) noexcept;

  void RecordDamagePipelineDurationUs(double duration_us) noexcept;
  void RecordStatsQuery(bool cache_hit, uint64_t read_wait_ns,
                        uint64_t write_wait_ns) noexcept;
  void RecordCombatEvent(const CombatEvent &event) noexcept;
  void RecordTriggerAttempt(uint8_t depth) noexcept;
  void RecordTriggerBlocked(uint8_t depth) noexcept;
  void RecordTriggerDispatched(uint8_t depth) noexcept;
  void RecordSummonEntityCount(uint32_t active_count) noexcept;
  void RecordSummonTargetSwitch() noexcept;
  void RecordSummonBudgetRequest(bool granted) noexcept;
  void RecordSummonEvent() noexcept;

  void ResetForTests() noexcept;

private:
  CombatTelemetry();
  CombatTelemetry(const CombatTelemetry &) = delete;
  CombatTelemetry &operator=(const CombatTelemetry &) = delete;
};

} // namespace NoMoreDay
