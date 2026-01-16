#pragma once
#include <raylib.h>
#include <stdint.h>
#include <type_traits>

namespace NoMoreDay::components {

/**
 * @brief Structure for GPU particles matching SSBO layout.
 * STRICTLY 64 BYTES (16 * 4) to ensure cross-driver std430 compatibility.
 */
struct GPUParticle {
    Vector2 position     = { 0.0f, 0.0f }; // 8
    Vector2 velocity     = { 0.0f, 0.0f }; // 8
    Vector2 acceleration = { 0.0f, 0.0f }; // 8
    Color color          = { 0, 0, 0, 0 }; // 4
    float lifetime       = 0.0f;           // 4
    float maxLifetime    = 0.0f;           // 4
    float scale          = 0.0f;           // 4
    uint32_t flags       = 0;              // 4
    float growthRate     = 0.0f;           // 4
    float padding[4]     = { 0.0f, 0.0f, 0.0f, 0.0f }; // 16

    GPUParticle() = default;
};

// Ensure Stride is exactly 64 bytes
static_assert(sizeof(GPUParticle) == 64, "GPUParticle struct must be exactly 64 bytes for SSBO alignment");

/**
 * @brief Structure for GPU entities (Physics & Sorting).
 * STRICTLY 32 BYTES (16 * 2) to match physics.compute.
 */
struct GPUEntity {
    Vector2 position = { 0.0f, 0.0f }; // 8
    Vector2 velocity = { 0.0f, 0.0f }; // 8
    float radius     = 0.0f;           // 4
    int32_t type     = 0;              // 4
    int32_t id       = 0;              // 4
    float padding    = 0.0f;           // 4

    GPUEntity() = default;
};

// Ensure Stride is exactly 32 bytes
static_assert(sizeof(GPUEntity) == 32, "GPUEntity struct must be exactly 32 bytes for physics SSBO compatibility");

/**
 * @brief Structure for GPU skill effects (SDF Rendering).
 * STRICTLY 64 BYTES (16 * 4) for alignment.
 */
struct GPUSkillEffect {
    Vector2 position     = { 0.0f, 0.0f }; // 8
    Vector2 velocity     = { 0.0f, 0.0f }; // 8
    Vector4 coreColor    = { 1.0f, 1.0f, 1.0f, 1.0f }; // 16
    Vector4 glowColor    = { 1.0f, 1.0f, 1.0f, 1.0f }; // 16
    float radius         = 0.0f;           // 4
    float sectorAngle    = 0.0f;           // 4 (Degrees)
    float softness       = 0.0f;           // 4
    float type           = 0.0f;           // 4 (0=Fan/Sector, 1=Annulus/Circle, 2=Beam)
    
    GPUSkillEffect() = default;
};

// Ensure Stride is exactly 64 bytes
static_assert(sizeof(GPUSkillEffect) == 64, "GPUSkillEffect struct must be exactly 64 bytes for SSBO alignment");

/**
 * @brief Centralized Color Manager for VFX
 * 颜色管理器：统一管理游戏内的特效颜色
 */
namespace Colors {
    // Defines a color in 0xRRGGBBAA format for easy hex usage
    // 使用 0xRRGGBBAA 格式定义的颜色辅助函数
    constexpr Color FromHex(uint32_t hex) {
        return Color{
            static_cast<unsigned char>((hex >> 24) & 0xFF),
            static_cast<unsigned char>((hex >> 16) & 0xFF),
            static_cast<unsigned char>((hex >> 8) & 0xFF),
            static_cast<unsigned char>((hex) & 0xFF)
        };
    }

    // --- Blade Ascendant Theme (剑修主题) ---
    
    // Low opacity trail color (Very faint water/ink)
    // 极淡的水墨色拖尾 (高透明度)
    constexpr Color INK_TRAIL_PALE = { 180, 220, 235, 40 }; 

    // Deep ink for impact/core visuals
    // 深色水墨，用于打击核心或强调
    constexpr Color INK_DEEP = { 20, 25, 35, 220 };

    // Standard Blade Cyan (The energy color)
    // 标准剑气天青色
    constexpr Color BLADE_CYAN = { 195, 248, 245, 255 };
    
    // Speed Line / Particle bright accent
    // 速度线/粒子的高亮色
    constexpr Color SPEED_ACCENT = { 200, 255, 255, 200 };
}

} // namespace NoMoreDay::components
