#include "engine/render/GPUUtils.hpp"

namespace NoMoreDay::utils {

bool GPUUtils::s_initialized = false;
void *GPUUtils::s_glMemoryBarrier = nullptr;
void *GPUUtils::s_glDrawArraysIndirect = nullptr;
void *GPUUtils::s_glBindBuffer = nullptr;
void *GPUUtils::s_glBindBufferBase = nullptr;
void *GPUUtils::s_glActiveTexture = nullptr;
void *GPUUtils::s_glBindImageTexture = nullptr;

void *GPUUtils::s_glBufferStorage = nullptr;
void *GPUUtils::s_glFenceSync = nullptr;
void *GPUUtils::s_glDeleteSync = nullptr;
void *GPUUtils::s_glClientWaitSync = nullptr;
void *GPUUtils::s_glMapBufferRange = nullptr;
void *GPUUtils::s_glUnmapBuffer = nullptr;
void *GPUUtils::s_glGenBuffers = nullptr;
void *GPUUtils::s_glDeleteBuffers = nullptr;
void *GPUUtils::s_glBufferData = nullptr;
void *GPUUtils::s_glBufferSubData = nullptr;
void *GPUUtils::s_glGetBufferSubData = nullptr;
void *GPUUtils::s_glBindBufferRange = nullptr;

void *GPUUtils::s_glGenTextures = nullptr;
void *GPUUtils::s_glDeleteTextures = nullptr;
void *GPUUtils::s_glBindTexture = nullptr;
void *GPUUtils::s_glTexParameteri = nullptr;
void *GPUUtils::s_glTexStorage3D = nullptr;
void *GPUUtils::s_glTexSubImage3D = nullptr;

GPUSupportInfo GPUUtils::Initialize() {
  if (s_initialized) {
    return CheckSupport();
  }

  GPUSupportInfo info = CheckSupport();

  // Load Base Functions
  s_glMemoryBarrier = (void *)glfwGetProcAddress("glMemoryBarrier");
  s_glDrawArraysIndirect = (void *)glfwGetProcAddress("glDrawArraysIndirect");
  s_glBindBuffer = (void *)glfwGetProcAddress("glBindBuffer");
  s_glBindBufferBase = (void *)glfwGetProcAddress("glBindBufferBase");
  s_glActiveTexture = (void *)glfwGetProcAddress("glActiveTexture");
  s_glBindImageTexture = (void *)glfwGetProcAddress("glBindImageTexture");

  // Load Buffer Functions
  s_glBufferStorage = (void *)glfwGetProcAddress("glBufferStorage");
  if (!s_glBufferStorage)
    s_glBufferStorage = (void *)glfwGetProcAddress("glBufferStorageARB");

  s_glFenceSync = (void *)glfwGetProcAddress("glFenceSync");
  s_glDeleteSync = (void *)glfwGetProcAddress("glDeleteSync");
  s_glClientWaitSync = (void *)glfwGetProcAddress("glClientWaitSync");
  s_glMapBufferRange = (void *)glfwGetProcAddress("glMapBufferRange");
  s_glUnmapBuffer = (void *)glfwGetProcAddress("glUnmapBuffer");
  s_glGenBuffers = (void *)glfwGetProcAddress("glGenBuffers");
  s_glDeleteBuffers = (void *)glfwGetProcAddress("glDeleteBuffers");
  s_glBufferData = (void *)glfwGetProcAddress("glBufferData");
  s_glBufferSubData = (void *)glfwGetProcAddress("glBufferSubData");
  s_glGetBufferSubData = (void *)glfwGetProcAddress("glGetBufferSubData");
  s_glBindBufferRange = (void *)glfwGetProcAddress("glBindBufferRange");

  // Load Texture Functions
  s_glGenTextures = (void *)glfwGetProcAddress("glGenTextures");
  s_glDeleteTextures = (void *)glfwGetProcAddress("glDeleteTextures");
  s_glBindTexture = (void *)glfwGetProcAddress("glBindTexture");
  s_glTexParameteri = (void *)glfwGetProcAddress("glTexParameteri");
  s_glTexStorage3D = (void *)glfwGetProcAddress("glTexStorage3D");
  s_glTexSubImage3D = (void *)glfwGetProcAddress("glTexSubImage3D");

  // Verify Critical Functions
  info.indirectDrawSupported = (s_glDrawArraysIndirect != nullptr);
  info.persistentMappingSupported =
      (s_glBufferStorage != nullptr && s_glFenceSync != nullptr);

  s_initialized = true;
  LOG_INFO("GPUUtils initialized. GL {}.{}, Indirect: {}, Persistent: {}, "
           "Compute: {}",
           info.majorVersion, info.minorVersion, info.indirectDrawSupported,
           info.persistentMappingSupported, info.computeShaderSupported);

  return info;
}

bool GPUUtils::IsInitialized() { return s_initialized; }

GPUSupportInfo GPUUtils::CheckSupport() {
  GPUSupportInfo info;
  int version = rlGetVersion();
  if (version == RL_OPENGL_43) {
    info.majorVersion = 4;
    info.minorVersion = 3;
    info.computeShaderSupported = true;
  } else if (version == RL_OPENGL_33) {
    info.majorVersion = 3;
    info.minorVersion = 3;
  }
  return info;
}

void GPUUtils::MemoryBarrier(Barrier barriers) {
  MemoryBarrier(static_cast<uint32_t>(barriers));
}

void GPUUtils::MemoryBarrier(uint32_t barriers) {
  if (s_glMemoryBarrier) {
    using FnType = void(APIENTRY *)(uint32_t);
    reinterpret_cast<FnType>(s_glMemoryBarrier)(barriers);
  }
}

void GPUUtils::DispatchCompute(uint32_t groupsX, uint32_t groupsY,
                               uint32_t groupsZ) {
  rlComputeShaderDispatch(groupsX, groupsY, groupsZ);
  MemoryBarrier(Barrier::SSBO);
}

void GPUUtils::DispatchComputeNoBarrier(uint32_t groupsX, uint32_t groupsY,
                                        uint32_t groupsZ) {
  rlComputeShaderDispatch(groupsX, groupsY, groupsZ);
}

void GPUUtils::BindBuffer(uint32_t target, uint32_t bufferId) {
  if (s_glBindBuffer) {
    using FnType = void(APIENTRY *)(uint32_t, uint32_t);
    reinterpret_cast<FnType>(s_glBindBuffer)(target, bufferId);
  }
}

void GPUUtils::BindBufferBase(Binding binding, uint32_t bufferId) {
  BindBufferBase(static_cast<uint32_t>(binding), bufferId);
}

void GPUUtils::BindBufferBase(uint32_t binding, uint32_t bufferId) {
  if (s_glBindBufferBase) {
    using FnType = void(APIENTRY *)(uint32_t, uint32_t, uint32_t);
    reinterpret_cast<FnType>(s_glBindBufferBase)(
        0x90D2, binding, bufferId); // GL_SHADER_STORAGE_BUFFER
  } else {
    rlBindShaderBuffer(bufferId, binding);
  }
}

void GPUUtils::BindBufferRange(uint32_t target, uint32_t index,
                               uint32_t bufferId, ptrdiff_t offset,
                               ptrdiff_t size) {
  if (s_glBindBufferRange) {
    using FnType =
        void(APIENTRY *)(uint32_t, uint32_t, uint32_t, ptrdiff_t, ptrdiff_t);
    reinterpret_cast<FnType>(s_glBindBufferRange)(target, index, bufferId,
                                                  offset, size);
  }
}

void GPUUtils::GenBuffers(int n, uint32_t *buffers) {
  if (s_glGenBuffers) {
    using FnType = void(APIENTRY *)(int, uint32_t *);
    reinterpret_cast<FnType>(s_glGenBuffers)(n, buffers);
  }
}

void GPUUtils::DeleteBuffers(int n, const uint32_t *buffers) {
  if (s_glDeleteBuffers) {
    using FnType = void(APIENTRY *)(int, const uint32_t *);
    reinterpret_cast<FnType>(s_glDeleteBuffers)(n, buffers);
  }
}

void GPUUtils::BufferData(uint32_t target, ptrdiff_t size, const void *data,
                          uint32_t usage) {
  if (s_glBufferData) {
    using FnType =
        void(APIENTRY *)(uint32_t, ptrdiff_t, const void *, uint32_t);
    reinterpret_cast<FnType>(s_glBufferData)(target, size, data, usage);
  }
}

void GPUUtils::BufferSubData(uint32_t target, ptrdiff_t offset, ptrdiff_t size,
                             const void *data) {
  if (s_glBufferSubData) {
    using FnType =
        void(APIENTRY *)(uint32_t, ptrdiff_t, ptrdiff_t, const void *);
    reinterpret_cast<FnType>(s_glBufferSubData)(target, offset, size, data);
  }
}

void GPUUtils::GetBufferSubData(uint32_t target, ptrdiff_t offset,
                                ptrdiff_t size, void *data) {
  if (s_glGetBufferSubData) {
    using FnType = void(APIENTRY *)(uint32_t, ptrdiff_t, ptrdiff_t, void *);
    reinterpret_cast<FnType>(s_glGetBufferSubData)(target, offset, size, data);
  }
}

void GPUUtils::BufferStorage(uint32_t target, ptrdiff_t size, const void *data,
                             uint32_t flags) {
  if (s_glBufferStorage) {
    using FnType =
        void(APIENTRY *)(uint32_t, ptrdiff_t, const void *, uint32_t);
    reinterpret_cast<FnType>(s_glBufferStorage)(target, size, data, flags);
  }
}

void *GPUUtils::MapBufferRange(uint32_t target, ptrdiff_t offset,
                               ptrdiff_t length, uint32_t access) {
  if (s_glMapBufferRange) {
    using FnType = void *(APIENTRY *)(uint32_t, ptrdiff_t, ptrdiff_t, uint32_t);
    return reinterpret_cast<FnType>(s_glMapBufferRange)(target, offset, length,
                                                        access);
  }
  return nullptr;
}

bool GPUUtils::UnmapBuffer(uint32_t target) {
  if (s_glUnmapBuffer) {
    using FnType = unsigned char(APIENTRY *)(uint32_t);
    return reinterpret_cast<FnType>(s_glUnmapBuffer)(target) != 0;
  }
  return false;
}

void *GPUUtils::FenceSync(uint32_t condition, uint32_t flags) {
  if (s_glFenceSync) {
    using FnType = void *(APIENTRY *)(uint32_t, uint32_t);
    return reinterpret_cast<FnType>(s_glFenceSync)(condition, flags);
  }
  return nullptr;
}

void GPUUtils::DeleteSync(void *sync) {
  if (s_glDeleteSync) {
    using FnType = void(APIENTRY *)(void *);
    reinterpret_cast<FnType>(s_glDeleteSync)(sync);
  }
}

uint32_t GPUUtils::ClientWaitSync(void *sync, uint32_t flags,
                                  uint64_t timeout) {
  if (s_glClientWaitSync) {
    using FnType = uint32_t(APIENTRY *)(void *, uint32_t, uint64_t);
    return reinterpret_cast<FnType>(s_glClientWaitSync)(sync, flags, timeout);
  }
  return 0x911D; // GL_WAIT_FAILED (safe fallback)
}

void GPUUtils::DrawArraysIndirect(uint32_t mode, size_t indirectOffset) {
  if (s_glDrawArraysIndirect) {
    using FnType = void(APIENTRY *)(uint32_t, const void *);
    reinterpret_cast<FnType>(s_glDrawArraysIndirect)(
        mode, reinterpret_cast<const void *>(indirectOffset));
  }
}

void GPUUtils::ActiveTexture(TextureUnit unit) {
  ActiveTexture(0x84C0 + static_cast<uint32_t>(unit)); // GL_TEXTURE0 + unit
}

void GPUUtils::ActiveTexture(uint32_t unit) {
  if (s_glActiveTexture) {
    using FnType = void(APIENTRY *)(uint32_t);
    reinterpret_cast<FnType>(s_glActiveTexture)(unit);
  }
}

void GPUUtils::BindTexture(uint32_t target, uint32_t textureId) {
  if (s_glBindTexture) {
    using FnType = void(APIENTRY *)(uint32_t, uint32_t);
    reinterpret_cast<FnType>(s_glBindTexture)(target, textureId);
  } else {
    glBindTexture(target, textureId);
  }
}

void GPUUtils::TexParameteri(uint32_t target, uint32_t pname, int param) {
  if (s_glTexParameteri) {
    using FnType = void(APIENTRY *)(uint32_t, uint32_t, int);
    reinterpret_cast<FnType>(s_glTexParameteri)(target, pname, param);
  } else {
    glTexParameteri(target, pname, param);
  }
}

void GPUUtils::GenTextures(int n, uint32_t *textures) {
  if (s_glGenTextures) {
    using FnType = void(APIENTRY *)(int, uint32_t *);
    reinterpret_cast<FnType>(s_glGenTextures)(n, textures);
  } else {
    glGenTextures(n, textures);
  }
}

void GPUUtils::DeleteTextures(int n, const uint32_t *textures) {
  if (s_glDeleteTextures) {
    using FnType = void(APIENTRY *)(int, const uint32_t *);
    reinterpret_cast<FnType>(s_glDeleteTextures)(n, textures);
  } else {
    glDeleteTextures(n, textures);
  }
}

void GPUUtils::TexStorage3D(uint32_t target, int levels,
                            uint32_t internalformat, int width, int height,
                            int depth) {
  if (s_glTexStorage3D) {
    using FnType = void(APIENTRY *)(uint32_t, int, uint32_t, int, int, int);
    reinterpret_cast<FnType>(s_glTexStorage3D)(target, levels, internalformat,
                                               width, height, depth);
  }
}

void GPUUtils::TexSubImage3D(uint32_t target, int level, int xoffset,
                             int yoffset, int zoffset, int width, int height,
                             int depth, uint32_t format, uint32_t type,
                             const void *pixels) {
  if (s_glTexSubImage3D) {
    using FnType = void(APIENTRY *)(uint32_t, int, int, int, int, int, int, int,
                                    uint32_t, uint32_t, const void *);
    reinterpret_cast<FnType>(s_glTexSubImage3D)(target, level, xoffset, yoffset,
                                                zoffset, width, height, depth,
                                                format, type, pixels);
  }
}

void GPUUtils::BindImageTexture(uint32_t unit, uint32_t textureId, int level,
                                bool layered, int layer, uint32_t access,
                                uint32_t format) {
  if (s_glBindImageTexture) {
    using FnType = void(APIENTRY *)(uint32_t, uint32_t, int, unsigned char, int,
                                    uint32_t, uint32_t);
    reinterpret_cast<FnType>(s_glBindImageTexture)(
        unit, textureId, level, layered ? 1 : 0, layer, access, format);
  }
}

} // namespace NoMoreDay::utils
