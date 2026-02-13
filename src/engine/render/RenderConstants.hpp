#pragma once
#include <cstdint>

namespace NoMoreDay {
namespace RenderConstants {

/**
 * @brief SSBO Binding Point 索引 (全局渲染管线)。
 *
 * 这些绑定点用于 MDI 渲染管线的持久/共享 Buffer。
 * 所有 GPU Buffer 绑定必须使用此枚举，严禁使用字面量。
 * 对应 GLSL: layout(std430, binding = X) buffer ...
 *
 * @note Compute Shader 的本地临时绑定 (如粒子系统、流场) 使用独立的枚举，
 *       因为这些系统在执行时独占绑定点，不与其他系统共享。
 */
enum class Binding : uint32_t {
  // === MDI Entity Rendering Pipeline ===
  SSBO_ENTITY_DATA = 0,  // GPUEntity 物理/变换数据 (GPUEntitySystem)
  SSBO_VISIBLE_ID = 1,   // 视锥剔除后的可见实体索引 (MDIRenderer)
  SSBO_COMMAND = 2,      // Indirect Draw Command (MDIRenderer)
  SSBO_VISUAL_STATS = 3, // 视觉属性 (Glow, Status Effects)

  // === Label / Beam / Text Rendering ===
  SSBO_LABEL_INSTANCE = 4, // Label 实例数据 (RenderSystem)
  SSBO_BEAM_INSTANCE = 5,  // Beam 实例数据 (RenderSystem)

  // === VFX Systems ===
  SSBO_SKILL_EFFECTS = 6, // 技能特效实例 (GPUSkillEffectSystem)
  SSBO_POPUP_DATA = 7,    // 伤害数字弹出 (PopupRenderer)
  SSBO_GLYPH_INSTANCE = 8, // Glyph 实例数据 (文字批量渲染)

  // === Lighting System (Phase 2) ===
  SSBO_LIGHT_DATA = 9, // GPULight SSBO (LightingPass)

  // === Trail System (Phase 3) ===
  SSBO_TRAIL_HEADERS = 10,
  SSBO_TRAIL_POINTS = 11,

  // === Material / Distortion (Phase 4) ===
  SSBO_MATERIAL_DATA = 12,
  SSBO_DISTORTION_DATA = 13,

  // === Reserved ===
  SSBO_RESERVED_10 = SSBO_TRAIL_HEADERS,
  SSBO_RESERVED_11 = SSBO_TRAIL_POINTS,
  SSBO_RESERVED_12 = SSBO_MATERIAL_DATA,
  SSBO_RESERVED_13 = SSBO_DISTORTION_DATA,
  SSBO_RESERVED_14 = 14,
  SSBO_RESERVED_15 = 15, // OpenGL 4.3 最低保证 16 个 SSBO Binding
};

/**
 * @brief Compute Shader 本地 SSBO Binding (粒子系统)。
 *
 * 这些是临时绑定点，仅在粒子系统 Compute Shader 执行期间有效。
 * 必须与 particle.compute / particle_emit.compute 中的 binding 保持一致。
 */
namespace ParticleCS {
constexpr uint32_t PARTICLES_IN = 0;  // 输入粒子数组
constexpr uint32_t PARTICLES_OUT = 1; // 输出粒子数组 (Compact)
constexpr uint32_t INDIRECT_CMD = 2;  // DrawIndirect Command
constexpr uint32_t ATOMIC_COUNT = 3;  // 原子计数器
constexpr uint32_t FORCE_FIELDS = 4;  // ForceField SSBO (readonly)
constexpr uint32_t SUB_EMISSION = 5;  // Sub-emission buffer
constexpr uint32_t SUB_EMIT_COUNTER = 6; // Sub-emission atomic counter
} // namespace ParticleCS

namespace TrailBinding {
constexpr uint32_t HEADERS = 10;
constexpr uint32_t POINTS = 11;
} // namespace TrailBinding

/**
 * @brief Compute Shader 本地 SSBO Binding (流场系统)。
 *
 * 这些是临时绑定点，仅在流场 Compute Shader 执行期间有效。
 * 必须与 flow_*.compute 中的 binding 保持一致。
 */
namespace FlowFieldCS {
constexpr uint32_t COST_FIELD = 0;        // 代价场
constexpr uint32_t INTEGRATION_READ = 1;  // 积分场 (读)
constexpr uint32_t FLOW_FIELD = 2;        // 流向量场
constexpr uint32_t DENSITY_FIELD = 3;     // 密度场
constexpr uint32_t INTEGRATION_WRITE = 4; // 积分场 (写, Ping-Pong)
} // namespace FlowFieldCS

/**
 * @brief Compute Shader 本地 SSBO Binding (Cull / Physics)。
 *
 * 用于 cull.compute 和 physics.compute。
 */
namespace EntityCS {
constexpr uint32_t ENTITY_DATA =
    0; // GPUEntity 数组 (对应 Binding::SSBO_ENTITY_DATA)
constexpr uint32_t VISIBLE_IDS = 1;  // 可见索引 (对应 Binding::SSBO_VISIBLE_ID)
constexpr uint32_t INDIRECT_CMD = 2; // Indirect Draw Command
constexpr uint32_t VISUAL_STATS = 3; // 可视化状态
} // namespace EntityCS

/**
 * @brief Uniform Block Binding Point 索引 (UBO)。
 */
enum class UBOBinding : uint32_t {
  UBO_GLOBAL_PARAMS = 0, // 全局参数 (时间、相机等)
  UBO_LIGHTING = 1,      // 光照参数 (Reserved)
};

/**
 * @brief Texture Unit 索引。
 */
enum class TextureUnit : uint32_t {
  TEX_ENTITY_ARRAY = 0,   // Entity Texture2DArray
  TEX_PARTICLE_ATLAS = 1, // Particle Atlas
  TEX_SKILL_SDF = 2,      // Skill SDF Texture
  TEX_FONT_ATLAS = 3,     // Font Atlas for Popups
};

/**
 * @brief Memory Barrier 类型。
 *
 * 可用 `|` 组合。
 */
enum class Barrier : uint32_t {
  None = 0,
  SSBO = 0x00002000,    // GL_SHADER_STORAGE_BARRIER_BIT
  Command = 0x00000040, // GL_COMMAND_BARRIER_BIT
  Buffer = 0x00000200,  // GL_BUFFER_UPDATE_BARRIER_BIT
  Image = 0x00000020,   // GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
  Client = 0x00004000,  // GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT

  // 常用组合
  All = SSBO | Command | Buffer,
};

constexpr Barrier operator|(Barrier a, Barrier b) {
  return static_cast<Barrier>(static_cast<uint32_t>(a) |
                              static_cast<uint32_t>(b));
}

/**
 * @brief Buffer Map 标志位。
 */
enum class MapFlag : uint32_t {
  Read = 0x0001,          // GL_MAP_READ_BIT
  Write = 0x0002,         // GL_MAP_WRITE_BIT
  Persistent = 0x0040,    // GL_MAP_PERSISTENT_BIT
  Coherent = 0x0080,      // GL_MAP_COHERENT_BIT
  FlushExplicit = 0x0010, // GL_MAP_FLUSH_EXPLICIT_BIT
};

inline MapFlag operator|(MapFlag a, MapFlag b) {
  return static_cast<MapFlag>(static_cast<uint32_t>(a) |
                              static_cast<uint32_t>(b));
}

/**
 * @brief 同步对象标志位。
 */
enum class SyncFlag : uint32_t {
  FlushCommands = 0x00000001, // GL_SYNC_FLUSH_COMMANDS_BIT
};

/**
 * @brief OpenGL 通用常量。
 */
namespace GL {
constexpr uint32_t DRAW_INDIRECT_BUFFER = 0x8F3F;
constexpr uint32_t SHADER_STORAGE_BUFFER = 0x90D2;
constexpr uint32_t SYNC_GPU_COMMANDS_COMPLETE = 0x9117;
constexpr uint32_t TEXTURE_2D = 0x0DE1;
constexpr uint32_t TEXTURE_2D_ARRAY = 0x8C1A;
constexpr uint32_t TEXTURE0 = 0x84C0;
constexpr uint32_t TRIANGLES = 0x0004;
constexpr uint32_t TRIANGLE_STRIP = 0x0005;
} // namespace GL

inline constexpr uint32_t ToGL(MapFlag b) { return static_cast<uint32_t>(b); }
inline constexpr uint32_t ToGL(SyncFlag b) { return static_cast<uint32_t>(b); }
inline constexpr uint32_t ToGL(Barrier b) { return static_cast<uint32_t>(b); }

// === Convenience Constants ===
namespace GPU {
constexpr int MAX_ENTITIES = 200000;
constexpr int MAX_PARTICLES = 200000;
constexpr int MAX_SKILL_EFFECTS = 1024;
constexpr int MAX_POPUPS = 2048;
constexpr int MAX_GLYPHS = 4096; // 批量文字渲染上限
} // namespace GPU

} // namespace RenderConstants
} // namespace NoMoreDay
