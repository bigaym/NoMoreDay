#include "engine/render/PersistentBuffer.hpp"
#include "engine/render/GPUUtils.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

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

// Define GLsync if not available
typedef struct __GLsync *GLsync;

#include <cstddef> // for ptrdiff_t

#ifndef GL_VERSION_1_5
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
#endif

// Removed basic GL typedefs as they conflict with gl.h included via PCH/headers

#ifndef GL_VERSION_3_2
typedef unsigned long long GLuint64;
#endif

#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT 0x0002
#endif

#ifndef GL_VERSION_3_2
typedef unsigned long long GLuint64;
#endif

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif

// Function Pointers
typedef void (APIENTRY *PFNGLBUFFERSTORAGEPROC) (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
static PFNGLBUFFERSTORAGEPROC glBufferStorage = nullptr;

typedef GLsync (APIENTRY *PFNGLFENCESYNCPROC) (GLenum condition, GLbitfield flags);
static PFNGLFENCESYNCPROC glFenceSync = nullptr;

typedef void (APIENTRY *PFNGLDELETESYNCPROC) (GLsync sync);
static PFNGLDELETESYNCPROC glDeleteSync = nullptr;

typedef GLenum (APIENTRY *PFNGLCLIENTWAITSYNCPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
static PFNGLCLIENTWAITSYNCPROC glClientWaitSync = nullptr;

typedef void* (APIENTRY *PFNGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
static PFNGLMAPBUFFERRANGEPROC glMapBufferRange = nullptr;

typedef GLboolean (APIENTRY *PFNGLUNMAPBUFFERPROC) (GLenum target);
static PFNGLUNMAPBUFFERPROC glUnmapBuffer = nullptr;

typedef void (APIENTRY *PFNGLMEMORYBARRIERPROC)(unsigned int barriers);

namespace NoMoreDay::render {

PersistentBuffer::PersistentBuffer() = default;

PersistentBuffer::~PersistentBuffer() {
    Destroy();
}

bool PersistentBuffer::IsSupported() {
    // Load function pointers if not already loaded
    if (!glBufferStorage) {
        glBufferStorage = (PFNGLBUFFERSTORAGEPROC)glfwGetProcAddress("glBufferStorage");
        if (!glBufferStorage) {
            glBufferStorage = (PFNGLBUFFERSTORAGEPROC)glfwGetProcAddress("glBufferStorageARB");
        }
    }
    
    if (!glFenceSync) glFenceSync = (PFNGLFENCESYNCPROC)glfwGetProcAddress("glFenceSync");
    if (!glDeleteSync) glDeleteSync = (PFNGLDELETESYNCPROC)glfwGetProcAddress("glDeleteSync");
    if (!glClientWaitSync) glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)glfwGetProcAddress("glClientWaitSync");
    
    if (!glMapBufferRange) glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC)glfwGetProcAddress("glMapBufferRange");
    if (!glUnmapBuffer) glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)glfwGetProcAddress("glUnmapBuffer");

    bool hasFuncs = glBufferStorage && glFenceSync && glDeleteSync && glClientWaitSync && glMapBufferRange && glUnmapBuffer;
    
    if (hasFuncs) {
        return true;
    } else {
        LOG_WARN("PersistentBuffer: Missing required OpenGL extensions.");
        return false;
    }
}

void PersistentBuffer::Create(size_t slotSize, unsigned int usageHint) {
    if (m_bufferId != 0) Destroy();

    m_slotSize = slotSize;

    if (IsSupported()) {
        m_mode = Mode::Persistent;
        LOG_INFO("PersistentBuffer: Creating in PERSISTENT mode. SlotSize={}, Total={}", slotSize, slotSize * 3);
        CreatePersistent(slotSize);
    } else {
        m_mode = Mode::Compat;
        LOG_INFO("PersistentBuffer: Creating in COMPAT mode. SlotSize={}", slotSize);
        CreateCompat(slotSize);
    }
}

void PersistentBuffer::CreatePersistent(size_t size) {
    m_totalSize = size * 3;
    
    unsigned int id = 0;
    typedef void (APIENTRY *PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
    static PFNGLGENBUFFERSPROC glGenBuffers = (PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
    typedef void (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
    static PFNGLBINDBUFFERPROC glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");

    if (glGenBuffers && glBindBuffer) {
        glGenBuffers(1, &id);
        m_bufferId = id;
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bufferId);
        
        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, m_totalSize, nullptr, flags);
        
        m_mappedPtr = (uint8_t*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, m_totalSize, flags);
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind
        
        if (!m_mappedPtr) {
            LOG_ERROR("PersistentBuffer: Failed to map buffer!");
            m_mode = Mode::Compat;
            CreateCompat(size);
            return;
        }
        
    } else {
        LOG_ERROR("PersistentBuffer: Failed to load basic GL gen/bind functions.");
        m_mode = Mode::Compat;
        CreateCompat(size);
    }
}

void PersistentBuffer::CreateCompat(size_t size) {
    typedef void (APIENTRY *PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
    static PFNGLGENBUFFERSPROC glGenBuffers = (PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
    typedef void (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
    static PFNGLBINDBUFFERPROC glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    typedef void (APIENTRY *PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const void * data, GLenum usage);
    static PFNGLBUFFERDATAPROC glBufferData = (PFNGLBUFFERDATAPROC)glfwGetProcAddress("glBufferData");

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
        typedef void (APIENTRY *PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint *buffers);
        static PFNGLDELETEBUFFERSPROC glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)glfwGetProcAddress("glDeleteBuffers");
        
        for (int i = 0; i < 3; i++) {
            if (m_fences[i]) {
                glDeleteSync((GLsync)m_fences[i]);
                m_fences[i] = nullptr;
            }
        }

        if (m_mode == Mode::Persistent && m_mappedPtr) {
             typedef void (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
             static PFNGLBINDBUFFERPROC glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
             glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bufferId);
             glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
             glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
             m_mappedPtr = nullptr;
        }

        if (glDeleteBuffers) glDeleteBuffers(1, &m_bufferId);
        m_bufferId = 0;
    }
    m_stagingBuffer.clear();
}

void PersistentBuffer::WaitForFence(void*& fencePtr) {
    if (!fencePtr) return;
    GLsync fence = static_cast<GLsync>(fencePtr);
    
    GLenum result = GL_TIMEOUT_EXPIRED;
    int retries = 0;
    while (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED) {
        result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000); // 1ms
        if (result == GL_WAIT_FAILED) {
            LOG_ERROR("PersistentBuffer: Fence wait failed!");
            break;
        }
        retries++;
        if (retries > 1000) {
             LOG_ERROR("PersistentBuffer: Fence wait timeout!");
             break;
        }
    }
    glDeleteSync(fence);
    fencePtr = nullptr;
}

void* PersistentBuffer::BeginWrite() {
    if (m_mode == Mode::Persistent) {
        WaitForFence(m_fences[m_writeSlot]);
        return m_mappedPtr + m_writeSlot * m_slotSize;
    } else {
        return m_stagingBuffer.data();
    }
}

void PersistentBuffer::Flush() {
    if (m_mode == Mode::Persistent) {
        // Just ensure memory visibility to GPU
        // COHERENT already guarantees it, but a MemoryBarrier doesn't hurt if we want to be safe before Dispatch
        // Actually, MemoryBarrier is usually called *before* Dispatch if we wrote via Shader.
        // If we wrote via Mapped Pointer, and use COHERENT, we are good.
        // But let's add ClientMappedBufferBarrier just in case.
        static PFNGLMEMORYBARRIERPROC glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)glfwGetProcAddress("glMemoryBarrier");
        if (glMemoryBarrier) glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    } else {
        // Compat: Upload
        typedef void (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
        static PFNGLBINDBUFFERPROC glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
        typedef void (APIENTRY *PFNGLBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, const void * data);
        static PFNGLBUFFERSUBDATAPROC glBufferSubData = (PFNGLBUFFERSUBDATAPROC)glfwGetProcAddress("glBufferSubData");

        if (glBindBuffer && glBufferSubData) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bufferId);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_slotSize, m_stagingBuffer.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
    }
}

void PersistentBuffer::Lock() {
    if (m_mode == Mode::Persistent) {
        m_fences[m_writeSlot] = (void*)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        m_writeSlot = (m_writeSlot + 1) % 3;
    }
}

void PersistentBuffer::Read(void* data, size_t size) {
    if (m_mode == Mode::Persistent) {
        if (m_mappedPtr) {
            size_t copySize = std::min(size, m_slotSize);
            memcpy(data, m_mappedPtr + m_writeSlot * m_slotSize, copySize);
        }
    } else {
        typedef void (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
        static PFNGLBINDBUFFERPROC glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
        typedef void (APIENTRY *PFNGLGETBUFFERSUBDATAPROC) (GLenum target, GLintptr offset, GLsizeiptr size, void * data);
        static PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC)glfwGetProcAddress("glGetBufferSubData");

        if (glBindBuffer && glGetBufferSubData) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bufferId);
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size, data);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
    }
}

void PersistentBuffer::BindBase(unsigned int bindingPoint) const {
    typedef void (APIENTRY *PFNGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
    static PFNGLBINDBUFFERBASEPROC glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)glfwGetProcAddress("glBindBufferBase");
    
    typedef void (APIENTRY *PFNGLBINDBUFFERRANGEPROC) (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
    static PFNGLBINDBUFFERRANGEPROC glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)glfwGetProcAddress("glBindBufferRange");

    if (m_mode == Mode::Persistent) {
        // Bind the CURRENT write slot (Logic: We are about to Dispatch using current frame data)
        size_t offset = m_writeSlot * m_slotSize;
        if (glBindBufferRange) {
            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_bufferId, offset, m_slotSize);
        }
    } else {
        if (glBindBufferBase) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_bufferId);
        }
    }
}

} // namespace NoMoreDay::render
