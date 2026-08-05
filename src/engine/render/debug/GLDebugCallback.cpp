#include "engine/render/debug/GLDebugCallback.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"

#include <GLFW/glfw3.h>

#include <string>

namespace NoMoreDay::render::debug {

namespace {

constexpr uint32_t kGlDebugOutput = 0x92E0;
constexpr uint32_t kGlDebugTypeError = 0x824C;
constexpr uint32_t kGlDebugSeverityHigh = 0x9146;
constexpr uint32_t kGlDebugCallbackFunction = 0x8244;
constexpr uint32_t kGlDebugCallbackUserParam = 0x8245;

using GlDebugCallbackFn = void(APIENTRY *)(uint32_t source, uint32_t type,
                                           uint32_t id, uint32_t severity,
                                           int length, const char *message,
                                           const void *userParam);
using GlSetDebugMessageCallbackFn =
    void(APIENTRY *)(GlDebugCallbackFn callback, const void *userParam);
using GlGetPointervFn = void(APIENTRY *)(uint32_t pname, void **params);
using GlIsEnabledFn = uint8_t(APIENTRY *)(uint32_t cap);

} // namespace

GLDebugCallback &GLDebugCallback::Get() {
  static GLDebugCallback instance;
  return instance;
}

void GLDebugCallback::OnDebugMessage(uint32_t source, uint32_t type,
                                     uint32_t id, uint32_t severity, int length,
                                     const char *message, const void *userParam) {
  // Severity filter: only ERROR-type or HIGH-severity messages are reported,
  // preventing MEDIUM/LOW/NOTIFICATION flooding of the log (mirrors the gate's
  // collector filter). The callback runs synchronously on the GL thread only.
  if (type != kGlDebugTypeError && severity != kGlDebugSeverityHigh) {
    return;
  }
  std::string text;
  if (message != nullptr) {
    if (length >= 0) {
      text.assign(message, static_cast<size_t>(length));
    } else {
      text = message;
    }
  }
  LOG_ERROR("GLDebugCallback: source=0x{:X} type=0x{:X} id={} severity=0x{:X}: "
            "{}",
            source, type, id, severity, text);

  auto *self = static_cast<GLDebugCallback *>(const_cast<void *>(userParam));
  if (self != nullptr) {
    self->m_reportedCount.fetch_add(1, std::memory_order_relaxed);
  }
}

bool GLDebugCallback::Install() {
  if (m_installed) {
    return true;
  }

  const auto setCallback = reinterpret_cast<GlSetDebugMessageCallbackFn>(
      glfwGetProcAddress("glDebugMessageCallback"));
  const auto getPointerv = reinterpret_cast<GlGetPointervFn>(
      glfwGetProcAddress("glGetPointerv"));
  const auto isEnabled =
      reinterpret_cast<GlIsEnabledFn>(glfwGetProcAddress("glIsEnabled"));
  if (setCallback == nullptr || getPointerv == nullptr ||
      isEnabled == nullptr) {
    LOG_WARN("GLDebugCallback: glDebugMessageCallback unavailable; production "
             "GL diagnostics disabled");
    return false;
  }

  // Save the previous callback and GL_DEBUG_OUTPUT enable state so Shutdown
  // (and the gate's own scoped install) can restore them.
  void *prevCallback = nullptr;
  void *prevUserParam = nullptr;
  getPointerv(kGlDebugCallbackFunction, &prevCallback);
  getPointerv(kGlDebugCallbackUserParam, &prevUserParam);
  m_prevCallback = prevCallback;
  m_prevUserParam = prevUserParam;
  m_wasEnabled = (isEnabled(kGlDebugOutput) != 0);

  setCallback(reinterpret_cast<GlDebugCallbackFn>(&GLDebugCallback::OnDebugMessage),
              this);
  NoMoreDay::utils::GPUUtils::Enable(kGlDebugOutput);
  m_installed = (isEnabled(kGlDebugOutput) != 0);
  if (!m_installed) {
    LOG_WARN("GLDebugCallback: GL_DEBUG_OUTPUT could not be enabled; "
             "production GL diagnostics disabled");
  } else {
    LOG_INFO("GLDebugCallback: production GL debug output installed");
  }
  return m_installed;
}

void GLDebugCallback::Shutdown() {
  if (!m_installed) {
    return;
  }
  const auto setCallback = reinterpret_cast<GlSetDebugMessageCallbackFn>(
      glfwGetProcAddress("glDebugMessageCallback"));
  const auto isEnabled =
      reinterpret_cast<GlIsEnabledFn>(glfwGetProcAddress("glIsEnabled"));
  if (setCallback != nullptr) {
    setCallback(reinterpret_cast<GlDebugCallbackFn>(m_prevCallback),
                m_prevUserParam);
  }
  if (isEnabled != nullptr) {
    if (m_wasEnabled) {
      NoMoreDay::utils::GPUUtils::Enable(kGlDebugOutput);
    } else {
      NoMoreDay::utils::GPUUtils::Disable(kGlDebugOutput);
    }
  }
  m_installed = false;
}

} // namespace NoMoreDay::render::debug
