#pragma once

#include <vector>
#include <mutex>
#include <atomic>
#include <raylib.h>
#include <raymath.h>
#include "engine/render/GPUData.hpp"
#include "engine/render/ComputeBuffer.hpp"
#include "engine/render/PersistentBuffer.hpp"

namespace NoMoreDay::systems {

/**
 * @brief High-performance GPU particle system using Indirect Drawing.
 */
class GPUParticleSystem {
public:
    // Singleton access
    static GPUParticleSystem& Get();
    
    // Lifecycle
    void Init(int maxParticles = 100000);
    void Shutdown();
    void Clear(); // Kill all active particles immediately
    
    // Per-frame operations
    void Update(float dt);
    void Render(const Camera2D& camera);
    bool RenderEmissionSnapshot(const Camera2D& camera, unsigned int outputFramebuffer,
                               unsigned int restoreFramebuffer, int width, int height);
    
    // Particle emission
    void Emit(const components::GPUParticle& particle);
    void Emit(const components::GPUParticle& particle, int materialId);
    void EmitBatch(const std::vector<components::GPUParticle>& particles);
    void EmitBatch(const std::vector<components::GPUParticle>& particles,
                   int materialId);
    
    // Matrix helper
    Matrix BuildMVP(const Camera2D& camera) const;
    
    // Accessors
    int GetMaxParticles() const { return m_maxParticles; }
    bool IsInitialized() const { return m_initialized; }
    
    // Testing seam: inject shader loading failure
    void SetFailShadersForTesting(bool fail) { m_failShadersForTesting = fail; }
    
private:
    GPUParticleSystem() = default;
    ~GPUParticleSystem() = default;
    GPUParticleSystem(const GPUParticleSystem&) = delete;
    GPUParticleSystem& operator=(const GPUParticleSystem&) = delete;
    
    // Internal helpers
    void CreateQuadVAO();
    bool LoadShaders();
    void CreateBuffers();
    
    // State
    bool m_initialized = false;
    bool m_failShadersForTesting = false;
    int m_maxParticles = 100000;
    int m_currentParticleCount = 0;  // Total particles in buffer (alive + dead slots)
    int m_targetDispatchCount = 0;   // Adaptive dispatch range
    bool m_pingPong = false;         // For double buffering (input/output swap)
    bool m_requestClear = false;     // Trigger full GPU reset in next Update
    float m_totalTime = 0.0f;        // Total elapsed time for noise/variation
    
    void HardResetGPU();            // Physically zero out GPU buffers
    void FinalizeFrame();           // Run finalize shader 
    
    // Lock-free Emission
    std::atomic<uint32_t> m_emitHead{0};
    components::GPUParticle* m_mappedPtr = nullptr;
    uint32_t m_emissionCap = 0;

    // GPU Buffers
    core::ComputeBuffer m_particleBuffer;    // All particles
    core::ComputeBuffer m_compactBuffer;     // Compacted alive particles
    render::PersistentBuffer m_indirectBuffer; // DrawArraysIndirect command (Persistent)
    render::PersistentBuffer m_atomicBuffer; // Atomic counter (Persistent/Triple buffered)
    
    // Asynchronous state
    uint32_t m_lastKnownAliveCount = 0;      // Read from previous frame
    uint32_t m_readbackFrameCounter = 0;     // Counter to throttle synchronization
    bool m_atomicPingPong = false;           // Deprecated: PersistentBuffer handles this internally
    
    // Shaders
    Shader m_computeShader = { 0 };
    Shader m_renderShader = { 0 };
    Shader m_emissionSnapshotShader = { 0 };
    
    // Shader uniform locations
    int m_computeDtLoc = -1;
    int m_computeTimeLoc = -1;
    int m_computeTotalLoc = -1;
    int m_computeForceFieldCountLoc = -1;
    int m_computeSubEmitterEnabledLoc = -1;
    int m_renderMvpLoc = -1;
    int m_renderAtlasLoc = -1;
    int m_renderBlendPassLoc = -1;
    int m_renderMaterialCountLoc = -1;
    int m_renderNormalArrayLoc = -1;
    int m_renderMaskArrayLoc = -1;
    int m_renderDetailArrayLoc = -1;
    int m_renderMaterialQualityLoc = -1;
    int m_renderNormalLightingEnabledLoc = -1;
    int m_renderSpecularEnabledLoc = -1;
    int m_renderShadowFactorLoc = -1;
    int m_renderLinearPipelineLoc = -1;
    int m_emissionSnapshotMvpLoc = -1;
    
    // VAO for quad rendering
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
    
    struct DrawArraysIndirectCommand {
        uint32_t count;         // = 6 (vertices per quad)
        uint32_t instanceCount; // Alive particle count (GPU writes)
        uint32_t first;         // = 0
        uint32_t baseInstance;  // = 0
    };

    // Phase 3: Emission Buffer
    render::PersistentBuffer m_emissionBuffer;
    core::ComputeBuffer m_subEmissionBuffer;
    render::PersistentBuffer m_subEmitCountBuffer;
    Shader m_emitShader = { 0 };
    Shader m_subEmitShader = { 0 };
    Shader m_finalizeShader = { 0 }; // Pass 3: Sync atomic to indirect
    int m_emitCountLoc = -1;
    int m_subEmitCountLoc = -1;
    int m_subEmitMaxParticlesLoc = -1;
    uint32_t m_subEmissionCap = 2048;
};

/**
 * @brief Particle effect type enumeration
 */
enum class ParticleEffectType {
    Projectile,
    Movement,
    Area,
    Burst,
    Trail
};

/**
 * @brief Helper class for creating various particle effects
 */
class InkEffectHelper {
public:
    // Color Presets
    static constexpr Color COLOR_INK_LIGHT = { 40, 40, 45, 120 };
    static constexpr Color COLOR_INK_DARK = { 20, 20, 25, 200 };
    static constexpr Color COLOR_GOLD_CORE = { 255, 215, 0, 255 };
    static constexpr Color COLOR_GOLD_GLOW = { 255, 180, 50, 150 };
    static constexpr Color COLOR_FROST_LIGHT = { 150, 220, 255, 200 };
    static constexpr Color COLOR_FROST_CORE = { 100, 180, 255, 255 };
    static constexpr Color COLOR_LIGHTNING_CORE = { 200, 200, 255, 255 };
    static constexpr Color COLOR_LIGHTNING_GLOW = { 150, 150, 255, 180 };
    static constexpr Color COLOR_SHADOW_CORE = { 60, 40, 80, 240 };
    static constexpr Color COLOR_SHADOW_GLOW = { 100, 80, 140, 150 };
    static constexpr Color COLOR_POISON_CORE = { 80, 200, 60, 220 };
    static constexpr Color COLOR_POISON_GLOW = { 120, 255, 80, 150 };
    static constexpr Color COLOR_BLOOD_CORE = { 180, 30, 30, 255 };
    static constexpr Color COLOR_BLOOD_GLOW = { 220, 60, 60, 180 };
    static constexpr Color COLOR_WHITE_SPARK = { 255, 255, 255, 255 };
    static constexpr Color COLOR_SWORD_QI = { 180, 200, 255, 220 };

    static components::GPUParticle CreateInkTrail(Vector2 pos, Vector2 vel, float scale, float life);
    static std::vector<components::GPUParticle> CreateInkSplash(Vector2 pos, int count, float radius, float force);
    static void AppendInkSplash(std::vector<components::GPUParticle>& out, Vector2 pos, int count, float radius, float force);
    static components::GPUParticle CreateGoldParticle(Vector2 pos, Vector2 vel, float scale);
    
    static std::vector<components::GPUParticle> CreateProjectileTrail(Vector2 pos, Vector2 dir, Color coreColor, Color glowColor, float trailLength = 30.0f, int count = 5);
    static void AppendProjectileTrail(std::vector<components::GPUParticle>& out, Vector2 pos, Vector2 dir, Color coreColor, Color glowColor, float trailLength = 30.0f, int count = 5);
    
    static std::vector<components::GPUParticle> CreateDashEffect(Vector2 startPos, Vector2 dir, Color color, float dashLength = 100.0f, int count = 15);
    static void AppendDashEffect(std::vector<components::GPUParticle>& out, Vector2 startPos, Vector2 dir, Color color, float dashLength = 100.0f, int count = 15);
    
    static std::vector<components::GPUParticle> CreateAreaEffect(Vector2 center, float radius, Color coreColor, Color edgeColor, int count = 20, float duration = 0.5f);
    static void AppendAreaEffect(std::vector<components::GPUParticle>& out, Vector2 center, float radius, Color coreColor, Color edgeColor, int count = 20, float duration = 0.5f);
    
    static components::GPUParticle CreateSpark(Vector2 pos, Vector2 vel, Color color, float scale = 1.0f);
    static std::vector<components::GPUParticle> CreateSlashEffect(Vector2 pos, Vector2 dir, Color color, float length = 50.0f);
    static void AppendSlashEffect(std::vector<components::GPUParticle>& out, Vector2 pos, Vector2 dir, Color color, float length = 50.0f);
};

} // namespace NoMoreDay::systems
