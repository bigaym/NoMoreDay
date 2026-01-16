#version 430 core

// -------------------------------------------------------------------------
// Name: <Name>
// Type: Compute Shader
// Purpose: <High level purpose>
// -------------------------------------------------------------------------

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// -------------------------------------------------------------------------
// Buffers (std430 for tight packing, align 4)
// -------------------------------------------------------------------------
struct EntityData {
    vec2 position;
    vec2 velocity;
    uint color;     // Packed RGBA
    float scale;
};

layout(std430, binding = 0) readonly buffer InputBuffer {
    EntityData in_entities[];
};

layout(std430, binding = 1) writeonly buffer OutputBuffer {
    EntityData out_entities[];
};

// -------------------------------------------------------------------------
// Uniforms
// -------------------------------------------------------------------------
uniform float u_deltaTime;
uniform vec2 u_resolution;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= in_entities.length()) return;

    // Logic here
    EntityData e = in_entities[idx];
    e.position += e.velocity * u_deltaTime;
    
    out_entities[idx] = e;
}
