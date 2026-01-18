#ifndef NOMOREDAY_GPUDATA_HPP
#define NOMOREDAY_GPUDATA_HPP

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
 * @brief Structure for GPU HoloBlade instances.
 * 48 Bytes (16 * 3) for alignment.
 */
struct HoloBladeInstance {
    Vector2 position     = { 0.0f, 0.0f }; // 8
    float rotation       = 0.0f;           // 4
    float scale          = 1.0f;           // 4
    Vector4 holoColor    = { 1.0f, 1.0f, 1.0f, 1.0f }; // 16
    float rimStrength    = 0.0f;           // 4
    float noiseSpeed     = 0.0f;           // 4
    float padding[2]      = { 0.0f, 0.0f }; // 8

    HoloBladeInstance() = default;
};

static_assert(sizeof(HoloBladeInstance) == 48, "HoloBladeInstance struct must be 48 bytes for SSBO alignment");

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

    // --- Rarity Colors (物品稀有度) ---
    
    constexpr Color RARITY_COMMON    = { 180, 180, 180, 255 }; // Common / 普通
    constexpr Color RARITY_UNCOMMON  = { 100, 255, 100, 255 }; // Uncommon / 优秀 (Lime/Green)
    constexpr Color RARITY_MAGIC     = { 60, 130, 255, 255 };  // Magic / 魔法
    constexpr Color RARITY_RARE      = { 255, 220, 0, 255 };   // Rare / 稀有
    constexpr Color RARITY_SET       = { 0, 255, 0, 255 };     // Set / 套装 (Green)
    constexpr Color RARITY_EPIC      = { 190, 60, 255, 255 };  // Epic / 史诗
    constexpr Color RARITY_LEGENDARY = { 255, 140, 0, 255 };   // Legendary / 传说
    constexpr Color RARITY_MYTHIC    = { 255, 40, 40, 255 };    // Mythic / 神话
    constexpr Color RARITY_ANCIENT   = { 230, 0, 0, 255 };      // Ancient / 远古

    // --- Elemental Colors (元素属性) ---
    
    // Fire / 火焰
    constexpr Color ELEM_FIRE      = { 255, 80, 30, 255 };   
    // Cold / 冰霜
    constexpr Color ELEM_COLD      = { 80, 180, 255, 255 };  
    // Lightning / 闪电
    constexpr Color ELEM_LIGHTNING = { 255, 255, 100, 255 }; 
    // Poison / 毒素
    constexpr Color ELEM_POISON    = { 100, 255, 60, 255 };  
    // Shadow / 暗影
    constexpr Color ELEM_SHADOW    = { 130, 50, 200, 255 };  
    // Void / 虚空
    constexpr Color ELEM_VOID      = { 40, 10, 60, 255 };    

    // --- UI & Status Colors (UI 与 状态) ---
    
    // Health Bar / 生命值
    constexpr Color UI_HEALTH = { 230, 40, 40, 255 };  
    // Mana Bar / 法力值
    constexpr Color UI_MANA   = { 40, 100, 230, 255 }; 
    // Gold / 金币
    constexpr Color UI_GOLD   = { 255, 215, 0, 255 };  
    // Experience / 经验值
    constexpr Color UI_XP     = { 100, 200, 255, 255 }; 

    // --- UI Control & Edge Colors (UI 控件与边缘) ---
    
    // Default Border / 默认边框
    constexpr Color UI_BORDER_DEFAULT   = { 80, 80, 90, 255 };
    // Hovered Border / 悬停边框
    constexpr Color UI_BORDER_HOVER     = { 120, 120, 140, 255 };
    // Active or Selected Border / 激活或选中边框
    constexpr Color UI_BORDER_ACTIVE    = { 255, 215, 0, 255 }; 
    // Disabled Border / 禁用边框
    constexpr Color UI_BORDER_DISABLED  = { 50, 50, 55, 255 };
    // Danger or Error Border / 危险或错误边框
    constexpr Color UI_BORDER_DANGER    = { 220, 60, 60, 255 };
    // Success Border / 成功边框
    constexpr Color UI_BORDER_SUCCESS   = { 60, 220, 60, 255 };
    // Info or Hint Border / 信息或提示边框
    constexpr Color UI_BORDER_INFO      = { 60, 160, 255, 255 };
    // Color for socket info in tooltips
    // Color for socket info in tooltips
    constexpr Color COLOR_SOCKET_INFO   = { 208, 239, 232, 255 };

    // --- Modern UI Theme Colors (Standardized) ---
    // Panels
    constexpr Color UI_BACKGROUND       = { 35, 35, 45, 180 }; // Panel Background
    constexpr Color UI_BORDER           = { 70, 70, 85, 255 }; // Panel Border
    constexpr Color UI_BORDER_HIGHLIGHT = { 200, 170, 50, 255 }; // Gold Highlight
    constexpr Color UI_SLOT_BG          = { 25, 25, 35, 200 }; // Slot Background

    // Text Resources
    constexpr Color TEXT_PRIMARY        = { 245, 245, 245, 255 };
    constexpr Color TEXT_SECONDARY      = { 180, 180, 180, 255 };
    constexpr Color TEXT_HIGHLIGHT      = { 255, 215, 0, 255 };

    // Buttons
    constexpr Color BUTTON_NORMAL       = { 50, 50, 65, 255 };
    constexpr Color BUTTON_HOVER        = { 70, 70, 95, 255 };
    constexpr Color BUTTON_PRESS        = { 40, 40, 55, 255 };

    // Status / Feedback
    constexpr Color STATUS_DANGER       = { 200, 50, 50, 255 }; // Red/Danger
    constexpr Color STATUS_SUCCESS      = { 50, 200, 50, 255 }; // Green/Success
    constexpr Color STATUS_INFO         = { 60, 160, 255, 255 }; // Blue/Info

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

    // Ink Black (#1A1A1A)
    // 水墨黑 (设计规范色)
    constexpr Color INK_BLACK = { 26, 26, 26, 255 };

    // Pure White Highlight
    // 纯白高光
    constexpr Color BLADE_WHITE = { 255, 255, 255, 255 };

    // Spirit Blade (Translucent blue-white for Blade Formation)
    // 灵体飞剑 (半透明蓝白 - 灵剑决)
    constexpr Color SPIRIT_BLADE = { 200, 230, 255, 160 };

    // Mind Blade Core (High-bright line)
    // 心剑核心 (高亮核心线 - 心剑·无影)
    constexpr Color MIND_BLADE_CORE = { 240, 255, 255, 255 };

    // Ink Silhouette (Pure black for Phantom Flash)
    // 水墨残影 (纯黑墨迹 - 绝影闪)
    constexpr Color INK_SILHOUETTE = { 10, 10, 10, 230 };

    // --- Map Affix Colors (地图词缀颜色) ---
    // Helpful/Positive (Player benefits)
    constexpr Color MAP_AFFIX_POSITIVESize = { 100, 255, 100, 255 }; // Light Green / 有益词缀
    // Harmful/Negative (Enemy strength)
    constexpr Color MAP_AFFIX_NEGATIVESize = { 255, 100, 100, 255 }; // Light Red / 敌人强化词缀

    // Correcting typos for easier use
    constexpr Color MAP_AFFIX_POSITIVE = { 100, 255, 100, 255 };
    constexpr Color MAP_AFFIX_NEGATIVE = { 255, 100, 100, 255 };
}

} // namespace NoMoreDay::components

#endif // NOMOREDAY_GPUDATA_HPP
