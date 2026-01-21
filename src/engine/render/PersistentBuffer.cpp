#include "engine/render/PersistentBuffer.hpp"
#include "engine/render/GPUUtils.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

// --- OpenGL Constants & Types Definition ---
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT 0x0080
#endif
#ifndef GL_MAP_READ_BIT
#define GL_MAP_READ_BIT 0x0001
#endif
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT 0x0002
#endif
#ifndef GL_DYNAMIC_STORAGE_BIT
#define GL_DYNAMIC_STORAGE_BIT 0x0100
#endif
#ifndef GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT
#define GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT 0x00004000
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif
#ifndef GL_ALREADY_SIGNALED
#define GL_ALREADY_SIGNALED 0x911A
#endif
#ifndef GL_TIMEOUT_EXPIRED
#define GL_TIMEOUT_EXPIRED 0x911C
#endif
#ifndef GL_CONDITION_SATISFIED
#define GL_CONDITION_SATISFIED 0x911C
#endif
#ifndef GL_WAIT_FAILED
#define GL_WAIT_FAILED 0x911D
#endif
#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT 0x00000001
#endif
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif

// Forward declare GLsync if needed
typedef struct __GLsync *GLsync;

// Function Pointers
typedef void(APIENTRY *PFNGLBUFFERSTORAGEPROC)(GLenum target, ptrdiff_t size,
                                               const void *data,
                                               GLbitfield flags);
static PFNGLBUFFERSTORAGEPROC glBufferStorage = nullptr;

typedef GLsync(APIENTRY *PFNGLFENCESYNCPROC)(GLenum condition,
                                             GLbitfield flags);
static PFNGLFENCESYNCPROC glFenceSync = nullptr;

typedef void(APIENTRY *PFNGLDELETESYNCPROC)(GLsync sync);
static PFNGLDELETESYNCPROC glDeleteSync = nullptr;

typedef GLenum(APIENTRY *PFNGLCLIENTWAITSYNCPROC)(GLsync sync, GLbitfield flags,
                                                  uint64_t timeout);
static PFNGLCLIENTWAITSYNCPROC glClientWaitSync = nullptr;

typedef void *(APIENTRY *PFNGLMAPBUFFERRANGEPROC)(GLenum target, ptrdiff_t offset,
                                                  ptrdiff_t length,
                                                  GLbitfield access);
static PFNGLMAPBUFFERRANGEPROC glMapBufferRange = nullptr;

typedef GLboolean(APIENTRY *PFNGLUNMAPBUFFERPROC)(GLenum target);
static PFNGLUNMAPBUFFERPROC glUnmapBuffer = nullptr;

typedef void(APIENTRY *PFNGLMEMORYBARRIERPROC)(unsigned int barriers);

namespace NoMoreDay::render {

PersistentBuffer::PersistentBuffer() = default;

PersistentBuffer::~PersistentBuffer() { Destroy(); }

bool PersistentBuffer::IsSupported() {
  if (!glBufferStorage) {
    glBufferStorage =
        (PFNGLBUFFERSTORAGEPROC)glfwGetProcAddress("glBufferStorage");
    if (!glBufferStorage) {
      glBufferStorage =
          (PFNGLBUFFERSTORAGEPROC)glfwGetProcAddress("glBufferStorageARB");
    }
  }

  if (!glFenceSync)
    glFenceSync = (PFNGLFENCESYNCPROC)glfwGetProcAddress("glFenceSync");
  if (!glDeleteSync)
    glDeleteSync = (PFNGLDELETESYNCPROC)glfwGetProcAddress("glDeleteSync");
  if (!glClientWaitSync)
    glClientWaitSync =
        (PFNGLCLIENTWAITSYNCPROC)glfwGetProcAddress("glClientWaitSync");

  if (!glMapBufferRange)
    glMapBufferRange =
        (PFNGLMAPBUFFERRANGEPROC)glfwGetProcAddress("glMapBufferRange");
  if (!glUnmapBuffer)
    glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)glfwGetProcAddress("glUnmapBuffer");

  bool hasFuncs = glBufferStorage && glFenceSync && glDeleteSync &&
                  glClientWaitSync && glMapBufferRange && glUnmapBuffer;

  return hasFuncs;
}

void PersistentBuffer::Create(size_t slotSize, int bufferCount,
                              unsigned int usageHint) {
  if (m_bufferId != 0)
    Destroy();

  const size_t alignment = 256;
  m_slotSize = (slotSize + alignment - 1) & ~(alignment - 1);
  m_bufferCount = bufferCount;
  if (m_bufferCount < 2)
    m_bufferCount = 2;

  if (IsSupported()) {
    m_mode = Mode::Persistent;
    LOG_INFO("PersistentBuffer: Creating in PERSISTENT mode. SlotSize={}, "
             "Count={}, Total={}",
             m_slotSize, m_bufferCount, m_slotSize * m_bufferCount);
    CreatePersistent(m_slotSize);
  } else {
    m_mode = Mode::Compat;
    LOG_INFO("PersistentBuffer: Creating in COMPAT mode. SlotSize={}",
             m_slotSize);
    CreateCompat(m_slotSize);
  }
}

void PersistentBuffer::CreatePersistent(size_t size) {
  m_totalSize = size * m_bufferCount;
  m_fences.resize(m_bufferCount, nullptr);

  unsigned int id = 0;
  typedef void(APIENTRY * PFNGLGENBUFFERSPROC)(GLsizei n, GLuint * buffers);
  static PFNGLGENBUFFERSPROC glGenBuffers =
      (PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
  typedef void(APIENTRY * PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
  static PFNGLBINDBUFFERPROC glBindBuffer =
      (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");

  if (glGenBuffers && glBindBuffer) {
    glGenBuffers(1, &id);
    m_bufferId = id;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bufferId);

    GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT |
                       GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, m_totalSize, nullptr, flags);

    m_mappedPtr = (uint8_t *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                              m_totalSize, flags);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    if (!m_mappedPtr) {
      LOG_ERROR("PersistentBuffer: Failed to map buffer!");
      m_mode = Mode::Compat;
      CreateCompat(size);
    }
  } else {
    m_mode = Mode::Compat;
    CreateCompat(size);
  }
}

void PersistentBuffer::CreateCompat(size_t size) {
  typedef void(APIENTRY * PFNGLGENBUFFERSPROC)(GLsizei n, GLuint * buffers);
  static PFNGLGENBUFFERSPROC glGenBuffers =
      (PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
  typedef void(APIENTRY * PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
  static PFNGLBINDBUFFERPROC glBindBuffer =
      (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
  typedef void(APIENTRY * PFNGLBUFFERDATAPROC)(GLenum target, ptrdiff_t size,
                                               const void *data, GLenum usage);
  static PFNGLBUFFERDATAPROC glBufferData =
      (PFNGLBUFFERDATAPROC)glfwGetProcAddress("glBufferData");

  if (glGenBuffers && glBindBuffer && glBufferData) {
    glGenBuffers(1, &m_bufferId);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  }

  m_stagingBuffer.resize(size);
}

void PersistentBuffer::Destroy() {
  if (m_bufferId != 0) {
    typedef void(APIENTRY * PFNGLDELETEBUFFERSPROC)(GLsizei n,
                                                    const GLuint *buffers);
    static PFNGLDELETEBUFFERSPROC glDeleteBuffers =
        (PFNGLDELETEBUFFERSPROC)glfwGetProcAddress("glDeleteBuffers");

    for (size_t i = 0; i < m_fences.size(); i++) {
      if (m_fences[i]) {
        glDeleteSync((GLsync)m_fences[i]);
        m_fences[i] = nullptr;
      }
    }
    m_fences.clear();

    if (m_mode == Mode::Persistent && m_mappedPtr) {
      typedef void(APIENTRY * PFNGLBINDBUFFERPROC)(GLenum target,
                                                   GLuint buffer);
      static PFNGLBINDBUFFERPROC glBindBuffer =
          (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bufferId);
      glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
      m_mappedPtr = nullptr;
    }

    if (glDeleteBuffers)
      glDeleteBuffers(1, &m_bufferId);
    m_bufferId = 0;
  }
  m_stagingBuffer.clear();
}

void PersistentBuffer::WaitForFence(void *&fencePtr) {
  if (!fencePtr)
    return;
  GLsync fence = static_cast<GLsync>(fencePtr);
  glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
  glDeleteSync(fence);
  fencePtr = nullptr;
}

void *PersistentBuffer::BeginWrite() {
  if (m_mode == Mode::Persistent) {
    WaitForFence(m_fences[m_writeSlot]);
    return m_mappedPtr + m_writeSlot * m_slotSize;
  } else {
    return m_stagingBuffer.data();
  }
}

void PersistentBuffer::Flush() {
  if (m_mode == Mode::Persistent) {
    static PFNGLMEMORYBARRIERPROC glMemoryBarrier =
        (PFNGLMEMORYBARRIERPROC)glfwGetProcAddress("glMemoryBarrier");
    if (glMemoryBarrier)
      glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
  } else {
    typedef void(APIENTRY * PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
    static PFNGLBINDBUFFERPROC glBindBuffer =
        (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    typedef void(APIENTRY * PFNGLBUFFERSUBDATAPROC)(
        GLenum target, ptrdiff_t offset, ptrdiff_t size, const void *data);
    static PFNGLBUFFERSUBDATAPROC glBufferSubData =
        (PFNGLBUFFERSUBDATAPROC)glfwGetProcAddress("glBufferSubData");

    if (glBindBuffer && glBufferSubData) {
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bufferId);
      glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_slotSize,
                      m_stagingBuffer.data());
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
  }
}

void PersistentBuffer::Lock() {
  if (m_mode == Mode::Persistent) {
    if (m_fences[m_writeSlot]) {
      glDeleteSync((GLsync)m_fences[m_writeSlot]);
    }
    m_fences[m_writeSlot] =
        (void *)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    m_writeSlot = (m_writeSlot + 1) % m_bufferCount;
  }
}

void PersistentBuffer::Read(void *data, size_t size) const {
  if (m_mode == Mode::Persistent) {
    if (m_mappedPtr) {
      int targetSlot = (m_writeSlot - 1 + m_bufferCount) % m_bufferCount;
      const_cast<PersistentBuffer *>(this)->WaitForFence(
          const_cast<PersistentBuffer *>(this)->m_fences[targetSlot]);

      static PFNGLMEMORYBARRIERPROC glMemoryBarrier =
          (PFNGLMEMORYBARRIERPROC)glfwGetProcAddress("glMemoryBarrier");
      if (glMemoryBarrier)
        glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

      size_t copySize = std::min(size, m_slotSize);
      memcpy(data, m_mappedPtr + targetSlot * m_slotSize, copySize);
    }
  } else {
    typedef void(APIENTRY * PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
    static PFNGLBINDBUFFERPROC glBindBuffer =
        (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    typedef void(APIENTRY * PFNGLGETBUFFERSUBDATAPROC)(
        GLenum target, ptrdiff_t offset, ptrdiff_t size, void *data);
    static PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData =
        (PFNGLGETBUFFERSUBDATAPROC)glfwGetProcAddress("glGetBufferSubData");

    if (glBindBuffer && glGetBufferSubData) {
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bufferId);
      glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size, data);
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
  }
}

void PersistentBuffer::BindBase(unsigned int bindingPoint) const {
  typedef void(APIENTRY * PFNGLBINDBUFFERBASEPROC)(GLenum target, GLuint index,
                                                   GLuint buffer);
  static PFNGLBINDBUFFERBASEPROC glBindBufferBase =
      (PFNGLBINDBUFFERBASEPROC)glfwGetProcAddress("glBindBufferBase");

  typedef void(APIENTRY * PFNGLBINDBUFFERRANGEPROC)(
      GLenum target, GLuint index, GLuint buffer, ptrdiff_t offset,
      ptrdiff_t size);
  static PFNGLBINDBUFFERRANGEPROC glBindBufferRange =
      (PFNGLBINDBUFFERRANGEPROC)glfwGetProcAddress("glBindBufferRange");

  if (m_mode == Mode::Persistent) {
    size_t offset = m_writeSlot * m_slotSize;
    if (glBindBufferRange) {
      glBindBufferRange(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_bufferId,
                        offset, m_slotSize);
    }
  } else {
    if (glBindBufferBase) {
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_bufferId);
    }
  }
}

void PersistentBuffer::BindPrevious(unsigned int bindingPoint) const {
  typedef void(APIENTRY * PFNGLBINDBUFFERRANGEPROC)(
      GLenum target, GLuint index, GLuint buffer, ptrdiff_t offset,
      ptrdiff_t size);
  static PFNGLBINDBUFFERRANGEPROC glBindBufferRange =
      (PFNGLBINDBUFFERRANGEPROC)glfwGetProcAddress("glBindBufferRange");

  if (m_mode == Mode::Persistent && m_bufferId != 0 && glBindBufferRange) {
    int prevSlot = (m_writeSlot - 1 + m_bufferCount) % m_bufferCount;
    const_cast<PersistentBuffer *>(this)->WaitForFence(
        const_cast<PersistentBuffer *>(this)->m_fences[prevSlot]);

    size_t offset = prevSlot * m_slotSize;
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_bufferId,
                      offset, m_slotSize);
  } else if (m_mode == Mode::Compat) {
    BindBase(bindingPoint);
  }
}

void PersistentBuffer::BindPreviousNoSync(unsigned int bindingPoint) const {
  typedef void(APIENTRY *PFNGLBINDBUFFERRANGEPROC)(
      GLenum target, GLuint index, GLuint buffer, ptrdiff_t offset,
      ptrdiff_t size);
  static PFNGLBINDBUFFERRANGEPROC glBindBufferRange =
      (PFNGLBINDBUFFERRANGEPROC)glfwGetProcAddress("glBindBufferRange");

  if (m_mode == Mode::Persistent && m_bufferId != 0 && glBindBufferRange) {
    int prevSlot = (m_writeSlot - 1 + m_bufferCount) % m_bufferCount;
    // SKIP WaitForFence to avoid CPU Stall. 
    // GPU sync should be handled by glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT)
    size_t offset = prevSlot * m_slotSize;
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_bufferId,
                      offset, m_slotSize);
  } else if (m_mode == Mode::Compat) {
    BindBase(bindingPoint);
  }
}

void PersistentBuffer::BindOldest(unsigned int bindingPoint) const {
  typedef void(APIENTRY * PFNGLBINDBUFFERRANGEPROC)(
      GLenum target, GLuint index, GLuint buffer, ptrdiff_t offset,
      ptrdiff_t size);
  static PFNGLBINDBUFFERRANGEPROC glBindBufferRange =
      (PFNGLBINDBUFFERRANGEPROC)glfwGetProcAddress("glBindBufferRange");

  if (m_mode == Mode::Persistent && m_bufferId != 0 && glBindBufferRange) {
    int oldestSlot = (m_writeSlot - 2 + m_bufferCount) % m_bufferCount;
    const_cast<PersistentBuffer *>(this)->WaitForFence(
        const_cast<PersistentBuffer *>(this)->m_fences[oldestSlot]);

    size_t offset = oldestSlot * m_slotSize;
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_bufferId,
                      offset, m_slotSize);
  } else if (m_mode == Mode::Compat) {
    BindBase(bindingPoint);
  }
}

void PersistentBuffer::Bind(unsigned int target, int slotType) const {
  typedef void(APIENTRY *PFNGLBINDBUFFERPROC)(unsigned int target,
                                              unsigned int buffer);
  static PFNGLBINDBUFFERPROC glBindBufferFn = nullptr;
  if (!glBindBufferFn) {
    glBindBufferFn = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
  }

  if (m_bufferId == 0 || !glBindBufferFn)
    return;

  glBindBufferFn(target, m_bufferId);

  // Note: For non-indexed targets like GL_DRAW_INDIRECT_BUFFER, 
  // you still need to use the correct offset in the draw call 
  // if you want to select a slot, because glBindBufferRange 
  // doesn't work for these targets.
  // However, we bind the buffer ID here for convenience.
}

} // namespace NoMoreDay::render