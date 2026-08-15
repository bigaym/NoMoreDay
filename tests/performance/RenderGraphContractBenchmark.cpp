#include "BenchmarkUtils.hpp"
#include "TestCommon.hpp"
#include "doctest.h"

#include "engine/render/graph/RenderContext.hpp"
#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/graph/RenderPass.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

namespace {

struct PerfStats {
  double meanMs = 0.0;
  double p95Ms = 0.0;
  double p99Ms = 0.0;
};

volatile uint32_t g_benchmarkSink = 0;
constexpr uint32_t kStressBenchmarkSeed = 0x4E4D4453; // "NMDS"

PerfStats ComputePerfStats(std::vector<double> samples) {
  if (samples.empty()) {
    return {};
  }
  std::sort(samples.begin(), samples.end());
  const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  const double mean = sum / static_cast<double>(samples.size());

  size_t idx95 = static_cast<size_t>(samples.size() * 0.95);
  if (idx95 >= samples.size()) {
    idx95 = samples.size() - 1;
  }
  size_t idx99 = static_cast<size_t>(samples.size() * 0.99);
  if (idx99 >= samples.size()) {
    idx99 = samples.size() - 1;
  }

  PerfStats stats = {};
  stats.meanMs = mean;
  stats.p95Ms = samples[idx95];
  stats.p99Ms = samples[idx99];
  return stats;
}

class BenchmarkPass final : public NoMoreDay::render::graph::RenderPass {
public:
  using SetupFn =
      std::function<void(NoMoreDay::render::graph::RenderGraphBuilder &)>;

  explicit BenchmarkPass(std::string name,
                         NoMoreDay::render::graph::RenderPassType type,
                         SetupFn setupFn, int workloadIterations)
      : m_name(std::move(name)), m_type(type), m_setupFn(std::move(setupFn)),
        m_workloadIterations(workloadIterations) {}

  void Setup(NoMoreDay::render::graph::RenderGraphBuilder &builder) override {
    if (m_setupFn) {
      m_setupFn(builder);
    }
  }

  void Execute(NoMoreDay::render::graph::RenderContext &) override {
    // Keep a small deterministic CPU workload so P95 ratio stays meaningful.
    uint32_t value = g_benchmarkSink;
    for (int i = 0; i < m_workloadIterations; ++i) {
      value = value * 1664525u + 1013904223u + static_cast<uint32_t>(i);
    }
    g_benchmarkSink = value;
  }

  const char *GetName() const override { return m_name.c_str(); }

  NoMoreDay::render::graph::RenderPassType Type() const override {
    return m_type;
  }

private:
  std::string m_name;
  NoMoreDay::render::graph::RenderPassType m_type;
  SetupFn m_setupFn;
  int m_workloadIterations = 0;
};

PerfStats RunGraphFrameBenchmark(bool validationEnabled, int frames,
                                 uint32_t seed) {
  using namespace NoMoreDay::render::graph;
  constexpr int kPassWorkloadIterations = 12288;
  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(frames));
  g_benchmarkSink = seed;

  RenderGraph::SetValidationEnabled(validationEnabled);
  for (int i = 0; i < frames; ++i) {
    RenderGraph graph;
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "ScenePass", RenderPassType::Scene, [](RenderGraphBuilder &builder) {
          builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
          builder.Write(RenderResourceTag::SceneDepth, RenderOwnerTag::Scene);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "LightingPass", RenderPassType::Lighting, [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor,
                       RenderOwnerTag::Lighting);
          builder.Write(RenderResourceTag::SceneHdrColor,
                        RenderOwnerTag::Lighting);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "VolumetricPass", RenderPassType::Volumetric, [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor,
                       RenderOwnerTag::Volumetric);
          builder.Write(RenderResourceTag::SceneHdrColor,
                        RenderOwnerTag::Volumetric);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "VFXPass", RenderPassType::VFX, [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX);
          builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "UIWorldPass", RenderPassType::UIWorld, [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::UIWorld);
          builder.Write(RenderResourceTag::SceneHdrColor,
                        RenderOwnerTag::UIWorld);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "PostProcessPass", RenderPassType::PostProcess, [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor,
                       RenderOwnerTag::PostProcess);
          builder.Write(RenderResourceTag::PostProcessLdrColor,
                        RenderOwnerTag::PostProcess);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "DistortionPass", RenderPassType::Distortion, [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::PostProcessLdrColor,
                       RenderOwnerTag::Distortion);
          builder.Write(RenderResourceTag::DistortionLdrColor,
                        RenderOwnerTag::Distortion);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "CompositePass", RenderPassType::Composite, [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::DistortionLdrColor,
                       RenderOwnerTag::Distortion);
          builder.Write(RenderResourceTag::FinalOutputColor,
                        RenderOwnerTag::Composite);
        }, kPassWorkloadIterations));

    NoMoreDay::render::graph::RenderContext context = {};
    const auto start = std::chrono::high_resolution_clock::now();
    graph.Build();
    graph.Execute(context);
    const auto end = std::chrono::high_resolution_clock::now();
    samples.push_back(
        std::chrono::duration<double, std::milli>(end - start).count());
  }

  return ComputePerfStats(std::move(samples));
}

} // namespace

TEST_CASE(
    "[Performance] bench_rendering_system - RenderGraph Contract Validation Guard") {
  using NoMoreDay::render::graph::RenderGraph;

  constexpr int kWarmupFrames = 120;
  constexpr int kMeasureFrames = 800;
  CHECK(kWarmupFrames > 0);
  CHECK(kMeasureFrames > kWarmupFrames);

  const bool previousValidationState = RenderGraph::IsValidationEnabled();
  std::cout << "RELEASE_GATE_CONTEXT stress_seed=" << kStressBenchmarkSeed
            << " warmup_frames=" << kWarmupFrames
            << " measure_frames=" << kMeasureFrames << "\n";

  // Warmup both paths to reduce one-time driver and allocator noise.
  (void)RunGraphFrameBenchmark(false, kWarmupFrames, kStressBenchmarkSeed + 1u);
  (void)RunGraphFrameBenchmark(true, kWarmupFrames, kStressBenchmarkSeed + 2u);

  const PerfStats baseline =
      RunGraphFrameBenchmark(false, kMeasureFrames, kStressBenchmarkSeed);
  const PerfStats withValidation =
      RunGraphFrameBenchmark(true, kMeasureFrames, kStressBenchmarkSeed);
  RenderGraph::SetValidationEnabled(previousValidationState);
  CHECK(RenderGraph::IsValidationEnabled() == previousValidationState);

  const double overheadP95Ms =
      std::max(0.0, withValidation.p95Ms - baseline.p95Ms);
  const double baselineP95Floor = std::max(0.000001, baseline.p95Ms);
  const double p95RegressionRatio = withValidation.p95Ms / baselineP95Floor;
  constexpr double kRatioGateBaselineFloorMs = 0.20;
  const bool ratioGateActive = baseline.p95Ms >= kRatioGateBaselineFloorMs;

  LOG_WARN(
      "bench_rendering_system: baseline(mean={:.4f},p95={:.4f},p99={:.4f}) "
      "validation(mean={:.4f},p95={:.4f},p99={:.4f}) overheadP95={:.4f}ms "
      "p95_ratio={:.3f} ratioGateActive={}",
      baseline.meanMs, baseline.p95Ms, baseline.p99Ms, withValidation.meanMs,
      withValidation.p95Ms, withValidation.p99Ms, overheadP95Ms, p95RegressionRatio,
      ratioGateActive ? "true" : "false");
  const double stressFps = 1000.0 / std::max(0.0001, withValidation.meanMs);
  std::cout << "RELEASE_GATE_CONTEXT stress_ratio_gate_active="
            << (ratioGateActive ? 1 : 0)
            << " ratio_gate_baseline_floor_ms=" << kRatioGateBaselineFloorMs
            << "\n";
  std::cout << "RELEASE_GATE_METRIC stress_144_fps=" << stressFps << "\n";

  CHECK(overheadP95Ms <= 0.03);
  if (ratioGateActive) {
    CHECK(p95RegressionRatio <= 1.05);
  }
}
