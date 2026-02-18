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

  explicit BenchmarkPass(std::string name, SetupFn setupFn, int workloadIterations)
      : m_name(std::move(name)), m_setupFn(std::move(setupFn)),
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

private:
  std::string m_name;
  SetupFn m_setupFn;
  int m_workloadIterations = 0;
};

PerfStats RunGraphFrameBenchmark(bool validationEnabled, int frames) {
  using namespace NoMoreDay::render::graph;
  constexpr int kPassWorkloadIterations = 12288;
  std::vector<double> samples;
  samples.reserve(static_cast<size_t>(frames));

  RenderGraph::SetValidationEnabled(validationEnabled);
  for (int i = 0; i < frames; ++i) {
    RenderGraph graph;
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "ScenePass", [](RenderGraphBuilder &builder) {
          builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::Scene);
          builder.Write(RenderResourceTag::SceneDepth, RenderOwnerTag::Scene);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "LightingPass", [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor,
                       RenderOwnerTag::Lighting);
          builder.Write(RenderResourceTag::SceneHdrColor,
                        RenderOwnerTag::Lighting);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "VolumetricPass", [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor,
                       RenderOwnerTag::Volumetric);
          builder.Write(RenderResourceTag::SceneHdrColor,
                        RenderOwnerTag::Volumetric);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "VFXPass", [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX);
          builder.Write(RenderResourceTag::SceneHdrColor, RenderOwnerTag::VFX);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "UIWorldPass", [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor, RenderOwnerTag::UIWorld);
          builder.Write(RenderResourceTag::SceneHdrColor,
                        RenderOwnerTag::UIWorld);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "PostProcessPass", [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::SceneHdrColor,
                       RenderOwnerTag::PostProcess);
          builder.Write(RenderResourceTag::PostProcessLdrColor,
                        RenderOwnerTag::PostProcess);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "DistortionPass", [](RenderGraphBuilder &builder) {
          builder.Read(RenderResourceTag::PostProcessLdrColor,
                       RenderOwnerTag::Distortion);
          builder.Write(RenderResourceTag::DistortionLdrColor,
                        RenderOwnerTag::Distortion);
        }, kPassWorkloadIterations));
    graph.AddPass(std::make_shared<BenchmarkPass>(
        "CompositePass", [](RenderGraphBuilder &builder) {
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

  const bool previousValidationState = RenderGraph::IsValidationEnabled();

  // Warmup both paths to reduce one-time driver and allocator noise.
  (void)RunGraphFrameBenchmark(false, kWarmupFrames);
  (void)RunGraphFrameBenchmark(true, kWarmupFrames);

  const PerfStats baseline = RunGraphFrameBenchmark(false, kMeasureFrames);
  const PerfStats withValidation = RunGraphFrameBenchmark(true, kMeasureFrames);
  RenderGraph::SetValidationEnabled(previousValidationState);

  const double overheadP95Ms =
      std::max(0.0, withValidation.p95Ms - baseline.p95Ms);
  const double baselineP95Floor = std::max(0.000001, baseline.p95Ms);
  const double p95RegressionRatio = withValidation.p95Ms / baselineP95Floor;

  LOG_WARN(
      "bench_rendering_system: baseline(mean={:.4f},p95={:.4f},p99={:.4f}) "
      "validation(mean={:.4f},p95={:.4f},p99={:.4f}) overheadP95={:.4f}ms "
      "p95_ratio={:.3f}",
      baseline.meanMs, baseline.p95Ms, baseline.p99Ms, withValidation.meanMs,
      withValidation.p95Ms, withValidation.p99Ms, overheadP95Ms,
      p95RegressionRatio);
  const double stressFps = 1000.0 / std::max(0.0001, withValidation.p95Ms);
  std::cout << "RELEASE_GATE_METRIC stress_144_fps=" << stressFps << "\n";

  CHECK(overheadP95Ms <= 0.2);
  CHECK(p95RegressionRatio <= 1.05);
}
