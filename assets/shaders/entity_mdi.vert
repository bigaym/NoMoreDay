#version 430 core
layout(location = 0) in vec2 aPos; // [-0.5, 0.5]

// Must match GPUEntity in GPUData.hpp (64 bytes)
struct InstanceData {
    vec2 position;     // 8  - Current physics position
    vec2 prevPosition; // 8  - Previous frame position (for render interpolation)
    vec2 velocity;     // 8  - Current velocity
    float radius;      // 4  - Collision radius
    uint type;         // 4  - Entity type
    uint flags;        // 4  - Behavior flags
    float padding[7];  // 28 - Padding to 64 bytes
};

// Bindings match Cull/MDIRenderer
layout(std430, binding = 0) readonly buffer Entities { InstanceData entities[]; };
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
    float padding[8];
};
layout(std430, binding = 3) readonly buffer StatsBuffer { GPUVisualStats stats[]; };

uniform mat4 viewProj;
uniform float interpolationFactor; // Alpha factor [0, 1] for smooth interpolation between physics frames

out vec2 vTexCoord;
out vec2 vLocalPos;
flat out uint vTextureIndex;
flat out uint vFlags;
flat out float vGlow;

void main() {
    uint entityId = visibleIndices[gl_InstanceID];
    InstanceData e = entities[entityId];
    
    // [RENDER INTERPOLATION] Smooth movement between physics frames
    // This eliminates stuttering when render FPS (180) > physics FPS (60)
    // mix(a, b, t) = a * (1-t) + b * t
    vec2 interpolatedPos = mix(e.prevPosition, e.position, interpolationFactor);
    
    // Auto-calculate Rotation from Velocity
    // Only rotate if moving significantly
    float rotation = 0.0;
    if (length(e.velocity) > 0.1) {
        rotation = atan(e.velocity.y, e.velocity.x);
    }
    
    float c = cos(rotation);
    float s = sin(rotation);
    mat2 rot = mat2(c, -s, s, c);
    
    // Apply Scale (Radius * 2) and Rotation
    vec2 scale = vec2(e.radius * 2.0);
    vec2 pos = aPos * scale;
    pos = rot * pos;
    vec2 worldPos = interpolatedPos + pos;
    
    gl_Position = viewProj * vec4(worldPos, 0.0, 1.0);
    
    vTexCoord = aPos + 0.5; // [0, 1]
    vLocalPos = aPos * 2.0; // [-1, 1] for circle SDF
    vTextureIndex = e.type;
    vFlags = e.flags;
    vGlow = stats[entityId].glowIntensity;
}
