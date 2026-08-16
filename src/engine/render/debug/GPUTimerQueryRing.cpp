#include "engine/render/debug/GPUTimerQueryRing.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "rlgl.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>

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
        if (slot.queryBegin > 0 || slot.queryEnd > 0) {
          // RG-3: unregister before GL release (observer-only pairing).
          if (slot.queryBegin > 0) {
            NoMoreDay::render::resources::GPUResourceRegistry::Get()
                .UnregisterResource(slot.queryBegin,
                                    NoMoreDay::render::graph::ResourceKind::QueryRing);
            reinterpret_cast<FnType>(s_glDeleteQueries)(1, &slot.queryBegin);
            slot.queryBegin = 0;
          }
          if (slot.queryEnd > 0) {
            NoMoreDay::render::resources::GPUResourceRegistry::Get()
                .UnregisterResource(slot.queryEnd,
                                    NoMoreDay::render::graph::ResourceKind::QueryRing);
            reinterpret_cast<FnType>(s_glDeleteQueries)(1, &slot.queryEnd);
            slot.queryEnd = 0;
          }
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

SlotState GPUTimerQueryRing::DebugGetSlotState(size_t ringIndex, uint32_t passId) const {
  if (ringIndex >= kRingDepth) return SlotState::Free;
  auto it = m_ring[ringIndex].slots.find(passId);
  if (it != m_ring[ringIndex].slots.end()) {
    return it->second.state;
  }
  return SlotState::Free;
}

void GPUTimerQueryRing::DebugSetSlotState(size_t ringIndex, uint32_t passId, SlotState state) {
  if (ringIndex < kRingDepth) {
    m_ring[ringIndex].slots[passId].state = state;
  }
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

  // 1. Check if previous query on this slot is still busy or ready
  if (slot.state == SlotState::Pending || slot.state == SlotState::Ready) {
    int available = 0;
    if (s_glGetQueryObjectiv && slot.queryBegin > 0) {
      using FnGetIv = void (APIENTRY *)(uint32_t, uint32_t, int *);
      reinterpret_cast<FnGetIv>(s_glGetQueryObjectiv)(
          slot.queryBegin, kGLQueryResultAvailable, &available);
    }
    if (available != 0) {
      if (s_glGetQueryObjectui64v && slot.queryBegin > 0 && slot.state == SlotState::Ready) {
        using FnGetUi64v = void (APIENTRY *)(uint32_t, uint32_t, uint64_t *);
        uint64_t timeNs = 0;
        reinterpret_cast<FnGetUi64v>(s_glGetQueryObjectui64v)(slot.queryBegin, kGLQueryResult, &timeNs);
        GPUTimerResult res = {};
        res.gpuTimeMs = static_cast<double>(timeNs) / 1000000.0;
        res.cpuTimeMs = slot.cpuDurationMs;
        res.frameIndex = slot.frameIndex;
        res.state = QueryState::Valid;
        if (passId == kFramePassId) {
          m_latestFrameResult = res;
        } else {
          m_latestValidResults[passId] = res;
        }
      }
      slot.state = SlotState::Free;
    } else {
      // Previous query still executing on GPU! Drop sample, do not call glBeginQuery on busy query, do not block.
      slot.state = SlotState::Discarded;
    }
  } else if (slot.state == SlotState::Discarded) {
    // Recreate query object to avoid slot exhaustion
    if (s_glDeleteQueries && s_glGenQueries) {
      using FnDel = void (APIENTRY *)(int, const uint32_t *);
      using FnGen = void (APIENTRY *)(int, uint32_t *);
      if (slot.queryBegin > 0) {
        NoMoreDay::render::resources::GPUResourceRegistry::Get()
            .UnregisterResource(slot.queryBegin,
                                NoMoreDay::render::graph::ResourceKind::QueryRing);
        reinterpret_cast<FnDel>(s_glDeleteQueries)(1, &slot.queryBegin);
        slot.queryBegin = 0;
      }
      if (slot.queryEnd > 0) {
        NoMoreDay::render::resources::GPUResourceRegistry::Get()
            .UnregisterResource(slot.queryEnd,
                                NoMoreDay::render::graph::ResourceKind::QueryRing);
        reinterpret_cast<FnDel>(s_glDeleteQueries)(1, &slot.queryEnd);
        slot.queryEnd = 0;
      }
      uint32_t q[2] = {0, 0};
      reinterpret_cast<FnGen>(s_glGenQueries)(2, q);
      slot.queryBegin = q[0];
      slot.queryEnd = q[1];
      if (slot.queryBegin > 0) {
        NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
            slot.queryBegin, NoMoreDay::render::graph::ResourceKind::QueryRing,
            NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u,
            "GPUTimerQueryRing");
      }
      if (slot.queryEnd > 0) {
        NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
            slot.queryEnd, NoMoreDay::render::graph::ResourceKind::QueryRing,
            NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u,
            "GPUTimerQueryRing");
      }
    }
    slot.state = SlotState::Free;
  }

  // 2. If slot is Free, begin query
  if (slot.state == SlotState::Free) {
    if (s_glGenQueries && s_glBeginQuery) {
      using FnGen = void (APIENTRY *)(int, uint32_t *);
      using FnBegin = void (APIENTRY *)(uint32_t, uint32_t);

      if (slot.queryBegin == 0) {
        uint32_t q[2] = {0, 0};
        reinterpret_cast<FnGen>(s_glGenQueries)(2, q);
        slot.queryBegin = q[0];
        slot.queryEnd = q[1];
        // RG-3 (observer-only): register each successfully generated query;
        // Shutdown remains the sole GL releaser.
        if (slot.queryBegin > 0) {
          NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
              slot.queryBegin, NoMoreDay::render::graph::ResourceKind::QueryRing,
              NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u,
              "GPUTimerQueryRing");
        }
        if (slot.queryEnd > 0) {
          NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
              slot.queryEnd, NoMoreDay::render::graph::ResourceKind::QueryRing,
              NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u,
              "GPUTimerQueryRing");
        }
      }
      if (slot.queryBegin > 0) {
        reinterpret_cast<FnBegin>(s_glBeginQuery)(kGLTimeElapsed, slot.queryBegin);
        slot.state = SlotState::Pending;
      }
    }
  }
}

void GPUTimerQueryRing::EndPass(uint32_t passId) {
  if (!m_initialized) return;

  auto &frameSlot = m_ring[m_currentRingIndex];
  auto it = frameSlot.slots.find(passId);
  if (it != frameSlot.slots.end() && it->second.active) {
    it->second.cpuDurationMs = GetCurrentTimeMs() - it->second.cpuStartTimeMs;
    if (it->second.state == SlotState::Pending) {
      if (s_glEndQuery && it->second.queryBegin > 0) {
        using FnEnd = void (APIENTRY *)(uint32_t);
        reinterpret_cast<FnEnd>(s_glEndQuery)(kGLTimeElapsed);
      }
      it->second.state = SlotState::Ready;
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

        if (slot.state == SlotState::Ready) {
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
            slot.state = SlotState::Free;
          } else {
            res.state = QueryState::Pending;
          }
        } else if (slot.state == SlotState::Discarded) {
          res.state = QueryState::Unavailable;
          slot.active = false;
          slot.resultReady = true;
        }
      } else {
        res.gpuTimeMs = slot.cpuDurationMs;
        res.state = QueryState::CpuFallback;
        slot.active = false;
        slot.resultReady = true;
        slot.state = SlotState::Free;
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
