#version 430 core
layout(location = 0) in vec2 aPos; // [-0.5, 0.5]

// Must match GPUEntity in GPUData.hpp
struct InstanceData {
    vec2 position;
    vec2 velocity;
    float radius;
    uint type;
    uint flags;
    float padding;
};

// Bindings match Cull/MDIRenderer
layout(std430, binding = 0) readonly buffer Entities { InstanceData entities[]; };
layout(std430, binding = 1) readonly buffer VisibleIndices { uint visibleIndices[]; };

uniform mat4 viewProj;
uniform float interpolationFactor; // Factor between 0 and 1 for physics interpolation

out vec2 vTexCoord;
out vec2 vLocalPos;
flat out uint vTextureIndex;
flat out uint vFlags;

void main() {
    uint entityId = visibleIndices[gl_InstanceID];
    InstanceData e = entities[entityId];
    
    // Auto-calculate Rotation from Velocity
    // Only rotate if moving significantly
    float rotation = 0.0;
    if (length(e.velocity) > 0.1) {
        rotation = atan(e.velocity.y, e.velocity.x);
    }
    
    float c = cos(rotation);
    float s = sin(rotation);
    mat2 rot = mat2(c, -s, s, c);
    
    // [INTERPOLATION] Predict current position based on velocity and frame offset
    // This removes the "stuttering" feel when rendering frame rate is higher than physics
    vec2 interpolatedPos = e.position + e.velocity * interpolationFactor;
    
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
}
