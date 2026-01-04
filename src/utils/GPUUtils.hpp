#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "glad.h" // Now accessible via target_include_directories
#include "../tools/Logger.hpp"
#include <cstdio>

namespace NoMoreDay::utils {

struct GPUSupportInfo {
    int majorVersion = 0;
    int minorVersion = 0;
    bool computeShaderSupported = false;
    int maxComputeWorkGroupCount[3] = { 0, 0, 0 };
    int maxComputeWorkGroupSize[3] = { 0, 0, 0 };
    int maxComputeWorkGroupInvocations = 0;
};

class GPUUtils {
public:
    static GPUSupportInfo CheckSupport() {
        GPUSupportInfo info;

        // 获取 OpenGL 版本
        const char* versionStr = (const char*)glGetString(GL_VERSION);
        LOG_INFO("OpenGL Context Version: {}", versionStr ? versionStr : "Unknown");

        glGetIntegerv(GL_MAJOR_VERSION, &info.majorVersion);
        glGetIntegerv(GL_MINOR_VERSION, &info.minorVersion);
        
        // Some drivers might return 0 for GL_MAJOR_VERSION if not properly initialized
        if (info.majorVersion == 0 && versionStr) {
            sscanf(versionStr, "%d.%d", &info.majorVersion, &info.minorVersion);
        }

        LOG_INFO("Detected OpenGL Version: {}.{}", info.majorVersion, info.minorVersion);

        // Compute Shader requires OpenGL 4.3
        if (info.majorVersion > 4 || (info.majorVersion == 4 && info.minorVersion >= 3)) {
            info.computeShaderSupported = true;
        }

        if (info.computeShaderSupported) {
            LOG_INFO("Compute Shaders are SUPPORTED.");
            
            // Query Compute Shader limits
            for (int i = 0; i < 3; i++) {
                glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &info.maxComputeWorkGroupCount[i]);
                glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &info.maxComputeWorkGroupSize[i]);
            }
            glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &info.maxComputeWorkGroupInvocations);

            LOG_DEBUG("Max Work Group Count: [{}, {}, {}]", 
                info.maxComputeWorkGroupCount[0], info.maxComputeWorkGroupCount[1], info.maxComputeWorkGroupCount[2]);
            LOG_DEBUG("Max Work Group Size: [{}, {}, {}]", 
                info.maxComputeWorkGroupSize[0], info.maxComputeWorkGroupSize[1], info.maxComputeWorkGroupSize[2]);
            LOG_DEBUG("Max Work Group Invocations: {}", info.maxComputeWorkGroupInvocations);
        } else {
            LOG_WARN("Compute Shaders are NOT supported on this hardware/context.");
        }

        return info;
    }

    static void DispatchCompute(unsigned int programId, unsigned int numGroupsX, unsigned int numGroupsY, unsigned int numGroupsZ) {
        glUseProgram(programId);
        glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
        // Memory barrier to ensure SSBO writes are visible
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glUseProgram(0);
    }
};

} // namespace NoMoreDay::utils