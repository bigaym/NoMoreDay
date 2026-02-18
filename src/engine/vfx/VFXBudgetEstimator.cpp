#include "engine/vfx/VFXBudgetEstimator.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include <nlohmann/json.hpp>

namespace NoMoreDay::vfx {
namespace {

float EstimateParticleCost(const ParticleEventParams &params) {
  const float count = static_cast<float>(std::max(1, params.count));
  const float life = std::max(0.05f, params.lifetime);
  const float speed = std::max(0.0f, params.speed + params.speedVariance * 0.5f);
  const float complexity = 1.0f + speed / 240.0f + std::max(0.0f, params.scale) * 0.2f;
  return count * life * complexity;
}

float EstimateLightCost(const LightEventParams &params) {
  const float radius = std::max(0.0f, params.radius);
  const float intensity = std::max(0.0f, params.intensity);
  const float duration = std::max(0.05f, params.duration);
  return radius * intensity * duration * 0.05f;
}

float EstimateShadowPulseCost(const ShadowPulseParams &params) {
  const float duration = std::max(0.05f, params.duration);
  return duration * (std::max(0.0f, params.softnessScale) +
                     std::max(0.0f, params.intensityScale)) *
         40.0f;
}

float EstimateMaterialPhaseShiftCost(const MaterialPhaseShiftParams &params) {
  const float duration = std::max(0.05f, params.duration);
  const float complexity = std::max(0.0f, params.roughnessScale) +
                           std::max(0.0f, params.specularScale) +
                           std::max(0.0f, params.emissiveScale);
  return duration * complexity * 24.0f;
}

} // namespace

VFXBudgetReport VFXBudgetEstimator::Analyze(const VFXSequenceAsset &sequence,
                                            const float warningThreshold) {
  VFXBudgetReport report = {};
  report.sequenceName = sequence.name;
  report.schemaVersion = sequence.version;
  report.eventCount = sequence.events.size();
  report.warningThreshold = warningThreshold;

  for (const VFXEvent &event : sequence.events) {
    switch (event.type) {
    case EventType::Particle:
      if (const auto *params = std::get_if<ParticleEventParams>(&event.params)) {
        report.particleCost += EstimateParticleCost(*params);
      }
      break;
    case EventType::Trail:
      if (const auto *params = std::get_if<TrailEventParams>(&event.params)) {
        report.particleCost +=
            std::max(0.05f, params->duration) *
            (std::max(0.1f, params->widthStart) + std::max(0.1f, params->widthEnd)) *
            5.0f;
      }
      break;
    case EventType::Light:
      if (const auto *params = std::get_if<LightEventParams>(&event.params)) {
        report.lightCost += EstimateLightCost(*params);
      }
      break;
    case EventType::Distortion:
      if (const auto *params = std::get_if<DistortionEventParams>(&event.params)) {
        report.shadowCost += std::max(0.0f, params->radius) *
                             std::max(0.0f, params->strength) *
                             std::max(0.05f, params->duration) * 0.08f;
      }
      break;
    case EventType::MaterialSwap:
      if (const auto *params = std::get_if<MaterialSwapParams>(&event.params)) {
        report.materialCost += std::max(0.05f, params->duration) * 12.0f;
      }
      break;
    case EventType::ShadowPulse:
      if (const auto *params = std::get_if<ShadowPulseParams>(&event.params)) {
        report.shadowCost += EstimateShadowPulseCost(*params);
      }
      break;
    case EventType::LightProfileBlend:
      if (const auto *params = std::get_if<LightProfileBlendParams>(&event.params)) {
        report.lightCost += std::max(0.05f, params->blendTime) * 20.0f;
      }
      break;
    case EventType::MaterialPhaseShift:
      if (const auto *params = std::get_if<MaterialPhaseShiftParams>(&event.params)) {
        report.materialCost += EstimateMaterialPhaseShiftCost(*params);
      }
      break;
    case EventType::Shake:
    case EventType::Sound:
    case EventType::Count:
      break;
    }
  }

  report.totalCost =
      report.particleCost + report.lightCost + report.shadowCost + report.materialCost;
  report.overBudget = report.totalCost > report.warningThreshold;
  return report;
}

nlohmann::json VFXBudgetEstimator::ToJson(const VFXBudgetReport &report) {
  return {
      {"sequence", report.sequenceName},
      {"schemaVersion", report.schemaVersion},
      {"eventCount", report.eventCount},
      {"cost",
       {{"particle", report.particleCost},
        {"light", report.lightCost},
        {"shadow", report.shadowCost},
        {"material", report.materialCost},
        {"total", report.totalCost}}},
      {"warningThreshold", report.warningThreshold},
      {"overBudget", report.overBudget},
  };
}

std::string VFXBudgetEstimator::BuildConsoleSummary(const VFXBudgetReport &report) {
  std::ostringstream ss;
  ss << "VFXBudget sequence=" << report.sequenceName << " schema=" << report.schemaVersion
     << " events=" << report.eventCount << " total=" << report.totalCost
     << " threshold=" << report.warningThreshold << " overBudget="
     << (report.overBudget ? "true" : "false") << " [particle=" << report.particleCost
     << ", light=" << report.lightCost << ", shadow=" << report.shadowCost
     << ", material=" << report.materialCost << "]";
  return ss.str();
}

} // namespace NoMoreDay::vfx
