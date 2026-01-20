#version 430 core
layout(location = 0) in vec2 aPos; // [-0.5, 0.5]

struct InstanceData {
    vec2 position;
    vec2 scale; 
    float rotation;
    uint textureIndex;
    uint flags;
    float _padding;
};

// Bindings match Cull/MDIRenderer
layout(std430, binding = 0) readonly buffer Entities { InstanceData entities[]; };
layout(std430, binding = 1) readonly buffer VisibleIndices { uint visibleIndices[]; };

uniform mat4 viewProj;

out vec2 vTexCoord;
out vec2 vLocalPos;
flat out uint vTextureIndex;
flat out uint vFlags;

void main() {
    uint entityId = visibleIndices[gl_InstanceID];
    InstanceData e = entities[entityId];
    
    // Rotation
    float c = cos(e.rotation);
    float s = sin(e.rotation);
    mat2 rot = mat2(c, -s, s, c);
    
    // Apply Scale and Rotation
    vec2 pos = aPos * e.scale;
    pos = rot * pos;
    vec2 worldPos = e.position + pos;
    
    gl_Position = viewProj * vec4(worldPos, 0.0, 1.0);
    
    vTexCoord = aPos + 0.5; // [0, 1]
    vLocalPos = aPos * 2.0; // [-1, 1] for circle SDF
    vTextureIndex = e.textureIndex;
    vFlags = e.flags;
}
