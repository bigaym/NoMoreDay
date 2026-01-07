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

#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
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

    static void MemoryBarrier(unsigned int barriers = GL_SHADER_STORAGE_BARRIER_BIT) {
        static PFNGLMEMORYBARRIERPROC glMemoryBarrier_ptr = (PFNGLMEMORYBARRIERPROC)glfwGetProcAddress("glMemoryBarrier");
        if (glMemoryBarrier_ptr) {
            glMemoryBarrier_ptr(barriers);
        }
    }

    static void DispatchCompute(unsigned int numGroupsX, unsigned int numGroupsY, unsigned int numGroupsZ) {
        rlComputeShaderDispatch(numGroupsX, numGroupsY, numGroupsZ);
        MemoryBarrier();
    }
};

} // namespace NoMoreDay::utils
