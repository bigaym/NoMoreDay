#pragma once

#include <algorithm>
#include <chrono>
#include <numeric>
#include <vector>

namespace NoMoreDay::tests {

struct BenchmarkStats {
  double min_ms;
  double max_ms;
  double mean_ms;
  double median_ms;
  double p99_ms;
};

inline BenchmarkStats CalculateStats(const std::vector<double> &samples) {
  if (samples.empty()) {
    return {0, 0, 0, 0, 0};
  }

  std::vector<double> sorted = samples;
  std::sort(sorted.begin(), sorted.end());

  const double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
  const double mean = sum / static_cast<double>(sorted.size());

  const size_t medianIdx = sorted.size() / 2;
  const double median = sorted[medianIdx];

  size_t idx99 = static_cast<size_t>(sorted.size() * 0.99);
  if (idx99 >= sorted.size()) {
    idx99 = sorted.size() - 1;
  }

  return {sorted.front(), sorted.back(), mean, median, sorted[idx99]};
}

class ScopedTimer {
public:
  explicit ScopedTimer(std::vector<double> &target) : m_target(target) {
    m_start = std::chrono::high_resolution_clock::now();
  }

  ~ScopedTimer() {
    const auto end = std::chrono::high_resolution_clock::now();
    m_target.push_back(
        std::chrono::duration<double, std::milli>(end - m_start).count());
  }

private:
  std::vector<double> &m_target;
  std::chrono::high_resolution_clock::time_point m_start;
};

#define LOG_BENCHMARK(name, stats, target)                                      \
  LOG_WARN("{}: Mean={:.3f}ms, P99={:.3f}ms (Target: {})", name,                \
           (stats).mean_ms, (stats).p99_ms, target)

} // namespace NoMoreDay::tests
