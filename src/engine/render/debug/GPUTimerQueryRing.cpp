#include "engine/render/debug/GPUTimerQueryRing.hpp"

#include "core/logging/Logger.hpp"
#include "rlgl.h"
#include <GLFW/glfw3.h>

#include <chrono>
#include <algorithm>
#include <array>

namespace NoMoreDay::render::debug {

namespace {
static void *s_glGenQueries = nullptr;
static void *s_glDeleteQueries = nullptr;
static void *s_glBeginQuery = nullptr;
static void *s_glEndQuery = nullptr;
static void *s_glGetQueryObjectiv = nullptr;
static void *s_glGetQueryObjectui64v = nullptr;

constexpr uint32_t kGLTimeElapsed = 0x88BF;
constexpr uint32_t kGLQueryResultAvailable = 0x8867;
constexpr uint32_t kGLQueryResult = 0x8866;

double GetCurrentTimeMs() {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
} // namespace

GPUTimerQueryRing &GPUTimerQueryRing::Get() {
  static GPUTimerQueryRing instance;
  return instance;
}

void GPUTimerQueryRing::Initialize() {
  if (m_initialized) return;

  s_glGenQueries = (void *)glfwGetProcAddress("glGenQueries");
  s_glDeleteQueries = (void *)glfwGetProcAddress("glDeleteQueries");
  s_glBeginQuery = (void *)glfwGetProcAddress("glBeginQuery");
  s_glEndQuery = (void *)glfwGetProcAddress("glEndQuery");
  s_glGetQueryObjectiv = (void *)glfwGetProcAddress("glGetQueryObjectiv");
  s_glGetQueryObjectui64v = (void *)glfwGetProcAddress("glGetQueryObjectui64v");

  for (size_t i = 0; i < kRingDepth; ++i) {
    m_ring[i] = {};
  }
  m_latestValidResults.clear();
  m_latestFrameResult = {};
  m_latestFrameResult.state = QueryState::Pending;
  m_frameHistory = {};
  m_frameHistoryCount = 0;
  m_frameHistoryWriteIndex = 0;
  m_frameIndex = 0;
  m_currentRingIndex = 0;
  m_initialized = true;
}

void GPUTimerQueryRing::Shutdown() {
  m_gpuTimerOverrideActive = false;
  m_gpuTimerOverrideValue = false;
  if (!m_initialized) return;

  if (s_glDeleteQueries) {
    using FnType = void (APIENTRY *)(int, const uint32_t *);
    for (size_t i = 0; i < kRingDepth; ++i) {
      for (auto &[id, slot] : m_ring[i].slots) {
        if (slot.queryBegin > 0) {
          uint32_t q[2] = {slot.queryBegin, slot.queryEnd};
          reinterpret_cast<FnType>(s_glDeleteQueries)(2, q);
        }
      }
    }
  }

  for (size_t i = 0; i < kRingDepth; ++i) {
    m_ring[i] = {};
  }
  m_latestValidResults.clear();
  m_latestFrameResult = {};
  m_latestFrameResult.state = QueryState::Pending;
  m_frameHistory = {};
  m_frameHistoryCount = 0;
  m_frameHistoryWriteIndex = 0;
  m_initialized = false;
}

void GPUTimerQueryRing::BeginFrame() {
  if (!m_initialized) Initialize();

  PollReadyQueries();

  if (m_frameIndex == UINT64_MAX) {
    LOG_ERROR("GPUTimerQueryRing: frameIndex overflow at UINT64_MAX; frame skipped (fail-closed)");
    return;
  }

  m_frameIndex++;
  m_currentRingIndex = m_frameIndex % kRingDepth;
  auto &frameSlot = m_ring[m_currentRingIndex];
  frameSlot.frameIndex = m_frameIndex;
  frameSlot.isComplete = false;
  frameSlot.aggregatePublished = false;
  for (auto &[passId, slot] : frameSlot.slots) {
    (void)passId;
    slot.active = false;
    slot.touchedThisFrame = false;
    slot.resultReady = false;
    slot.resultValid = false;
    slot.gpuDurationMs = 0.0;
  }
}

void GPUTimerQueryRing::EndFrame() {
  if (!m_initialized) return;

  auto &frameSlot = m_ring[m_currentRingIndex];
  frameSlot.isComplete = true;
}

void GPUTimerQueryRing::DebugSetFrameIndex(uint64_t frameIndex) {
  m_frameIndex = frameIndex;
}

void GPUTimerQueryRing::DebugInjectPassResult(uint32_t passId,
                                              const GPUTimerResult &result) {
  m_latestValidResults[passId] = result;
}

void GPUTimerQueryRing::DebugSetGpuTimerSupported(bool supported) {
  m_gpuTimerOverrideActive = true;
  m_gpuTimerOverrideValue = supported;
}

void GPUTimerQueryRing::BeginPass(uint32_t passId) {
  if (!m_initialized) return;

  auto &frameSlot = m_ring[m_currentRingIndex];
  auto &slot = frameSlot.slots[passId];
  slot.passId = passId;
  slot.frameIndex = m_frameIndex;
  slot.active = true;
  slot.touchedThisFrame = true;
  slot.resultReady = false;
  slot.resultValid = false;
  slot.gpuDurationMs = 0.0;
  slot.cpuStartTimeMs = GetCurrentTimeMs();

  if (s_glGenQueries && s_glBeginQuery) {
    using FnGen = void (APIENTRY *)(int, uint32_t *);
    using FnBegin = void (APIENTRY *)(uint32_t, uint32_t);

    if (slot.queryBegin == 0) {
      uint32_t q[2] = {0, 0};
      reinterpret_cast<FnGen>(s_glGenQueries)(2, q);
      slot.queryBegin = q[0];
      slot.queryEnd = q[1];
    }
    if (slot.queryBegin > 0) {
      reinterpret_cast<FnBegin>(s_glBeginQuery)(kGLTimeElapsed, slot.queryBegin);
    }
  }
}

void GPUTimerQueryRing::EndPass(uint32_t passId) {
  if (!m_initialized) return;

  auto &frameSlot = m_ring[m_currentRingIndex];
  auto it = frameSlot.slots.find(passId);
  if (it != frameSlot.slots.end() && it->second.active) {
    it->second.cpuDurationMs = GetCurrentTimeMs() - it->second.cpuStartTimeMs;
    if (s_glEndQuery && it->second.queryBegin > 0) {
      using FnEnd = void (APIENTRY *)(uint32_t);
      reinterpret_cast<FnEnd>(s_glEndQuery)(kGLTimeElapsed);
    }
  }
}

void GPUTimerQueryRing::PollReadyQueries() {
  if (!m_initialized) return;

  for (size_t i = 0; i < kRingDepth; ++i) {
    auto &frameSlot = m_ring[i];
    if (frameSlot.frameIndex == 0) continue;

    for (auto &[passId, slot] : frameSlot.slots) {
      if (!slot.touchedThisFrame || slot.resultReady || !slot.active) continue;

      GPUTimerResult res = {};
      res.cpuTimeMs = slot.cpuDurationMs;
      res.frameIndex = slot.frameIndex;

      if (s_glGetQueryObjectiv && s_glGetQueryObjectui64v && slot.queryBegin > 0) {
        using FnGetIv = void (APIENTRY *)(uint32_t, uint32_t, int *);
        using FnGetUi64v = void (APIENTRY *)(uint32_t, uint32_t, uint64_t *);

        int available = 0;
        reinterpret_cast<FnGetIv>(s_glGetQueryObjectiv)(slot.queryBegin, kGLQueryResultAvailable, &available);
        if (available != 0) {
          uint64_t timeNs = 0;
          reinterpret_cast<FnGetUi64v>(s_glGetQueryObjectui64v)(slot.queryBegin, kGLQueryResult, &timeNs);
          res.gpuTimeMs = static_cast<double>(timeNs) / 1000000.0;
          res.state = QueryState::Valid;
          slot.gpuDurationMs = res.gpuTimeMs;
          slot.resultValid = true;
          if (passId == kFramePassId) {
            m_latestFrameResult = res;
          } else {
            m_latestValidResults[passId] = res;
          }
          slot.active = false;
          slot.resultReady = true;
        } else {
          res.state = QueryState::Pending;
        }
      } else {
        res.gpuTimeMs = slot.cpuDurationMs;
        res.state = QueryState::CpuFallback;
        slot.active = false;
        slot.resultReady = true;
      }
    }

    if (frameSlot.isComplete && !frameSlot.aggregatePublished) {
      bool hasGpuPass = false;
      bool allPassesReady = true;
      bool allPassesValid = true;
      double aggregateGpuMs = 0.0;
      for (const auto &[passId, slot] : frameSlot.slots) {
        if (!slot.touchedThisFrame || passId == kFramePassId) {
          continue;
        }
        hasGpuPass = true;
        allPassesReady = allPassesReady && slot.resultReady;
        allPassesValid = allPassesValid && slot.resultValid;
        aggregateGpuMs += slot.gpuDurationMs;
      }
      if (hasGpuPass && allPassesReady) {
        m_latestFrameResult = {};
        m_latestFrameResult.gpuTimeMs = aggregateGpuMs;
        m_latestFrameResult.state =
            allPassesValid ? QueryState::Valid : QueryState::CpuFallback;
        m_latestFrameResult.frameIndex = frameSlot.frameIndex;
        if (allPassesValid) {
          m_frameHistory[m_frameHistoryWriteIndex] = m_latestFrameResult;
          m_frameHistoryWriteIndex =
              (m_frameHistoryWriteIndex + 1) % m_frameHistory.size();
          m_frameHistoryCount =
              std::min(m_frameHistoryCount + 1, m_frameHistory.size());
        }
        frameSlot.aggregatePublished = true;
      }
    }
  }
}

GPUTimerResult GPUTimerQueryRing::GetPassResult(uint32_t passId) const {
  auto it = m_latestValidResults.find(passId);
  if (it != m_latestValidResults.end()) {
    return it->second;
  }
  GPUTimerResult res = {};
  res.state = QueryState::Pending;
  return res;
}

bool GPUTimerQueryRing::IsGpuTimerSupported() const {
  if (m_gpuTimerOverrideActive) {
    return m_gpuTimerOverrideValue;
  }
  return s_glGenQueries != nullptr && s_glDeleteQueries != nullptr &&
         s_glBeginQuery != nullptr && s_glEndQuery != nullptr &&
         s_glGetQueryObjectiv != nullptr && s_glGetQueryObjectui64v != nullptr;
}

bool GPUTimerQueryRing::IsGpuTimeValid(uint32_t passId) const {
  auto res = GetPassResult(passId);
  return res.state == QueryState::Valid;
}

double GPUTimerQueryRing::GetValidGpuTimeMs(uint32_t passId) const {
  auto res = GetPassResult(passId);
  if (res.state == QueryState::Valid) {
    return res.gpuTimeMs;
  }
  return -1.0;
}

double GPUTimerQueryRing::GetValidFrameP95Ms() const {
  if (m_frameHistoryCount == 0) {
    return -1.0;
  }

  std::array<double, 120> values = {};
  for (size_t i = 0; i < m_frameHistoryCount; ++i) {
    values[i] = m_frameHistory[i].gpuTimeMs;
  }
  std::sort(values.begin(), values.begin() +
                              static_cast<std::ptrdiff_t>(m_frameHistoryCount));
  const size_t index = std::min(
      m_frameHistoryCount - 1,
      (m_frameHistoryCount * 95u) / 100u);
  return values[index];
}

} // namespace NoMoreDay::render::debug
