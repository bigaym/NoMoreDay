#version 430 core
layout(location = 0) in vec2 aPos;

struct InstanceData {
    vec2 position;
    vec2 prevPosition;
    vec2 velocity;
    float radius;
    int type;
    uint flags;
    uint frameId;
    float padding[6];
};

// Binding 来源: RenderConstants::Binding::SSBO_ENTITY_DATA (0)
layout(std430, binding = 0) readonly buffer Entities { InstanceData entities[]; };
// Binding 来源: RenderConstants::Binding::SSBO_VISIBLE_ID (1)
layout(std430, binding = 1) readonly buffer VisibleIndices { uint visibleIndices[]; };

struct GPUVisualStats {
    float weaponDamage;
    float attackSpeed;
    float critChance;
    float critDamage;
    float defenseRating;
    float statusStrength;
    float glowIntensity;
    uint glowColorPacked;
    uint activeStatusMask;
    float statusTimer;
    float padding[6];
};
// Binding 来源: RenderConstants::Binding::SSBO_VISUAL_STATS (3)
layout(std430, binding = 3) readonly buffer StatsBuffer { GPUVisualStats stats[]; };

uniform mat4 viewProj;
uniform float interpolationFactor;
uniform float uTime;

out vec2 vTexCoord;
out vec2 vLocalPos;
flat out int vTextureIndex;
flat out uint vFlags;
flat out float vGlow;
flat out uint vStatusMask;
flat out float vTime;

void main() {
    // 从剔除后的索引缓冲中获取真正的实体 ID
    uint entityId = visibleIndices[gl_InstanceID];
    InstanceData e = entities[entityId];
    GPUVisualStats s = stats[entityId];
    
    // 插值位置
    vec2 interpolatedPos = mix(e.prevPosition, e.position, interpolationFactor);
    
    // 朝向计算
    float rotation = 0.0;
    // Check GPU_ENTITY_FLAG_NO_ROTATION (1 << 3)
    if ((e.flags & 8u) == 0u) {
        if (length(e.velocity) > 0.1) {
            rotation = atan(e.velocity.y, e.velocity.x);
        }
    }
    
    float c = cos(rotation);
    float s_rot = sin(rotation);
    mat2 rot = mat2(c, -s_rot, s_rot, c);
    
    // 渲染尺寸：物理半径 * 4
    float renderRadius = e.radius * 4.0;
    vec2 pos = aPos * (renderRadius * 2.0);
    pos = rot * pos;
    vec2 worldPos = interpolatedPos + pos;
    
    gl_Position = viewProj * vec4(worldPos, 0.0, 1.0);
    
    vTexCoord = aPos + 0.5;
    vLocalPos = aPos * 2.0;
    vTextureIndex = e.type;
    vFlags = e.flags;
    vGlow = s.glowIntensity;
    vStatusMask = s.activeStatusMask;
    vTime = uTime;
}
