#version 430
layout(location = 0) in vec2 vertexPos; 

// Must match GPUEntity in GPUData.hpp (64 bytes)
struct Entity {
    vec2 position;     // 8
    vec2 prevPosition; // 8
    vec2 velocity;     // 8
    float radius;      // 4
    int type;          // 4
    uint flags;        // 4
    uint frameId;      // 4
    float padding[6];  // 24
};

layout(std430, binding = 1) buffer EntityBuffer { Entity entities[]; };

uniform mat4 mvp;
uniform float renderAlpha; // Interpolation factor [0, 1]

out vec4 fragColor;
out vec2 localPos;

void main() {
    uint id = gl_InstanceID;
    Entity e = entities[id];
    
    // Check flags for NO_RENDER (bit 1)
    if (e.radius <= 0.0 || (e.flags & 2u) != 0u) {
        gl_Position = vec4(2.0, 2.0, 0.0, 1.0); // Clip
        return;
    }

    localPos = vertexPos * 2.0; // [-1, 1] range
    
    // Interpolation for smooth movement
    vec2 interpolatedPos = mix(e.prevPosition, e.position, renderAlpha);
    
    vec2 worldPos = vertexPos * (e.radius * 2.0) + interpolatedPos;
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
    
    // Color based on Type
    if (e.type == 1) { // Enemy
        fragColor = vec4(1.0, 0.3, 0.3, 1.0);
    } else { // Neutral/Player
        fragColor = vec4(0.3, 0.3, 1.0, 1.0);
    }
}
