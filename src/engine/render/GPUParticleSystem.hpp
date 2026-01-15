#pragma once

#include <vector>
#include <raylib.h>
#include <raymath.h>
#include "engine/render/GPUData.hpp"
#include "engine/render/ComputeBuffer.hpp"

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

    std::mutex m_emitMutex; // Protects m_stagedParticles
};

/**
 * @brief Particle effect type enumeration for different skill categories
 */
enum class ParticleEffectType {
    Projectile,     // Bound to projectile position
    Movement,       // Dash/teleport - at origin and along path
    Area,           // Sustained area - within radius
    Burst,          // Instant impact
    Trail           // Following entity
};

/**
 * @brief Helper class for creating various particle effects with ink-painting style
 */
class InkEffectHelper {
public:
    // === Color Presets ===
    static constexpr Color COLOR_INK_LIGHT = { 40, 40, 45, 120 };
    static constexpr Color COLOR_INK_DARK = { 20, 20, 25, 200 };
    static constexpr Color COLOR_GOLD_CORE = { 255, 215, 0, 255 };
    static constexpr Color COLOR_GOLD_GLOW = { 255, 180, 50, 150 };
    
    // New color presets for variety
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

    // === Basic Particle Creators ===
    // Create a generic ink particle (for trails/ambient)
    static components::GPUParticle CreateInkTrail(Vector2 pos, Vector2 vel, float scale, float life);

    // Create a burst of ink particles (reduced size)
    static std::vector<components::GPUParticle> CreateInkSplash(Vector2 pos, int count, float radius, float force);
    static void AppendInkSplash(std::vector<components::GPUParticle>& out, Vector2 pos, int count, float radius, float force);

    // Create a gold stream particle (for empowered effects)
    static components::GPUParticle CreateGoldParticle(Vector2 pos, Vector2 vel, float scale);
    
    // === New Specialized Creators for Different Skill Types ===
    
    // Projectile trail - small particles following projectile direction
    static std::vector<components::GPUParticle> CreateProjectileTrail(
        Vector2 pos, Vector2 dir, Color coreColor, Color glowColor, 
        float trailLength = 30.0f, int count = 5);
    static void AppendProjectileTrail(
        std::vector<components::GPUParticle>& out,
        Vector2 pos, Vector2 dir, Color coreColor, Color glowColor, 
        float trailLength = 30.0f, int count = 5);
    
    // Movement/dash effect - particles at start position spreading along dash direction
    static std::vector<components::GPUParticle> CreateDashEffect(
        Vector2 startPos, Vector2 dir, Color color, 
        float dashLength = 100.0f, int count = 15);
    static void AppendDashEffect(
        std::vector<components::GPUParticle>& out,
        Vector2 startPos, Vector2 dir, Color color, 
        float dashLength = 100.0f, int count = 15);
    
    // Area sustained effect - particles within radius
    static std::vector<components::GPUParticle> CreateAreaEffect(
        Vector2 center, float radius, Color coreColor, Color edgeColor,
        int count = 20, float duration = 0.5f);
    static void AppendAreaEffect(
        std::vector<components::GPUParticle>& out,
        Vector2 center, float radius, Color coreColor, Color edgeColor,
        int count = 20, float duration = 0.5f);
    
    // Spark/flash effect - bright, quick particles
    static components::GPUParticle CreateSpark(Vector2 pos, Vector2 vel, Color color, float scale = 1.0f);
    
    // Sword qi slash effect - thin elongated particles
    static std::vector<components::GPUParticle> CreateSlashEffect(
        Vector2 pos, Vector2 dir, Color color, float length = 50.0f);
    static void AppendSlashEffect(
        std::vector<components::GPUParticle>& out,
        Vector2 pos, Vector2 dir, Color color, float length = 50.0f);
};

} // namespace NoMoreDay::systems
