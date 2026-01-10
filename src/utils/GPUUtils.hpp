#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "GLFW/glfw3.h"
#include "../tools/Logger.hpp"
#include <cstdio>

namespace NoMoreDay::utils {

struct GPUSupportInfo {
    int majorVersion = 0;
    int minorVersion = 0;
    bool computeShaderSupported = false;
    int maxComputeWorkGroupCount[3] = {0};
    int maxComputeWorkGroupSize[3] = {0};
    int maxComputeWorkGroupInvocations = 0;
};

// Unified OpenGL pointer for functions not in rlgl
#ifndef APIENTRY
    #if defined(_WIN32)
        #define APIENTRY __stdcall
    #else
        #define APIENTRY
    #endif
#endif

typedef void (APIENTRY *PFNGLMEMORYBARRIERPROC)(unsigned int barriers);
typedef void (APIENTRY *PFNGLBINDIMAGETEXTUREPROC)(unsigned int unit, unsigned int texture, 
    int level, unsigned char layered, int layer, unsigned int access, unsigned int format);

// OpenGL constants for image binding (not in raylib headers)
#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif

#ifndef GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#endif

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif

#ifndef GL_READ_WRITE
#define GL_READ_WRITE 0x88BA
#endif

#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

class GPUUtils {
public:
    static GPUSupportInfo CheckSupport() {
        GPUSupportInfo info;
        
        // Manual binding for functions not in rlgl
        static PFNGLMEMORYBARRIERPROC glMemoryBarrier_ptr = nullptr;
        if (glMemoryBarrier_ptr == nullptr) {
            glMemoryBarrier_ptr = (PFNGLMEMORYBARRIERPROC)glfwGetProcAddress("glMemoryBarrier");
            if (glMemoryBarrier_ptr) {
                LOG_INFO("Successfully bound glMemoryBarrier via glfwGetProcAddress.");
            } else {
                LOG_WARN("Failed to bind glMemoryBarrier! Compute results might be inconsistent.");
            }
        }

        // 获取 OpenGL 版本
        int version = rlGetVersion(); 
        LOG_INFO("OpenGL Context Version enum: {}", version);

        if (version == RL_OPENGL_43) {
            info.majorVersion = 4;
            info.minorVersion = 3;
            info.computeShaderSupported = true;
        } else if (version == RL_OPENGL_33) {
            info.majorVersion = 3;
            info.minorVersion = 3;
        }

        LOG_INFO("Detected OpenGL Version: {}.{}", info.majorVersion, info.minorVersion);

        if (info.computeShaderSupported) {
            LOG_INFO("Compute Shaders are SUPPORTED.");
        } else {
            LOG_WARN("Compute Shaders are NOT supported on this hardware/context.");
        }

        return info;
    }

    static void MemoryBarrier(unsigned int barriers = GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT) {
        static PFNGLMEMORYBARRIERPROC glMemoryBarrier_ptr = (PFNGLMEMORYBARRIERPROC)glfwGetProcAddress("glMemoryBarrier");
        if (glMemoryBarrier_ptr) {
            glMemoryBarrier_ptr(barriers);
        }
    }

    /**
     * @brief Bind a texture as an image for compute shader access
     * @param unit Image unit (0-7 typically)
     * @param textureId OpenGL texture ID
     * @param level Mipmap level (usually 0)
     * @param layered If true, all layers are bound
     * @param layer Layer to bind if not layered
     * @param access GL_READ_ONLY, GL_WRITE_ONLY, or GL_READ_WRITE
     * @param format Internal format (e.g., GL_RGBA8)
     */
    static void BindImageTexture(unsigned int unit, unsigned int textureId, int level = 0, 
                                  bool layered = false, int layer = 0, 
                                  unsigned int access = GL_WRITE_ONLY, 
                                  unsigned int format = GL_RGBA8) {
        static PFNGLBINDIMAGETEXTUREPROC glBindImageTexture_ptr = nullptr;
        if (glBindImageTexture_ptr == nullptr) {
            glBindImageTexture_ptr = (PFNGLBINDIMAGETEXTUREPROC)glfwGetProcAddress("glBindImageTexture");
            if (!glBindImageTexture_ptr) {
                LOG_WARN("Failed to bind glBindImageTexture! Image-based compute will not work.");
                return;
            }
        }
        glBindImageTexture_ptr(unit, textureId, level, layered ? 1 : 0, layer, access, format);
    }

    static void DispatchCompute(unsigned int numGroupsX, unsigned int numGroupsY, unsigned int numGroupsZ) {
        rlComputeShaderDispatch(numGroupsX, numGroupsY, numGroupsZ);
        MemoryBarrier();
    }
};

} // namespace NoMoreDay::utils
