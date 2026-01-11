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
class GPUParticleSystem {
public:
    // Singleton access
    static GPUParticleSystem& Get();
    
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
    GPUParticleSystem() = default;
    ~GPUParticleSystem() = default;
    GPUParticleSystem(const GPUParticleSystem&) = delete;
    GPUParticleSystem& operator=(const GPUParticleSystem&) = delete;
    
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

class InkEffectHelper {
public:
    static constexpr Color COLOR_INK_LIGHT = { 40, 40, 45, 120 };
    static constexpr Color COLOR_INK_DARK = { 20, 20, 25, 200 };
    static constexpr Color COLOR_GOLD_CORE = { 255, 215, 0, 255 };
    static constexpr Color COLOR_GOLD_GLOW = { 255, 180, 50, 150 };

    // Create a generic ink particle (for trails/ambient)
    static components::GPUParticle CreateInkTrail(Vector2 pos, Vector2 vel, float scale, float life);

    // Create a burst of ink particles
    static std::vector<components::GPUParticle> CreateInkSplash(Vector2 pos, int count, float radius, float force);

    // Create a gold stream particle (for empowered effects)
    static components::GPUParticle CreateGoldParticle(Vector2 pos, Vector2 vel, float scale);
};

} // namespace NoMoreDay::systems
