#pragma once
#include "rlgl.h"
#include <GLFW/glfw3.h>
#include <cstddef>
#include <vector>

namespace NoMoreDay::core {

class ComputeBuffer {
public:
  ComputeBuffer() = default;
  ~ComputeBuffer() { Release(); }

  // Disable copy
  ComputeBuffer(const ComputeBuffer &) = delete;
  ComputeBuffer &operator=(const ComputeBuffer &) = delete;

  // Allow move
  ComputeBuffer(ComputeBuffer &&other) noexcept
      : m_id(other.m_id), m_size(other.m_size) {
    other.m_id = 0;
    other.m_size = 0;
  }
  ComputeBuffer &operator=(ComputeBuffer &&other) noexcept {
    if (this != &other) {
      Release();
      m_id = other.m_id;
      m_size = other.m_size;
      other.m_id = 0;
      other.m_size = 0;
    }
    return *this;
  }

  void Create(size_t size, const void *data = nullptr,
              int usage = RL_DYNAMIC_DRAW) {
    if (m_id != 0)
      Release();
    m_size = size;
    m_id = rlLoadShaderBuffer(size, data, usage);
  }

  void Update(const void *data, size_t size, size_t offset = 0) {
    if (m_id == 0)
      return;
    rlUpdateShaderBuffer(m_id, data, size, offset);
  }

  void BindBase(unsigned int index) const {
    if (m_id == 0)
      return;
    rlBindShaderBuffer(m_id, index);
  }

  void Read(void *outData, size_t size, size_t offset = 0) const {
    if (m_id == 0)
      return;
    rlReadShaderBuffer(m_id, outData, size, offset);
  }

  /// Bind this buffer to a specific OpenGL buffer target (e.g.,
  /// GL_DRAW_INDIRECT_BUFFER)
  void Bind(unsigned int target) const {
    if (m_id == 0)
      return;
    // Use glBindBuffer directly (need to get the function pointer)
    typedef void (*PFNGLBINDBUFFERPROC)(unsigned int target,
                                        unsigned int buffer);
    static PFNGLBINDBUFFERPROC glBindBufferFn = nullptr;
    if (!glBindBufferFn) {
      glBindBufferFn = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    }
    if (glBindBufferFn) {
      glBindBufferFn(target, m_id);
    }
  }

  void Release() {
    if (m_id != 0) {
      rlUnloadShaderBuffer(m_id);
      m_id = 0;
      m_size = 0;
    }
  }

  unsigned int GetId() const { return m_id; }
  size_t GetSize() const { return m_size; }

private:
  unsigned int m_id = 0;
  size_t m_size = 0;
};

} // namespace NoMoreDay::core
