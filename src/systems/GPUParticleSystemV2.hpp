#pragma once

#include <vector>
#include <raylib.h>
#include <raymath.h>
#include "../components/GPUData.hpp"
#include "../core/ComputeBuffer.hpp"

namespace NoMoreDay::systems {

/**
 * @brief High-performance GPU particle system using Indirect Drawing.
 * 
 * Architecture:
 * - Uses compute shader for physics and stream compaction
 * - Zero CPU readback via DrawArraysIndirect
 * - Supports 100k+ particles at 60fps
 * 
 * Buffer Layout:
 * - Slot 0: Particle buffer (all particles)
 * - Slot 1: Compact buffer (alive particles only)
 * - Slot 2: DrawIndirect buffer
 * - Slot 3: Atomic counter
 */
class GPUParticleSystemV2 {
public:
    // Singleton access
    static GPUParticleSystemV2& Get();
    
    // Lifecycle
    void Init(int maxParticles = 100000);
    void Shutdown();
    
    // Per-frame operations
    void Update(float dt);
    void Render(const Camera2D& camera);
    
    // Particle emission
    void Emit(const components::GPUParticle& particle);
    void EmitBatch(const std::vector<components::GPUParticle>& particles);
    
    // Accessors
    int GetMaxParticles() const { return m_maxParticles; }
    bool IsInitialized() const { return m_initialized; }
    
private:
    GPUParticleSystemV2() = default;
    ~GPUParticleSystemV2() = default;
    GPUParticleSystemV2(const GPUParticleSystemV2&) = delete;
    GPUParticleSystemV2& operator=(const GPUParticleSystemV2&) = delete;
    
    // Internal helpers
    void CreateQuadVAO();
    void LoadShaders();
    void CreateBuffers();
    Matrix BuildMVP(const Camera2D& camera) const;
    
    // State
    bool m_initialized = false;
    int m_maxParticles = 100000;
    int m_currentParticleCount = 0;  // Total particles in buffer (alive + dead slots)
    bool m_pingPong = false;         // For double buffering (input/output swap)
    
    // Staging buffer for new particles (CPU side)
    std::vector<components::GPUParticle> m_stagedParticles;
    
    // GPU Buffers
    core::ComputeBuffer m_particleBuffer;    // All particles
    core::ComputeBuffer m_compactBuffer;     // Compacted alive particles
    core::ComputeBuffer m_indirectBuffer;    // DrawArraysIndirect command
    core::ComputeBuffer m_atomicBuffer;      // Atomic counter for compaction
    
    // Shaders
    Shader m_computeShader = { 0 };
    Shader m_renderShader = { 0 };
    
    // Shader uniform locations
    int m_computeDtLoc = -1;
    int m_computeTotalLoc = -1;
    int m_renderMvpLoc = -1;
    
    // VAO for quad rendering
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
    
    // DrawIndirect command structure (must match OpenGL spec)
    struct DrawArraysIndirectCommand {
        uint32_t count;         // = 6 (vertices per quad)
        uint32_t instanceCount; // Alive particle count (GPU writes)
        uint32_t first;         // = 0
        uint32_t baseInstance;  // = 0
    };
};

} // namespace NoMoreDay::systems
