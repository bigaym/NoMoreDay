#pragma once
#include "GLFW/glfw3.h"
#include "core/logging/Logger.hpp"
#include "engine/render/RenderConstants.hpp"
#include "raylib.h"
#include "rlgl.h"
#include <cstdint>

// Basic types for GL if not available
#ifndef GL_SYNC_TYPEDEF_
typedef struct __GLsync *GLsync;
#define GL_SYNC_TYPEDEF_
#endif

namespace NoMoreDay::utils {

using namespace NoMoreDay::RenderConstants;

struct GPUSupportInfo {
  int majorVersion = 0;
  int minorVersion = 0;
  bool computeShaderSupported = false;
  bool indirectDrawSupported = false;
  bool persistentMappingSupported = false;
  int maxComputeWorkGroupCount[3] = {0};
  int maxComputeWorkGroupSize[3] = {0};
  int maxComputeWorkGroupInvocations = 0;
};

class GPUUtils {
public:
  // === 初始化 ===
  /**
   * @brief 检测 GPU 能力并加载所有 GL 扩展函数。
   * 必须在 OpenGL Context 创建后、任何渲染操作前调用。
   */
  static GPUSupportInfo Initialize();

  /**
   * @brief 检查是否已初始化。
   */
  static bool IsInitialized();

  /**
   * @brief 检查硬件能力。 (Initialize 内部调用)
   */
  static GPUSupportInfo CheckSupport();

  // === Memory Barriers ===
  /**
   * @brief 发出内存屏障，确保 GPU 操作顺序。
   * @param barriers 使用 Barrier 枚举组合 (如 Barrier::SSBO | Barrier::Command)
   */
  static void MemoryBarrier(Barrier barriers);

  // 便捷重载，接受原始 uint32_t (用于兼容)
  static void
  MemoryBarrier(uint32_t barriers =
                    0x00002000); // Default to GL_SHADER_STORAGE_BARRIER_BIT

  // === Compute Shader ===
  /**
   * @brief 分派 Compute Shader，自动添加 SSBO Barrier。
   */
  static void DispatchCompute(uint32_t groupsX, uint32_t groupsY,
                              uint32_t groupsZ);

  /**
   * @brief 分派 Compute Shader，不添加 Barrier (性能敏感场景)。
   */
  static void DispatchComputeNoBarrier(uint32_t groupsX, uint32_t groupsY,
                                       uint32_t groupsZ);

  // === Buffer Operations ===
  static void BindBuffer(uint32_t target, uint32_t bufferId);
  static void BindBufferBase(Binding binding, uint32_t bufferId);
  static void BindBufferBase(uint32_t binding, uint32_t bufferId);
  static void BindBufferRange(uint32_t target, uint32_t index,
                              uint32_t bufferId, ptrdiff_t offset,
                              ptrdiff_t size);

  static void GenBuffers(int n, uint32_t *buffers);
  static void DeleteBuffers(int n, const uint32_t *buffers);
  static void BufferData(uint32_t target, ptrdiff_t size, const void *data,
                         uint32_t usage);
  static void BufferSubData(uint32_t target, ptrdiff_t offset, ptrdiff_t size,
                            const void *data);
  static void GetBufferSubData(uint32_t target, ptrdiff_t offset,
                               ptrdiff_t size, void *data);
  static void BufferStorage(uint32_t target, ptrdiff_t size, const void *data,
                            uint32_t flags);

  // === Map/Unmap Operations ===
  static void *MapBufferRange(uint32_t target, ptrdiff_t offset,
                              ptrdiff_t length, uint32_t access);
  static void FlushMappedBufferRange(uint32_t target, ptrdiff_t offset,
                                     ptrdiff_t length);
  static bool UnmapBuffer(uint32_t target);

  // === Sync Operations ===
  static void *FenceSync(uint32_t condition, uint32_t flags);
  static void DeleteSync(void *sync);
  static uint32_t ClientWaitSync(void *sync, uint32_t flags, uint64_t timeout);

  // === Texture Operations ===
  static void GenTextures(int n, uint32_t *textures);
  static void DeleteTextures(int n, const uint32_t *textures);
  static void BindTexture(uint32_t target, uint32_t textureId);
  static void ActiveTexture(TextureUnit unit);
  static void ActiveTexture(uint32_t unit);
  static void TexParameteri(uint32_t target, uint32_t pname, int param);

  // 3D/Array textures
  static void TexStorage3D(uint32_t target, int levels, uint32_t internalformat,
                           int width, int height, int depth);
  static void TexSubImage3D(uint32_t target, int level, int xoffset,
                            int yoffset, int zoffset, int width, int height,
                            int depth, uint32_t format, uint32_t type,
                            const void *pixels);

  // === Image Binding (Compute Shader) ===
  static void BindImageTexture(uint32_t unit, uint32_t textureId, int level = 0,
                               bool layered = false, int layer = 0,
                               uint32_t access = 0x88B9,  // GL_WRITE_ONLY
                               uint32_t format = 0x8058); // GL_RGBA8

  // === Indirect Draw ===
  static void DrawArraysIndirect(uint32_t mode, size_t indirectOffset = 0);

private:
  GPUUtils() = delete;

  static bool s_initialized;

  // Basic Pointers
  static void *s_glMemoryBarrier;
  static void *s_glDrawArraysIndirect;
  static void *s_glBindBuffer;
  static void *s_glBindBufferBase;
  static void *s_glActiveTexture;
  static void *s_glBindImageTexture;

  // Buffer Pointers
  static void *s_glBufferStorage;
  static void *s_glFenceSync;
  static void *s_glDeleteSync;
  static void *s_glClientWaitSync;
  static void *s_glMapBufferRange;
  static void *s_glFlushMappedBufferRange;
  static void *s_glUnmapBuffer;
  static void *s_glGenBuffers;
  static void *s_glDeleteBuffers;
  static void *s_glBufferData;
  static void *s_glBufferSubData;
  static void *s_glGetBufferSubData;
  static void *s_glBindBufferRange;

  // Texture Pointers
  static void *s_glGenTextures;
  static void *s_glDeleteTextures;
  static void *s_glBindTexture;
  static void *s_glTexParameteri;
  static void *s_glTexStorage3D;
  static void *s_glTexSubImage3D;
};

} // namespace NoMoreDay::utils
