#pragma once

#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/GPUUtils.hpp"
#include "GLFW/glfw3.h"
#include "rlgl.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace NoMoreDay::render::validation {


constexpr uint32_t kGlDebugOutput = 0x92E0;
constexpr uint32_t kGlDebugTypeError = 0x824C;
constexpr uint32_t kGlDebugSeverityHigh = 0x9146;
constexpr uint32_t kGlDebugCallbackFunction = 0x8244;
constexpr uint32_t kGlDebugCallbackUserParam = 0x8245;
constexpr size_t kMaxGlDiagnostics = 256;

using GlDebugCallbackFn = void(APIENTRY *)(uint32_t source, uint32_t type, uint32_t id,
                                           uint32_t severity, int length,
                                           const char *message, const void *userParam);
using GlSetDebugMessageCallbackFn =
    void(APIENTRY *)(GlDebugCallbackFn callback, const void *userParam);
using GlGetPointervFn = void(APIENTRY *)(uint32_t pname, void **params);
using GlIsEnabledFn = uint8_t(APIENTRY *)(uint32_t cap);

// Single-threaded, lock-free diagnostic collector. The GL debug callback is
// invoked synchronously on the GL thread only, so no synchronization is needed;
// the installing thread id is asserted on every capture.
class GlDebugCollector {
public:
  void Record(uint32_t source, uint32_t type, uint32_t id, uint32_t severity,
              const std::string &message) {
    assert(std::this_thread::get_id() == m_installThreadId);
    if (m_count >= kMaxGlDiagnostics) {
      ++m_droppedCount;
      return;
    }
    GlDiagnosticRecord record;
    record.id = id;
    record.source = source;
    record.type = type;
    record.severity = severity;
    record.message = message;
    const auto now = std::chrono::steady_clock::now();
    record.elapsedMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count());
    m_records[m_count] = std::move(record);
    ++m_count;
  }

  std::array<GlDiagnosticRecord, kMaxGlDiagnostics> m_records{};
  size_t m_count{0};
  size_t m_droppedCount{0};
  std::thread::id m_installThreadId{};
  std::chrono::steady_clock::time_point m_start{std::chrono::steady_clock::now()};
};

inline void APIENTRY GlDebugMessageCallbackHandler(uint32_t source, uint32_t type, uint32_t id,
                                                    uint32_t severity, int length,
                                                    const char *message, const void *userParam) {
  // Runtime filter: only ERROR-type or HIGH-severity messages are collected to
  // prevent MEDIUM/LOW/NOTIFICATION flooding of the queue.
  if (type != kGlDebugTypeError && severity != kGlDebugSeverityHigh) {
    return;
  }
  auto *collector =
      static_cast<GlDebugCollector *>(const_cast<void *>(userParam));
  if (collector == nullptr) {
    return;
  }
  std::string messageText;
  if (message != nullptr) {
    if (length >= 0) {
      messageText.assign(message, static_cast<size_t>(length));
    } else {
      messageText = message;
    }
  }
  collector->Record(source, type, id, severity, messageText);
}

// RAII guard: installs the GL debug callback for the full gate lifecycle and
// restores the previous callback / GL_DEBUG_OUTPUT enable state on exit.
class GlDebugOutputGuard {
public:
  bool Install() {
    m_setCallback = reinterpret_cast<GlSetDebugMessageCallbackFn>(
        glfwGetProcAddress("glDebugMessageCallback"));
    m_getPointerv =
        reinterpret_cast<GlGetPointervFn>(glfwGetProcAddress("glGetPointerv"));
    m_isEnabled =
        reinterpret_cast<GlIsEnabledFn>(glfwGetProcAddress("glIsEnabled"));
    if (m_setCallback == nullptr || m_getPointerv == nullptr || m_isEnabled == nullptr) {
      return false;
    }

    void *prevCallback = nullptr;
    void *prevUserParam = nullptr;
    m_getPointerv(kGlDebugCallbackFunction, &prevCallback);
    m_getPointerv(kGlDebugCallbackUserParam, &prevUserParam);
    m_prevCallback = reinterpret_cast<GlDebugCallbackFn>(prevCallback);
    m_prevUserParam = prevUserParam;
    m_wasEnabled = (m_isEnabled(kGlDebugOutput) != 0);

    m_collector.m_installThreadId = std::this_thread::get_id();
    m_setCallback(&GlDebugMessageCallbackHandler, &m_collector);
    NoMoreDay::utils::GPUUtils::Enable(kGlDebugOutput);
    m_installed = (m_isEnabled(kGlDebugOutput) != 0);
    return m_installed;
  }

  ~GlDebugOutputGuard() {
    if (m_setCallback != nullptr) {
      m_setCallback(m_prevCallback, m_prevUserParam);
    }
    if (m_isEnabled != nullptr) {
      if (m_wasEnabled) {
        NoMoreDay::utils::GPUUtils::Enable(kGlDebugOutput);
      } else {
        NoMoreDay::utils::GPUUtils::Disable(kGlDebugOutput);
      }
    }
  }

  const GlDebugCollector &Collector() const { return m_collector; }

private:
  GlSetDebugMessageCallbackFn m_setCallback{nullptr};
  GlGetPointervFn m_getPointerv{nullptr};
  GlIsEnabledFn m_isEnabled{nullptr};
  GlDebugCallbackFn m_prevCallback{nullptr};
  const void *m_prevUserParam{nullptr};
  bool m_wasEnabled{false};
  bool m_installed{false};
  GlDebugCollector m_collector;
};

} // namespace NoMoreDay::render::validation
