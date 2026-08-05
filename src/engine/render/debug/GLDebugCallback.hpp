#pragma once

#include <atomic>
#include <cstdint>

namespace NoMoreDay::render::debug {

// Resident production GL debug callback (P0 S3). Installs GL_DEBUG_OUTPUT with
// a severity-filtered glDebugMessageCallback once at engine init so driver
// errors surface through the log instead of silently. Diagnostic-only: it
// never aborts rendering when unsupported.
//
// Coexistence with the hardware validation gate: the gate installs its own
// scoped callback for the gate lifecycle (GlDebugOutputGuard) and restores the
// previous callback and GL_DEBUG_OUTPUT enable state on exit. The production
// install therefore composes with gate runs regardless of ordering - the guard
// saves the production callback if it is active and restores it afterwards.
class GLDebugCallback {
public:
  static GLDebugCallback &Get();

  // Installs the production debug callback. Returns true when installed and
  // enabled. Returns false (and logs the reason) when glDebugMessageCallback is
  // unavailable or GL_DEBUG_OUTPUT cannot be enabled; rendering is unaffected.
  bool Install();
  // Restores the previous callback and GL_DEBUG_OUTPUT enable state.
  void Shutdown();

  [[nodiscard]] bool IsInstalled() const { return m_installed; }

  // Number of ERROR-type / HIGH-severity messages reported since install.
  // Written from the GL thread only; read for tests and diagnostics.
  [[nodiscard]] uint64_t GetReportedCount() const {
    return m_reportedCount.load(std::memory_order_relaxed);
  }

private:
  GLDebugCallback() = default;
  ~GLDebugCallback() = default;

  // GL callback functions use the platform calling convention (APIENTRY),
  // applied when registering the pointer in the .cpp translation unit.
  static void OnDebugMessage(uint32_t source, uint32_t type, uint32_t id,
                             uint32_t severity, int length,
                             const char *message, const void *userParam);

  bool m_installed = false;
  bool m_wasEnabled = false;
  void *m_prevCallback = nullptr;
  const void *m_prevUserParam = nullptr;
  std::atomic<uint64_t> m_reportedCount{0};
};

} // namespace NoMoreDay::render::debug
