#ifndef NOMOREDAY_GPUDATA_HPP
#define NOMOREDAY_GPUDATA_HPP

#pragma once
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <stdint.h>
#include <type_traits>

namespace NoMoreDay::render::abi {
inline constexpr uint32_t GPU_ABI_VERSION = 5;
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

constexpr int MAX_LIGHTS = 4096;
}

namespace NoMoreDay::render::skillfx {
inline constexpr uint32_t kElementBits = 4u;
inline constexpr uint32_t kElementMask = (1u << kElementBits) - 1u; // low 4 bits
inline constexpr uint32_t kSkillIdShift = kElementBits;
inline constexpr uint32_t kSkillIdBits = 8u;
inline constexpr uint32_t kSkillIdMask = ((1u << kSkillIdBits) - 1u) << kSkillIdShift;

inline constexpr uint32_t PackSkillEffectFlags(const uint8_t elementType,
                                               const uint32_t skillId) {
  const uint32_t e = static_cast<uint32_t>(elementType) & kElementMask;
  const uint32_t s = (skillId << kSkillIdShift) & kSkillIdMask;
  return e | s;
}

inline constexpr uint8_t UnpackSkillEffectElementType(const uint32_t flags) {
  return static_cast<uint8_t>(flags & kElementMask);
}

inline constexpr uint32_t UnpackSkillEffectSkillId(const uint32_t flags) {
  return (flags & kSkillIdMask) >> kSkillIdShift;
}
} // namespace NoMoreDay::render::skillfx

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
  AreaLight = 3,
  LineLight = 4,
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
  float dirX = 1.0f;
  float dirY = 0.0f;
  float spotCosHalfAngle = -1.0f;
  float spotOuterCos = -1.0f;
  uint32_t lightType = static_cast<uint32_t>(LightType::PointLight);
  uint32_t shadowMapIndex = 0u;
  uint32_t priority = 0u;
  uint32_t flags = 0u;
};

static_assert(sizeof(GPULight) == 64,
              "GPULight struct must be exactly 64 bytes for SSBO alignment");

// V3 Baseline ABI placeholders (Step A)
struct GPUShadowCaster {
  float posX = 0.0f;
  float posY = 0.0f;
  float radius = 0.0f;
  float occluderHeight = 0.0f;
  uint32_t shapeIndex = 0;
  uint32_t dynamicFlag = 0;
  uint32_t reserved0 = 0;
  uint32_t reserved1 = 0;
};
static_assert(std::is_standard_layout_v<GPUShadowCaster>,
              "GPUShadowCaster must be standard layout");
static_assert(sizeof(GPUShadowCaster) == 32,
              "GPUShadowCaster struct must be exactly 32 bytes");
static_assert(alignof(GPUShadowCaster) == alignof(float),
              "GPUShadowCaster alignment mismatch");

struct GPUShadowLight {
  uint32_t lightId = 0;
  uint32_t shadowType = 0;
  uint32_t reserved0 = 0;
  uint32_t reserved1 = 0;
  float atlasRect[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float penumbraParams[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};
static_assert(std::is_standard_layout_v<GPUShadowLight>,
              "GPUShadowLight must be standard layout");
static_assert(sizeof(GPUShadowLight) == 48,
              "GPUShadowLight struct must be exactly 48 bytes");
static_assert(alignof(GPUShadowLight) == alignof(float),
              "GPUShadowLight alignment mismatch");

struct GPUShadowAtlasMeta {
  uint32_t tileIndex = 0;
  uint32_t lastUsedFrame = 0;
  float priorityScore = 0.0f;
  float occupancy = 0.0f;
};
static_assert(std::is_standard_layout_v<GPUShadowAtlasMeta>,
              "GPUShadowAtlasMeta must be standard layout");
static_assert(sizeof(GPUShadowAtlasMeta) == 16,
              "GPUShadowAtlasMeta struct must be exactly 16 bytes");
static_assert(alignof(GPUShadowAtlasMeta) == alignof(float),
              "GPUShadowAtlasMeta alignment mismatch");

struct GPUClusterHeader {
  uint32_t offset = 0;
  uint32_t pointCount = 0;
  uint32_t spotCount = 0;
  uint32_t areaCount = 0;
};
static_assert(std::is_standard_layout_v<GPUClusterHeader>,
              "GPUClusterHeader must be standard layout");
static_assert(sizeof(GPUClusterHeader) == 16,
              "GPUClusterHeader struct must be exactly 16 bytes");
static_assert(alignof(GPUClusterHeader) == alignof(uint32_t),
              "GPUClusterHeader alignment mismatch");

struct GPUClusterCounters {
  uint32_t writeCursor = 0;
  uint32_t overflowPoint = 0;
  uint32_t overflowSpot = 0;
  uint32_t overflowArea = 0;
  uint32_t overflowLine = 0;
  uint32_t reserved0 = 0;
  uint32_t reserved1 = 0;
  uint32_t reserved2 = 0;
};
static_assert(std::is_standard_layout_v<GPUClusterCounters>,
              "GPUClusterCounters must be standard layout");
static_assert(sizeof(GPUClusterCounters) == 32,
              "GPUClusterCounters struct must be exactly 32 bytes");
static_assert(alignof(GPUClusterCounters) == alignof(uint32_t),
              "GPUClusterCounters alignment mismatch");

struct GPUClusterLightIndex {
  uint32_t lightIndex = 0;
};
static_assert(std::is_standard_layout_v<GPUClusterLightIndex>,
              "GPUClusterLightIndex must be standard layout");
static_assert(sizeof(GPUClusterLightIndex) == 4,
              "GPUClusterLightIndex struct must be exactly 4 bytes");
static_assert(alignof(GPUClusterLightIndex) == alignof(uint32_t),
              "GPUClusterLightIndex alignment mismatch");

struct GPUClusterPackedLight {
  float posX = 0.0f;
  float posY = 0.0f;
  float radius = 0.0f;
  float invRadiusSq = 0.0f;
  float colorTimesIntensityR = 0.0f;
  float colorTimesIntensityG = 0.0f;
  float colorTimesIntensityB = 0.0f;
  float spotCosHalfAngle = -1.0f;
  float spotOuterCos = -1.0f;
  float dirX = 1.0f;
  float dirY = 0.0f;
  uint32_t lightType = 0;
  uint32_t shadowMapIndex = 0u;
  uint32_t flags = 0u;
  uint32_t reserved0 = 0u;
  uint32_t reserved1 = 0u;
};
static_assert(std::is_standard_layout_v<GPUClusterPackedLight>,
              "GPUClusterPackedLight must be standard layout");
static_assert(sizeof(GPUClusterPackedLight) == 64,
              "GPUClusterPackedLight struct must be exactly 64 bytes");
static_assert(alignof(GPUClusterPackedLight) == alignof(float),
              "GPUClusterPackedLight alignment mismatch");

struct GPULightBounds {
  Vector2 minXY = {0.0f, 0.0f};
  Vector2 maxXY = {0.0f, 0.0f};
  float minLayer = 0.0f;
  float maxLayer = 0.0f;
  uint32_t lightIndex = 0;
  uint32_t reserved = 0;
};
static_assert(std::is_standard_layout_v<GPULightBounds>,
              "GPULightBounds must be standard layout");
static_assert(sizeof(GPULightBounds) == 32,
              "GPULightBounds struct must be exactly 32 bytes");
static_assert(alignof(GPULightBounds) == alignof(float),
              "GPULightBounds alignment mismatch");

struct alignas(16) GPUMaterialDataV3 {
  Vector4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
  Vector4 emissiveAndIntensity = {0.0f, 0.0f, 0.0f, 0.0f};
  Vector4 pbrParams = {0.6f, 0.0f, 1.0f, 0.0f};
  Vector4 textureSlots = {-1.0f, -1.0f, -1.0f, -1.0f};
  Vector4 fresnelControl = {0.04f, 0.3f, 0.1f, 0.0f};
  Vector4 uvParams = {1.0f, 1.0f, 0.0f, 0.0f};
  Vector4 reserved0 = {0.0f, 0.0f, 0.0f, 0.0f};
  Vector4 reserved1 = {0.0f, 0.0f, 0.0f, 0.0f};
};
static_assert(std::is_standard_layout_v<GPUMaterialDataV3>,
              "GPUMaterialDataV3 must be standard layout");
static_assert(sizeof(GPUMaterialDataV3) == 128,
              "GPUMaterialDataV3 struct must be exactly 128 bytes");
static_assert(alignof(GPUMaterialDataV3) == 16,
              "GPUMaterialDataV3 alignment must be 16 bytes");

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
  uint32_t flags = 0u; // 4 (low 4 bits: elementType, next 8 bits: skillId)
  float type = 0.0f; // 4 (0=Fan,1=Disc,2=Blade,3=Crescent,4=Ring,5=EllipseRing)

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
 * @brief GPU text command from CPU-side event collection.
 * 16 bytes, V4 text pipeline command input.
 */
struct GPUTextCommand {
  float worldPosX = 0.0f;          // 4
  float worldPosY = 0.0f;          // 4
  uint32_t stringId = 0;           // 4
  uint32_t colorAndFlags = 0;      // 4
};

static_assert(std::is_standard_layout_v<GPUTextCommand>,
              "GPUTextCommand must be standard layout");
static_assert(sizeof(GPUTextCommand) == 16,
              "GPUTextCommand struct must be exactly 16 bytes");

/**
 * @brief GPU glyph metric table entry for text layout compute.
 * 40 bytes, tightly packed scalar payload for ABI stability.
 */
struct GPUGlyphMetrics {
  float uvMinX = 0.0f;             // 4
  float uvMinY = 0.0f;             // 4
  float uvMaxX = 0.0f;             // 4
  float uvMaxY = 0.0f;             // 4
  float offsetX = 0.0f;            // 4
  float offsetY = 0.0f;            // 4
  float sizeX = 0.0f;              // 4
  float sizeY = 0.0f;              // 4
  float advance = 0.0f;            // 4
  float reserved = 0.0f;           // 4
};

static_assert(std::is_standard_layout_v<GPUGlyphMetrics>,
              "GPUGlyphMetrics must be standard layout");
static_assert(sizeof(GPUGlyphMetrics) == 40,
              "GPUGlyphMetrics struct must be exactly 40 bytes");

/**
 * @brief GPU text quad generated by layout pass and consumed by draw pass.
 * 40 bytes.
 */
struct GPUTextQuad {
  float screenPosX = 0.0f;         // 4
  float screenPosY = 0.0f;         // 4
  float sizeX = 0.0f;              // 4
  float sizeY = 0.0f;              // 4
  float uvMinX = 0.0f;             // 4
  float uvMinY = 0.0f;             // 4
  float uvMaxX = 0.0f;             // 4
  float uvMaxY = 0.0f;             // 4
  uint32_t colorPacked = 0;        // 4
  float opacity = 1.0f;            // 4
};

static_assert(std::is_standard_layout_v<GPUTextQuad>,
              "GPUTextQuad must be standard layout");
static_assert(sizeof(GPUTextQuad) == 40,
              "GPUTextQuad struct must be exactly 40 bytes");

/**
 * @brief GPU loot instance payload for V4 loot rendering path.
 * 32 bytes.
 */
struct GPULootInstance {
  float worldPosX = 0.0f;         // 4
  float worldPosY = 0.0f;         // 4
  float labelOffsetX = 0.0f;      // 4
  float labelOffsetY = -24.0f;    // 4
  uint32_t itemId = 0;            // 4
  uint32_t rarityColor = 0;       // 4 (RGBA8 packed)
  float glowIntensity = 0.0f;     // 4
  uint32_t flags = 0;             // 4
};

static_assert(std::is_standard_layout_v<GPULootInstance>,
              "GPULootInstance must be standard layout");
static_assert(sizeof(GPULootInstance) == 32,
              "GPULootInstance struct must be exactly 32 bytes");

/**
 * @brief Emissive material stamp projection payload (CPU-side span DTO).
 *
 * Produced by the Game-side EmissiveStampAdapter from the ECS
 * (Position + ActiveMaterialSwap, KilledTag-excluded, resolved via
 * MaterialManager) and consumed by RadianceCascadesPass::RunMaterialEmissive,
 * which keeps the world->pixel conversion and per-stamp compute dispatch.
 */
struct EmissiveStampInput {
  Vector2 worldPos = {0.0f, 0.0f};    // world-space center
  float worldHalfExtent = 0.0f;       // max(Radius, sprite half-extent, 24)
  int maskLayer = 0;                  // material mask texture-array layer
  Vector4 emissionRGBA = {0.0f, 0.0f, 0.0f, 0.0f}; // RGB + intensity
};

/**
 * @brief V5 radiance cascade runtime configuration payload.
 * 32 bytes.
 */
struct RadianceCascadeConfig {
  uint32_t numLevels = 0;
  uint32_t raysPerProbe = 0;
  float baseInterval = 0.0f;
  float temporalWeight = 0.9f;
  uint32_t halfResolution = 0;
  uint32_t sdfUpdateInterval = 1;
  float giIntensity = 1.0f;
  uint32_t reserved = 0;
};

static_assert(std::is_standard_layout_v<RadianceCascadeConfig>,
              "RadianceCascadeConfig must be standard layout");
static_assert(sizeof(RadianceCascadeConfig) == 32,
              "RadianceCascadeConfig struct must be exactly 32 bytes");

/**
 * @brief V5 SPH particle payload.
 * 48 bytes.
 */
struct GPUFluidParticle {
  Vector2 position = {0.0f, 0.0f};   // 8
  Vector2 velocity = {0.0f, 0.0f};   // 8
  float density = 0.0f;              // 4
  float pressure = 0.0f;             // 4
  Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // 16
  float lifetime = 0.0f;             // 4
  uint32_t flags = 0;                // 4
};

static_assert(std::is_standard_layout_v<GPUFluidParticle>,
              "GPUFluidParticle must be standard layout");
static_assert(sizeof(GPUFluidParticle) == 48,
              "GPUFluidParticle struct must be exactly 48 bytes");

/**
 * @brief V5 SPH runtime config payload.
 * 32 bytes.
 */
struct GPUFluidConfig {
  float smoothingRadius = 0.0f;  // 4
  float restDensity = 0.0f;      // 4
  float stiffness = 0.0f;        // 4
  float viscosity = 0.0f;        // 4
  Vector2 gravity = {0.0f, -9.8f}; // 8
  float surfaceTension = 0.0f;   // 4
  uint32_t maxParticles = 0;     // 4
};

static_assert(std::is_standard_layout_v<GPUFluidConfig>,
              "GPUFluidConfig must be standard layout");
static_assert(sizeof(GPUFluidConfig) == 32,
              "GPUFluidConfig struct must be exactly 32 bytes");

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
