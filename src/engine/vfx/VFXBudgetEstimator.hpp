#pragma once

#include "engine/vfx/VFXTypes.hpp"

#include <cstddef>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace NoMoreDay::vfx {

struct VFXBudgetReport {
  std::string sequenceName;
  int schemaVersion = 0;
  size_t eventCount = 0;
  float particleCost = 0.0f;
  float lightCost = 0.0f;
  float shadowCost = 0.0f;
  float materialCost = 0.0f;
  float totalCost = 0.0f;
  float warningThreshold = 0.0f;
  bool overBudget = false;
};

class VFXBudgetEstimator {
public:
  static constexpr float DEFAULT_WARNING_THRESHOLD = 1200.0f;

  [[nodiscard]] static VFXBudgetReport
  Analyze(const VFXSequenceAsset &sequence,
          float warningThreshold = DEFAULT_WARNING_THRESHOLD);
  [[nodiscard]] static nlohmann::json ToJson(const VFXBudgetReport &report);
  [[nodiscard]] static std::string BuildConsoleSummary(const VFXBudgetReport &report);
};

} // namespace NoMoreDay::vfx
