#include "game/systems/combat/CombatTelemetry.hpp"

#include "spdlog/spdlog.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace NoMoreDay {

namespace {

#if COMBAT_TELEMETRY_ENABLED

constexpr size_t kEventTypeCount = static_cast<size_t>(CombatEventType::Count);
constexpr size_t kDamageWindowSize = 2048;
constexpr size_t kTriggerDepthBuckets = 4; // depth 0/1/2/3+
constexpr float kMinOutputIntervalSeconds = 0.1f;
constexpr float kMaxOutputIntervalSeconds = 30.0f;

struct DamageWindowStats {
  double avg_us = 0.0;
  double p95_us = 0.0;
  double p99_us = 0.0;
  uint32_t sample_count = 0;
};

struct TelemetryState {
  std::atomic<bool> runtime_enabled{true};
  std::atomic<bool> output_enabled{true};
  std::atomic<uint32_t> output_interval_ms{1000};

  std::array<std::atomic<uint32_t>, kDamageWindowSize> damage_window_us{};
  std::atomic<uint32_t> damage_write_index{0};
  std::atomic<uint32_t> damage_count{0};

  std::atomic<uint64_t> stats_calls{0};
  std::atomic<uint64_t> stats_cache_hits{0};
  std::atomic<uint64_t> stats_read_wait_ns{0};
  std::atomic<uint64_t> stats_write_wait_ns{0};

  std::array<std::atomic<uint32_t>, kEventTypeCount> event_frame_counts{};
  std::atomic<uint32_t> event_frame_total{0};
  std::array<uint32_t, kEventTypeCount> event_last_frame_counts{};
  uint32_t event_last_frame_total = 0;

  std::atomic<uint64_t> trigger_attempts{0};
  std::atomic<uint64_t> trigger_blocked{0};
  std::atomic<uint64_t> trigger_dispatched{0};
  std::array<std::atomic<uint64_t>, kTriggerDepthBuckets> trigger_attempt_depth{};
  std::array<std::atomic<uint64_t>, kTriggerDepthBuckets> trigger_dispatch_depth{};

  std::atomic<uint32_t> summon_entity_count_frame{0};
  uint32_t summon_entity_count_last_frame = 0;
  std::atomic<uint64_t> summon_target_switches{0};
  std::atomic<uint64_t> summon_budget_requests{0};
  std::atomic<uint64_t> summon_budget_granted{0};
  std::atomic<uint64_t> summon_events{0};

  float elapsed_since_output = 0.0f;
  std::mutex frame_mutex;
};

TelemetryState &GetState() {
  static TelemetryState state;
  return state;
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

bool ParseBool(const char *raw, const bool default_value) {
  if (!raw || raw[0] == '\0') {
    return default_value;
  }

  const std::string normalized = ToLower(raw);
  if (normalized == "1" || normalized == "true" || normalized == "on" ||
      normalized == "yes") {
    return true;
  }
  if (normalized == "0" || normalized == "false" || normalized == "off" ||
      normalized == "no") {
    return false;
  }
  return default_value;
}

uint32_t ParseIntervalMs(const char *raw, const uint32_t default_ms) {
  if (!raw || raw[0] == '\0') {
    return default_ms;
  }
  char *end_ptr = nullptr;
  const double parsed = std::strtod(raw, &end_ptr);
  if (end_ptr == raw) {
    return default_ms;
  }
  const double clamped_seconds =
      std::clamp(parsed, static_cast<double>(kMinOutputIntervalSeconds),
                 static_cast<double>(kMaxOutputIntervalSeconds));
  return static_cast<uint32_t>(clamped_seconds * 1000.0);
}

const char *EventTypeName(const CombatEventType type) {
  switch (type) {
  case CombatEventType::OnSkillCast:
    return "OnSkillCast";
  case CombatEventType::OnSkillHit:
    return "OnSkillHit";
  case CombatEventType::OnCrit:
    return "OnCrit";
  case CombatEventType::OnDodge:
    return "OnDodge";
  case CombatEventType::OnBlock:
    return "OnBlock";
  case CombatEventType::OnTakeDamage:
    return "OnTakeDamage";
  case CombatEventType::OnDealDamage:
    return "OnDealDamage";
  case CombatEventType::OnKill:
    return "OnKill";
  case CombatEventType::OnHeal:
    return "OnHeal";
  case CombatEventType::OnOverkill:
    return "OnOverkill";
  case CombatEventType::OnMeleeHit:
    return "OnMeleeHit";
  case CombatEventType::OnProjectileHit:
    return "OnProjectileHit";
  case CombatEventType::OnAreaHit:
    return "OnAreaHit";
  case CombatEventType::OnApplyAilment:
    return "OnApplyAilment";
  case CombatEventType::OnReceiveAilment:
    return "OnReceiveAilment";
  case CombatEventType::OnStun:
    return "OnStun";
  case CombatEventType::OnLowHealth:
    return "OnLowHealth";
  case CombatEventType::OnFullHealth:
    return "OnFullHealth";
  case CombatEventType::OnManaSpent:
    return "OnManaSpent";
  case CombatEventType::OnUsePotion:
    return "OnUsePotion";
  case CombatEventType::OnResourceConsumed:
    return "OnResourceConsumed";
  case CombatEventType::OnChannelTick:
    return "OnChannelTick";
  case CombatEventType::OnChannelEnd:
    return "OnChannelEnd";
  case CombatEventType::OnDash:
    return "OnDash";
  case CombatEventType::OnMoveDistance:
    return "OnMoveDistance";
  case CombatEventType::OnGoldPickup:
    return "OnGoldPickup";
  case CombatEventType::OnSummon:
    return "OnSummon";
  case CombatEventType::OnMinionDeath:
    return "OnMinionDeath";
  case CombatEventType::OnMinionHit:
    return "OnMinionHit";
  case CombatEventType::Count:
    return "Count";
  }
  return "Unknown";
}

size_t ClampDepthBucket(const uint8_t depth) {
  return (std::min)(static_cast<size_t>(depth), kTriggerDepthBuckets - 1);
}

bool IsSummonEvent(const CombatEvent &event) {
  if (event.type == CombatEventType::OnSummon ||
      event.type == CombatEventType::OnMinionDeath ||
      event.type == CombatEventType::OnMinionHit) {
    return true;
  }
  return event.minion != entt::null || event.summon_owner != entt::null ||
         event.summon_entity != entt::null;
}

void PushDamageDurationUs(TelemetryState &state, const double duration_us) {
  if (duration_us < 0.0) {
    return;
  }
  const double clamped = std::clamp(duration_us, 0.0, 10'000'000.0);
  const uint32_t quantized = static_cast<uint32_t>(clamped);
  const uint32_t idx =
      state.damage_write_index.fetch_add(1, std::memory_order_relaxed);
  state.damage_window_us[idx % kDamageWindowSize].store(quantized,
                                                         std::memory_order_relaxed);

  uint32_t previous = state.damage_count.load(std::memory_order_relaxed);
  while (previous < kDamageWindowSize &&
         !state.damage_count.compare_exchange_weak(
             previous, previous + 1, std::memory_order_relaxed)) {
  }
}

DamageWindowStats ReadDamageWindowStats(const TelemetryState &state) {
  DamageWindowStats stats;
  const uint32_t count = state.damage_count.load(std::memory_order_relaxed);
  if (count == 0) {
    return stats;
  }

  const uint32_t sample_count = (std::min)(count, static_cast<uint32_t>(kDamageWindowSize));
  std::vector<uint32_t> samples;
  samples.reserve(sample_count);

  if (sample_count < kDamageWindowSize) {
    for (uint32_t i = 0; i < sample_count; ++i) {
      samples.push_back(state.damage_window_us[i].load(std::memory_order_relaxed));
    }
  } else {
    for (const auto &value : state.damage_window_us) {
      samples.push_back(value.load(std::memory_order_relaxed));
    }
  }

  std::sort(samples.begin(), samples.end());
  const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  const size_t idx95 =
      static_cast<size_t>((sample_count - 1) * 0.95);
  const size_t idx99 =
      static_cast<size_t>((sample_count - 1) * 0.99);

  stats.sample_count = sample_count;
  stats.avg_us = sum / static_cast<double>(sample_count);
  stats.p95_us = static_cast<double>(samples[idx95]);
  stats.p99_us = static_cast<double>(samples[idx99]);
  return stats;
}

void RollFrameCounters(TelemetryState &state) {
  state.event_last_frame_total =
      state.event_frame_total.exchange(0, std::memory_order_relaxed);
  for (size_t i = 0; i < kEventTypeCount; ++i) {
    state.event_last_frame_counts[i] =
        state.event_frame_counts[i].exchange(0, std::memory_order_relaxed);
  }
  state.summon_entity_count_last_frame =
      state.summon_entity_count_frame.exchange(0, std::memory_order_relaxed);
}

std::string BuildTopEventsSummary(
    const std::array<uint32_t, kEventTypeCount> &counts) {
  std::vector<std::pair<uint32_t, CombatEventType>> non_zero;
  non_zero.reserve(kEventTypeCount);
  for (size_t i = 0; i < kEventTypeCount; ++i) {
    if (counts[i] > 0) {
      non_zero.emplace_back(counts[i], static_cast<CombatEventType>(i));
    }
  }
  if (non_zero.empty()) {
    return "none";
  }

  std::sort(non_zero.begin(), non_zero.end(),
            [](const auto &lhs, const auto &rhs) { return lhs.first > rhs.first; });

  std::ostringstream oss;
  const size_t limit = (std::min)(static_cast<size_t>(3), non_zero.size());
  for (size_t i = 0; i < limit; ++i) {
    if (i != 0) {
      oss << "|";
    }
    oss << EventTypeName(non_zero[i].second) << ":" << non_zero[i].first;
  }
  return oss.str();
}

std::string BuildDepthSummary(
    const std::array<uint64_t, kTriggerDepthBuckets> &values) {
  std::ostringstream oss;
  oss << "[d0=" << values[0] << ",d1=" << values[1] << ",d2=" << values[2]
      << ",d3+=" << values[3] << "]";
  return oss.str();
}

void EmitSummary(TelemetryState &state, const float interval_seconds) {
  const DamageWindowStats damage_stats = ReadDamageWindowStats(state);

  const uint64_t stats_calls =
      state.stats_calls.exchange(0, std::memory_order_relaxed);
  const uint64_t stats_cache_hits =
      state.stats_cache_hits.exchange(0, std::memory_order_relaxed);
  const uint64_t stats_read_wait_ns =
      state.stats_read_wait_ns.exchange(0, std::memory_order_relaxed);
  const uint64_t stats_write_wait_ns =
      state.stats_write_wait_ns.exchange(0, std::memory_order_relaxed);

  const uint64_t trigger_attempts =
      state.trigger_attempts.exchange(0, std::memory_order_relaxed);
  const uint64_t trigger_blocked =
      state.trigger_blocked.exchange(0, std::memory_order_relaxed);
  std::array<uint64_t, kTriggerDepthBuckets> trigger_attempt_depth{};
  std::array<uint64_t, kTriggerDepthBuckets> trigger_dispatch_depth{};
  for (size_t i = 0; i < kTriggerDepthBuckets; ++i) {
    trigger_attempt_depth[i] =
        state.trigger_attempt_depth[i].exchange(0, std::memory_order_relaxed);
    trigger_dispatch_depth[i] =
        state.trigger_dispatch_depth[i].exchange(0, std::memory_order_relaxed);
  }
  state.trigger_dispatched.exchange(0, std::memory_order_relaxed);

  const uint64_t summon_switches =
      state.summon_target_switches.exchange(0, std::memory_order_relaxed);
  const uint64_t summon_budget_requests =
      state.summon_budget_requests.exchange(0, std::memory_order_relaxed);
  const uint64_t summon_budget_granted =
      state.summon_budget_granted.exchange(0, std::memory_order_relaxed);
  const uint64_t summon_events =
      state.summon_events.exchange(0, std::memory_order_relaxed);

  const double stats_hit_rate =
      stats_calls > 0
          ? static_cast<double>(stats_cache_hits) /
                static_cast<double>(stats_calls) * 100.0
          : 0.0;
  const double lock_wait_us_per_call =
      stats_calls > 0
          ? static_cast<double>(stats_read_wait_ns + stats_write_wait_ns) /
                static_cast<double>(stats_calls) / 1000.0
          : 0.0;

  const double safe_interval = (std::max)(0.001f, interval_seconds);
  const double trigger_calls_per_sec =
      static_cast<double>(trigger_attempts) / static_cast<double>(safe_interval);
  const double trigger_interception_rate =
      trigger_attempts > 0
          ? static_cast<double>(trigger_blocked) /
                static_cast<double>(trigger_attempts) * 100.0
          : 0.0;

  const double summon_switches_per_sec =
      static_cast<double>(summon_switches) / static_cast<double>(safe_interval);
  const double summon_events_per_sec =
      static_cast<double>(summon_events) / static_cast<double>(safe_interval);
  const double summon_budget_hit_rate =
      summon_budget_requests > 0
          ? static_cast<double>(summon_budget_granted) /
                static_cast<double>(summon_budget_requests) * 100.0
          : 100.0;

  const std::string top_events = BuildTopEventsSummary(state.event_last_frame_counts);
  const std::string trigger_attempt_depth_summary =
      BuildDepthSummary(trigger_attempt_depth);
  const std::string trigger_dispatch_depth_summary =
      BuildDepthSummary(trigger_dispatch_depth);

  LOG_INFO(
      "[CombatTelemetry] dp_us(avg/p95/p99)={:.2f}/{:.2f}/{:.2f} samples={} "
      "stats(calls={} hit={:.1f}% lock_wait_us/call={:.3f}) "
      "events(frame_total={} top={}) "
      "trigger(calls/s={:.1f} intercept={:.1f}% depth_attempt={} "
      "depth_dispatch={}) "
      "summon(active={} switch/s={:.1f} events/s={:.1f} budget_hit={:.1f}%)",
      damage_stats.avg_us, damage_stats.p95_us, damage_stats.p99_us,
      damage_stats.sample_count, stats_calls, stats_hit_rate, lock_wait_us_per_call,
      state.event_last_frame_total, top_events, trigger_calls_per_sec,
      trigger_interception_rate, trigger_attempt_depth_summary,
      trigger_dispatch_depth_summary, state.summon_entity_count_last_frame,
      summon_switches_per_sec, summon_events_per_sec, summon_budget_hit_rate);
}

#endif // COMBAT_TELEMETRY_ENABLED

} // namespace

CombatTelemetry &CombatTelemetry::Get() {
  static CombatTelemetry instance;
  return instance;
}

CombatTelemetry::CombatTelemetry() {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  state.runtime_enabled.store(
      ParseBool(std::getenv("NMD_COMBAT_TELEMETRY"), true),
      std::memory_order_relaxed);
  state.output_enabled.store(
      ParseBool(std::getenv("NMD_COMBAT_TELEMETRY_OUTPUT"), true),
      std::memory_order_relaxed);
  state.output_interval_ms.store(
      ParseIntervalMs(std::getenv("NMD_COMBAT_TELEMETRY_INTERVAL_SEC"), 1000),
      std::memory_order_relaxed);
#endif
}

void CombatTelemetry::BeginFrame(float dt) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  std::lock_guard<std::mutex> lock(state.frame_mutex);

  RollFrameCounters(state);

  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    state.elapsed_since_output = 0.0f;
    return;
  }

  state.elapsed_since_output += (std::max)(0.0f, dt);
  const bool output_enabled =
      state.output_enabled.load(std::memory_order_relaxed);
  const float output_interval =
      static_cast<float>(state.output_interval_ms.load(std::memory_order_relaxed)) /
      1000.0f;
  if (output_enabled && state.elapsed_since_output >= output_interval) {
    EmitSummary(state, state.elapsed_since_output);
    state.elapsed_since_output = 0.0f;
  }
#else
  (void)dt;
#endif
}

void CombatTelemetry::SetRuntimeEnabled(bool enabled) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  GetState().runtime_enabled.store(enabled, std::memory_order_relaxed);
#else
  (void)enabled;
#endif
}

bool CombatTelemetry::IsRuntimeEnabled() const noexcept {
#if COMBAT_TELEMETRY_ENABLED
  return GetState().runtime_enabled.load(std::memory_order_relaxed);
#else
  return false;
#endif
}

void CombatTelemetry::SetOutputEnabled(bool enabled) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  GetState().output_enabled.store(enabled, std::memory_order_relaxed);
#else
  (void)enabled;
#endif
}

bool CombatTelemetry::IsOutputEnabled() const noexcept {
#if COMBAT_TELEMETRY_ENABLED
  return GetState().output_enabled.load(std::memory_order_relaxed);
#else
  return false;
#endif
}

void CombatTelemetry::SetOutputIntervalSeconds(float seconds) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  const float clamped =
      std::clamp(seconds, kMinOutputIntervalSeconds, kMaxOutputIntervalSeconds);
  GetState().output_interval_ms.store(static_cast<uint32_t>(clamped * 1000.0f),
                                      std::memory_order_relaxed);
#else
  (void)seconds;
#endif
}

void CombatTelemetry::RecordDamagePipelineDurationUs(double duration_us) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }
  PushDamageDurationUs(state, duration_us);
#else
  (void)duration_us;
#endif
}

void CombatTelemetry::RecordStatsQuery(bool cache_hit, uint64_t read_wait_ns,
                                       uint64_t write_wait_ns) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }
  state.stats_calls.fetch_add(1, std::memory_order_relaxed);
  if (cache_hit) {
    state.stats_cache_hits.fetch_add(1, std::memory_order_relaxed);
  }
  state.stats_read_wait_ns.fetch_add(read_wait_ns, std::memory_order_relaxed);
  state.stats_write_wait_ns.fetch_add(write_wait_ns, std::memory_order_relaxed);
#else
  (void)cache_hit;
  (void)read_wait_ns;
  (void)write_wait_ns;
#endif
}

void CombatTelemetry::RecordCombatEvent(const CombatEvent &event) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }

  state.event_frame_total.fetch_add(1, std::memory_order_relaxed);
  const size_t index = static_cast<size_t>(event.type);
  if (index < kEventTypeCount) {
    state.event_frame_counts[index].fetch_add(1, std::memory_order_relaxed);
  }
  if (IsSummonEvent(event)) {
    state.summon_events.fetch_add(1, std::memory_order_relaxed);
  }
#else
  (void)event;
#endif
}

void CombatTelemetry::RecordTriggerAttempt(uint8_t depth) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }
  state.trigger_attempts.fetch_add(1, std::memory_order_relaxed);
  state.trigger_attempt_depth[ClampDepthBucket(depth)].fetch_add(
      1, std::memory_order_relaxed);
#else
  (void)depth;
#endif
}

void CombatTelemetry::RecordTriggerBlocked(uint8_t depth) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  (void)depth;
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }
  state.trigger_blocked.fetch_add(1, std::memory_order_relaxed);
#else
  (void)depth;
#endif
}

void CombatTelemetry::RecordTriggerDispatched(uint8_t depth) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }
  state.trigger_dispatched.fetch_add(1, std::memory_order_relaxed);
  state.trigger_dispatch_depth[ClampDepthBucket(depth)].fetch_add(
      1, std::memory_order_relaxed);
#else
  (void)depth;
#endif
}

void CombatTelemetry::RecordSummonEntityCount(uint32_t active_count) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }
  state.summon_entity_count_frame.store(active_count, std::memory_order_relaxed);
#else
  (void)active_count;
#endif
}

void CombatTelemetry::RecordSummonTargetSwitch() noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }
  state.summon_target_switches.fetch_add(1, std::memory_order_relaxed);
#endif
}

void CombatTelemetry::RecordSummonBudgetRequest(bool granted) noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }
  state.summon_budget_requests.fetch_add(1, std::memory_order_relaxed);
  if (granted) {
    state.summon_budget_granted.fetch_add(1, std::memory_order_relaxed);
  }
#else
  (void)granted;
#endif
}

void CombatTelemetry::RecordSummonEvent() noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  if (!state.runtime_enabled.load(std::memory_order_relaxed)) {
    return;
  }
  state.summon_events.fetch_add(1, std::memory_order_relaxed);
#endif
}

void CombatTelemetry::ResetForTests() noexcept {
#if COMBAT_TELEMETRY_ENABLED
  auto &state = GetState();
  std::lock_guard<std::mutex> lock(state.frame_mutex);

  state.runtime_enabled.store(true, std::memory_order_relaxed);
  state.output_enabled.store(false, std::memory_order_relaxed);
  state.output_interval_ms.store(1000, std::memory_order_relaxed);

  state.damage_write_index.store(0, std::memory_order_relaxed);
  state.damage_count.store(0, std::memory_order_relaxed);
  for (auto &sample : state.damage_window_us) {
    sample.store(0, std::memory_order_relaxed);
  }

  state.stats_calls.store(0, std::memory_order_relaxed);
  state.stats_cache_hits.store(0, std::memory_order_relaxed);
  state.stats_read_wait_ns.store(0, std::memory_order_relaxed);
  state.stats_write_wait_ns.store(0, std::memory_order_relaxed);

  state.event_frame_total.store(0, std::memory_order_relaxed);
  for (auto &count : state.event_frame_counts) {
    count.store(0, std::memory_order_relaxed);
  }
  state.event_last_frame_total = 0;
  state.event_last_frame_counts.fill(0);

  state.trigger_attempts.store(0, std::memory_order_relaxed);
  state.trigger_blocked.store(0, std::memory_order_relaxed);
  state.trigger_dispatched.store(0, std::memory_order_relaxed);
  for (auto &bucket : state.trigger_attempt_depth) {
    bucket.store(0, std::memory_order_relaxed);
  }
  for (auto &bucket : state.trigger_dispatch_depth) {
    bucket.store(0, std::memory_order_relaxed);
  }

  state.summon_entity_count_frame.store(0, std::memory_order_relaxed);
  state.summon_entity_count_last_frame = 0;
  state.summon_target_switches.store(0, std::memory_order_relaxed);
  state.summon_budget_requests.store(0, std::memory_order_relaxed);
  state.summon_budget_granted.store(0, std::memory_order_relaxed);
  state.summon_events.store(0, std::memory_order_relaxed);

  state.elapsed_since_output = 0.0f;
#endif
}

} // namespace NoMoreDay
