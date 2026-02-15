#ifndef NOMOREDAY_GPUDATA_HPP
#define NOMOREDAY_GPUDATA_HPP

#pragma once
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <stdint.h>
#include <type_traits>

namespace NoMoreDay::render::abi {
inline constexpr uint32_t GPU_ABI_VERSION = 1;
inline constexpr uint32_t GPU_ABI_COMPAT_MIN_VERSION =
    (GPU_ABI_VERSION > 0) ? (GPU_ABI_VERSION - 1) : 0;
}

namespace NoMoreDay::Constants::GPU {
  // Texture Array Constraints
  constexpr int TEXTURE_LAYER_SIZE = 128;       // Standardized sprite size (px)
  constexpr int MAX_TEXTURE_LAYERS = 256;       // Max sprites in primary array
  constexpr int SDF_CIRCLE_TYPE = -1;           // Special type for SDF rendering
  constexpr int MAX_FORCE_FIELDS = 16;
  constexpr int MAX_TRAILS = 512;
  constexpr int MAX_TRAIL_POINTS_PER_TRAIL = 64;
  
  // Status Visual Indices (packed into activeStatusMask bits)
  constexpr uint32_t STATUS_NONE = 0;
  constexpr uint32_t STATUS_FROZEN = 1 << 0;
  constexpr uint32_t STATUS_BURNING = 1 << 1;
  constexpr uint32_t STATUS_POISONED = 1 << 2;
  constexpr uint32_t STATUS_SHOCKED = 1 << 3;
}

namespace NoMoreDay::Constants::Lighting {
constexpr float FIRE_RADIUS = 120.0f;
constexpr float FIRE_INTENSITY = 1.5f;
constexpr float FIRE_COLOR_R = 1.0f;
constexpr float FIRE_COLOR_G = 0.7f;
constexpr float FIRE_COLOR_B = 0.3f;

constexpr float SKILL_ICE_RADIUS = 80.0f;
constexpr float SKILL_ICE_INTENSITY = 2.0f;
constexpr float SKILL_ICE_COLOR_R = 0.5f;
constexpr float SKILL_ICE_COLOR_G = 0.8f;
constexpr float SKILL_ICE_COLOR_B = 1.0f;

constexpr float EXPLOSION_RADIUS = 300.0f;
constexpr float EXPLOSION_INTENSITY = 5.0f;

constexpr float AMBIENT_FIREFLY_RADIUS = 40.0f;
constexpr float AMBIENT_FIREFLY_INTENSITY = 0.5f;

constexpr int MAX_LIGHTS = 256;
}

namespace NoMoreDay::components {

// /**
//  * @brief CPU component to store previous position for interpolation.
//  */
// struct PrevPosition {
//   float x = 0.0f;
//   float y = 0.0f;
// };
// NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PrevPosition, x, y)

/**
 * @brief Structure for GPU particles matching SSBO layout.
 * STRICTLY 64 BYTES (16 * 4) to ensure cross-driver std430 compatibility.
 */
struct GPUParticle {
  Vector2 position = {0.0f, 0.0f};             // 8
  Vector2 velocity = {0.0f, 0.0f};             // 8
  Vector2 acceleration = {0.0f, 0.0f};         // 8
  Color color = {0, 0, 0, 0};                  // 4
  float lifetime = 0.0f;                       // 4
  float maxLifetime = 0.0f;                    // 4
  float scale = 0.0f;                          // 4
  uint32_t flags = 0;                          // 4
  float growthRate = 0.0f;                     // 4
  float rotation = 0.0f;                       // 4
  int16_t textureIndex = -1;                   // 2
  uint16_t subUV = 0;                          // 2 (rows << 8) | cols
  uint16_t animFrameCount = 0;                 // 2
  uint8_t blendMode = 0;                       // 1 (0=Alpha, 1=Additive)
  uint8_t subEmitterType = 0;                  // 1 (0=None)
  float subEmitterParam = 0.0f;                // 4

  GPUParticle() = default;
};

// Ensure Stride is exactly 64 bytes
static_assert(sizeof(GPUParticle) == 64,
              "GPUParticle struct must be exactly 64 bytes for SSBO alignment");

/**
 * @brief Structure for GPU material data (SSBO).
 * STRICTLY 64 BYTES (16 * 4) for std430 alignment.
 */
struct GPUMaterialData {
  float baseColorR = 1.0f;
  float baseColorG = 1.0f;
  float baseColorB = 1.0f;
  float baseColorA = 1.0f;

  float emissiveR = 0.0f;
  float emissiveG = 0.0f;
  float emissiveB = 0.0f;
  float emissiveIntensity = 0.0f;

  float distortion = 0.0f;
  uint32_t blendMode = 0;
  uint32_t shaderVariant = 0;
  uint32_t flags = 0;

  int32_t textureSlot0 = -1;
  int32_t textureSlot1 = -1;
  int32_t textureSlot2 = -1;
  int32_t textureSlot3 = -1;
};

static_assert(sizeof(GPUMaterialData) == 64,
              "GPUMaterialData struct must be exactly 64 bytes for SSBO alignment");

/**
 * @brief Structure for GPU distortion sources (screen-space shockwaves).
 * STRICTLY 16 BYTES for std430 alignment.
 */
struct GPUDistortionSource {
  float posX = 0.0f;
  float posY = 0.0f;
  float radius = 0.0f;
  float strength = 0.0f;
};

static_assert(sizeof(GPUDistortionSource) == 16,
              "GPUDistortionSource must be 16 bytes for SSBO alignment");

struct GPUTrailPoint {
  float posX = 0.0f;
  float posY = 0.0f;
  float dirX = 0.0f;
  float dirY = 0.0f;
  float width = 0.0f;
  float lifetime = 0.0f;
  uint32_t colorPacked = 0;
  uint32_t flags = 0;
};

static_assert(sizeof(GPUTrailPoint) == 32,
              "GPUTrailPoint struct must be exactly 32 bytes for SSBO alignment");

struct GPUTrailHeader {
  int32_t headIndex = 0;
  int32_t pointCount = 0;
  int32_t maxPoints = 64;
  float maxLifetime = 0.5f;
  float widthStart = 8.0f;
  float widthEnd = 1.0f;
  uint32_t colorStart = 0xFFFFFFFF;
  uint32_t colorEnd = 0x00000000;
};

static_assert(sizeof(GPUTrailHeader) == 32,
              "GPUTrailHeader struct must be exactly 32 bytes");

enum class ForceFieldType : uint32_t {
  Radial = 0,
  Vortex = 1,
  Noise = 2,
};

struct GPUForceField {
  float posX = 0.0f;
  float posY = 0.0f;
  float radius = 100.0f;
  float strength = 50.0f;
  uint32_t type = 0;
  float falloff = 1.0f;
  float noiseFrequency = 1.0f;
  float padding = 0.0f;
};

static_assert(sizeof(GPUForceField) == 32,
              "GPUForceField struct must be exactly 32 bytes");

enum class LightType : uint8_t {
  PointLight = 0,
  SpotLight = 1,
  AmbientZone = 2,
};

struct GPULight {
  float posX = 0.0f;
  float posY = 0.0f;
  float radius = 100.0f;
  float intensity = 1.0f;
  float colorR = 1.0f;
  float colorG = 1.0f;
  float colorB = 1.0f;
  float colorA = 1.0f;
};

static_assert(sizeof(GPULight) == 32,
              "GPULight struct must be exactly 32 bytes for SSBO alignment");

/**
 * @brief Structure for GPU entities (Physics & Sorting).
 * STRICTLY 64 BYTES (16 * 4) to match physics.compute.
 * Includes prevPosition for render interpolation to smooth movement.
 */
struct GPUEntity {
  Vector2 position = {0.0f, 0.0f}; // 8  - Current physics position
  Vector2 prevPosition = {
      0.0f, 0.0f}; // 8  - Previous frame position (for interpolation)
  Vector2 velocity = {0.0f, 0.0f}; // 8  - Current velocity
  float radius = 0.0f;             // 4  - Collision/render radius
  int32_t type = 0;                // 4  - Entity type (0=player, 1=enemy, etc.)
  uint32_t flags = 0;              // 4  - Behavior flags
  uint32_t frameId = 0;            // 4  - Frame ID for stale data detection
  float padding[6] = {0.0f};       // 24 - Padding to 64 bytes

  GPUEntity() = default;
};

// Flags for GPUEntity
constexpr uint32_t GPU_ENTITY_FLAG_KINEMATIC =
    1 << 0; // Entity moved by CPU/Logic, GPU skips integration
constexpr uint32_t GPU_ENTITY_FLAG_NO_RENDER =
    1 << 1; // Entity not rendered by MDI (e.g. has Sprite)
constexpr uint32_t GPU_ENTITY_FLAG_CHASING =
    1 << 2; // Entity follows flow field steering in GPU physics
constexpr uint32_t GPU_ENTITY_FLAG_NO_ROTATION =
    1 << 3; // Entity maintains fixed rotation (ignores velocity alignment)

// Ensure Stride is exactly 64 bytes for SSBO alignment
static_assert(
    sizeof(GPUEntity) == 64,
    "GPUEntity struct must be exactly 64 bytes for physics SSBO compatibility");

namespace GPUFlags {
  constexpr uint32_t AI_STATE_SHIFT = 8;
  constexpr uint32_t AI_STATE_MASK = 0xFF << AI_STATE_SHIFT;
  constexpr uint32_t MATERIAL_ID_SHIFT = 16;
  constexpr uint32_t MATERIAL_ID_MASK = 0xFFFFu << MATERIAL_ID_SHIFT;
  
  inline constexpr uint32_t PackAIState(uint8_t state) {
    return static_cast<uint32_t>(state) << AI_STATE_SHIFT;
  }
  inline constexpr uint8_t UnpackAIState(uint32_t flags) {
    return static_cast<uint8_t>((flags & AI_STATE_MASK) >> AI_STATE_SHIFT);
  }

  inline constexpr void PackMaterialId(uint32_t &flags, int materialId) {
    uint32_t clamped = 0;
    if (materialId > 0) {
      clamped = static_cast<uint32_t>(materialId > 0xFFFF ? 0xFFFF : materialId);
    }
    flags = (flags & ~MATERIAL_ID_MASK) | (clamped << MATERIAL_ID_SHIFT);
  }

  inline constexpr int UnpackMaterialId(uint32_t flags) {
    return static_cast<int>((flags & MATERIAL_ID_MASK) >> MATERIAL_ID_SHIFT);
  }
}

/**
 * @brief Structure for GPU skill effects (SDF Rendering).
 * STRICTLY 64 BYTES (16 * 4) for alignment.
 */
struct GPUSkillEffect {
  Vector2 position = {0.0f, 0.0f};              // 8
  Vector2 velocity = {0.0f, 0.0f};              // 8
  Vector4 coreColor = {1.0f, 1.0f, 1.0f, 1.0f}; // 16
  Vector4 glowColor = {1.0f, 1.0f, 1.0f, 1.0f}; // 16
  float radius = 0.0f;                          // 4
  float sectorAngle = 0.0f;                     // 4 (Degrees)
  float softness = 0.0f;                        // 4
  float type = 0.0f; // 4 (0=Fan/Sector, 1=Annulus/Circle, 2=Beam)

  GPUSkillEffect() = default;
};

// Ensure Stride is exactly 64 bytes
static_assert(
    sizeof(GPUSkillEffect) == 64,
    "GPUSkillEffect struct must be exactly 64 bytes for SSBO alignment");

/**
 * @brief Structure for GPU HoloBlade instances.
 * 48 Bytes (16 * 3) for alignment.
 */
struct HoloBladeInstance {
  Vector2 position = {0.0f, 0.0f};              // 8
  float rotation = 0.0f;                        // 4
  float scale = 1.0f;                           // 4
  Vector4 holoColor = {1.0f, 1.0f, 1.0f, 1.0f}; // 16
  float rimStrength = 0.0f;                     // 4
  float noiseSpeed = 0.0f;                      // 4
  float padding[2] = {0.0f, 0.0f};              // 8

  HoloBladeInstance() = default;
};

static_assert(sizeof(HoloBladeInstance) == 48,
              "HoloBladeInstance struct must be 48 bytes for SSBO alignment");

/**
 * @brief Structure for GPU damage popups.
 * 48 Bytes (16 * 3) for alignment.
 */
struct GPUPopupInstance {
  Vector2 position = {0.0f, 0.0f};   // 8
  Vector2 velocity = {0.0f, 0.0f};   // 8
  float timer = 0.0f;                // 4
  float lifeTime = 1.0f;             // 4
  uint32_t glyphData = 0;            // 4 - Packed: startIdx << 16 | count
  uint32_t colorPacked = 0xFFFFFFFF; // 4 - RGBA8
  uint32_t flags = 0;                // 4 - bit0: isCrit, bit1: isStatus
  float scale = 1.0f;                // 4
  float padding[2] = {0.0f, 0.0f};   // 8

  GPUPopupInstance() = default;
};

static_assert(sizeof(GPUPopupInstance) == 48,
              "GPUPopupInstance struct must be 48 bytes for SSBO alignment");

/**
 * @brief Structure for GPU Visual Stats (Attribute Sync).
 * STRICTLY 64 BYTES (16 * 4) for alignment.
 * Used for dynamic visual effects based on character stats (glow, speed, etc).
 */
struct GPUVisualStats {
  float weaponDamage = 0.0f;             // 4
  float attackSpeed = 1.0f;              // 4
  float critChance = 0.0f;               // 4
  float critDamage = 0.0f;               // 4
  float defenseRating = 0.0f;            // 4 (Armor/Evasion mix)
  float statusStrength = 0.0f;           // 4 (Elemental intensity)
  float glowIntensity = 0.0f;            // 4
  uint32_t glowColorPacked = 0xFFFFFFFF; // 4
  
  uint32_t activeStatusMask = 0;         // 4 bytes - Bitmask of active status effects
  float statusTimer = 0.0f;              // 4 bytes - Animation phase [0, 1]
  float padding[6] = {0.0f};             // 24 bytes padding to reach 64

  GPUVisualStats() = default;
};

// Ensure Stride is exactly 64 bytes
static_assert(
    sizeof(GPUVisualStats) == 64,
    "GPUVisualStats struct must be exactly 64 bytes for SSBO alignment");

/**
 * @brief Structure for GPU Item Label instances (Instanced UI).
 * STRICTLY 64 BYTES (16 * 4) for alignment.
 */
struct GPULabelInstance {
  Vector2 position = {0.0f, 0.0f};      // 8  - Screen/World coords (Top-Left or Center)
  Vector2 size = {0.0f, 0.0f};          // 8  - Width, Height
  Vector4 bgColor = {0, 0, 0, 0};       // 16 - Background Color (RGBA)
  Vector4 borderColor = {0, 0, 0, 0};   // 16 - Border Color (RGBA)
  float borderWidth = 0.0f;             // 4  - Border width in pixels
  float cornerRadius = 0.0f;            // 4  - Radius in pixels
  float padding[2] = {0.0f, 0.0f};      // 8  - Padding to 64 bytes
  
  GPULabelInstance() = default;
};

// Static assert for alignment safety
static_assert(sizeof(GPULabelInstance) == 64, "GPULabelInstance must be 64 bytes");

/**
 * @brief Structure for individual GPU glyph instances (Text Rendering).
 * 48 Bytes (16 * 3) for alignment.
 */
struct GPUGlyphInstance {
  Vector2 position = {0.0f, 0.0f};      // 8  - Screen/World position
  Vector2 size = {0.0f, 0.0f};          // 8  - Glyph size in pixels
  Vector2 uvMin = {0.0f, 0.0f};         // 8  - Top-left UV
  Vector2 uvMax = {0.0f, 0.0f};         // 8  - Bottom-right UV
  uint32_t colorPacked = 0xFFFFFFFF;    // 4  - RGBA8 packed color
  float scale = 1.0f;                   // 4  - Scale factor
  float padding[2] = {0.0f};            // 8  - Padding to 48 bytes
  
  GPUGlyphInstance() = default;
};

// Ensure Stride is exactly 48 bytes
static_assert(sizeof(GPUGlyphInstance) == 48,
              "GPUGlyphInstance struct must be exactly 48 bytes for SSBO alignment");

/**
 * @brief Centralized Color Manager for VFX
 * 颜色管理器：统一管理游戏内的特效颜色
 */
namespace Colors {
// Defines a color in 0xRRGGBBAA format for easy hex usage
// 使用 0xRRGGBBAA 格式定义的颜色辅助函数
constexpr Color FromHex(uint32_t hex) {
  return Color{static_cast<unsigned char>((hex >> 24) & 0xFF),
               static_cast<unsigned char>((hex >> 16) & 0xFF),
               static_cast<unsigned char>((hex >> 8) & 0xFF),
               static_cast<unsigned char>((hex) & 0xFF)};
}

// --- Rarity Colors (物品稀有度) ---

constexpr Color RARITY_COMMON = {180, 180, 180, 255}; // Common / 普通
constexpr Color RARITY_UNCOMMON = {100, 255, 100,
                                   255}; // Uncommon / 优秀 (Lime/Green)
constexpr Color RARITY_MAGIC = {60, 130, 255, 255};    // Magic / 魔法
constexpr Color RARITY_RARE = {255, 220, 0, 255};      // Rare / 稀有
constexpr Color RARITY_SET = {0, 255, 0, 255};         // Set / 套装 (Green)
constexpr Color RARITY_EPIC = {190, 60, 255, 255};     // Epic / 史诗
constexpr Color RARITY_LEGENDARY = {255, 140, 0, 255}; // Legendary / 传说
constexpr Color RARITY_MYTHIC = {255, 40, 40, 255};    // Mythic / 神话
constexpr Color RARITY_ANCIENT = {230, 0, 0, 255};     // Ancient / 远古

// --- Elemental Colors (元素属性) ---

// Fire / 火焰
constexpr Color ELEM_FIRE = {255, 80, 30, 255};
// Cold / 冰霜
constexpr Color ELEM_COLD = {80, 180, 255, 255};
// Lightning / 闪电
constexpr Color ELEM_LIGHTNING = {255, 255, 100, 255};
// Poison / 毒素
constexpr Color ELEM_POISON = {100, 255, 60, 255};
// Shadow / 暗影
constexpr Color ELEM_SHADOW = {130, 50, 200, 255};
// Void / 虚空
constexpr Color ELEM_VOID = {40, 10, 60, 255};

// --- UI & Status Colors (UI 与 状态) ---

// Health Bar / 生命值
constexpr Color UI_HEALTH = {230, 40, 40, 255};
// Mana Bar / 法力值
constexpr Color UI_MANA = {40, 100, 230, 255};
// Gold / 金币
constexpr Color UI_GOLD = {255, 215, 0, 255};
// Experience / 经验值
constexpr Color UI_XP = {100, 200, 255, 255};

// --- UI Control & Edge Colors (UI 控件与边缘) ---

// Default Border / 默认边框
constexpr Color UI_BORDER_DEFAULT = {80, 80, 90, 255};
// Hovered Border / 悬停边框
constexpr Color UI_BORDER_HOVER = {120, 120, 140, 255};
// Active or Selected Border / 激活或选中边框
constexpr Color UI_BORDER_ACTIVE = {255, 215, 0, 255};
// Disabled Border / 禁用边框
constexpr Color UI_BORDER_DISABLED = {50, 50, 55, 255};
// Danger or Error Border / 危险或错误边框
constexpr Color UI_BORDER_DANGER = {220, 60, 60, 255};
// Success Border / 成功边框
constexpr Color UI_BORDER_SUCCESS = {60, 220, 60, 255};
// Info or Hint Border / 信息或提示边框
constexpr Color UI_BORDER_INFO = {60, 160, 255, 255};
// Color for socket info in tooltips
// Color for socket info in tooltips
constexpr Color COLOR_SOCKET_INFO = {208, 239, 232, 255};

// --- Modern UI Theme Colors (Standardized) ---
// Panels
constexpr Color UI_BACKGROUND = {35, 35, 45, 180};         // Panel Background
constexpr Color UI_BORDER = {70, 70, 85, 255};             // Panel Border
constexpr Color UI_BORDER_HIGHLIGHT = {200, 170, 50, 255}; // Gold Highlight
constexpr Color UI_SLOT_BG = {25, 25, 35, 200};            // Slot Background

// Text Resources
constexpr Color TEXT_PRIMARY = {245, 245, 245, 255};
constexpr Color TEXT_SECONDARY = {180, 180, 180, 255};
constexpr Color TEXT_HIGHLIGHT = {255, 215, 0, 255};

// Buttons
constexpr Color BUTTON_NORMAL = {50, 50, 65, 255};
constexpr Color BUTTON_HOVER = {70, 70, 95, 255};
constexpr Color BUTTON_PRESS = {40, 40, 55, 255};

// Status / Feedback
constexpr Color STATUS_DANGER = {200, 50, 50, 255};  // Red/Danger
constexpr Color STATUS_SUCCESS = {50, 200, 50, 255}; // Green/Success
constexpr Color STATUS_INFO = {60, 160, 255, 255};   // Blue/Info

// --- Main Menu / Pause Menu Colors ---
constexpr Color MENU_BTN_TEXT_NORMAL = {220, 230, 225, 255};      // Silk White (帛白) - High contrast on dark background
constexpr Color MENU_BTN_TEXT_HOVER = {255, 100, 80, 255};       // Bright Cinnabar (鲜朱砂) - Interact feedback
constexpr Color MENU_BTN_TEXT_PRESS = {160, 40, 30, 255};       // Deep Cinnabar (暗朱砂) - Strong feedback

// --- Blade Ascendant Theme (剑修主题) ---

// Low opacity trail color (Very faint water/ink)
// 极淡的水墨色拖尾 (高透明度)
constexpr Color INK_TRAIL_PALE = {180, 220, 235, 40};

// Deep ink for impact/core visuals
// 深色水墨，用于打击核心或强调
constexpr Color INK_DEEP = {20, 25, 35, 220};

// Standard Blade Cyan (The energy color)
// 标准剑气天青色
constexpr Color BLADE_CYAN = {195, 248, 245, 255};

// Speed Line / Particle bright accent
// 速度线/粒子的高亮色
constexpr Color SPEED_ACCENT = {200, 255, 255, 200};

// Ink Black (#1A1A1A)
// 水墨黑 (设计规范色)
constexpr Color INK_BLACK = {26, 26, 26, 255};

// Pure White Highlight
// 纯白高光
constexpr Color BLADE_WHITE = {255, 255, 255, 255};

// Spirit Blade (Translucent blue-white for Blade Formation)
// 灵体飞剑 (半透明蓝白 - 灵剑决)
constexpr Color SPIRIT_BLADE = {200, 230, 255, 160};

// Mind Blade Core (High-bright line)
// 心剑核心 (高亮核心线 - 心剑·无影)
constexpr Color MIND_BLADE_CORE = {240, 255, 255, 255};

// Ink Silhouette (Pure black for Phantom Flash)
// 水墨残影 (纯黑墨迹 - 绝影闪)
constexpr Color INK_SILHOUETTE = {10, 10, 10, 230};

// --- Map Affix Colors (地图词缀颜色) ---


// Correcting typos for easier use
constexpr Color MAP_AFFIX_POSITIVE = {100, 255, 100, 255};
constexpr Color MAP_AFFIX_NEGATIVE = {255, 100, 100, 255};
} // namespace Colors

} // namespace NoMoreDay::components

#endif // NOMOREDAY_GPUDATA_HPP
