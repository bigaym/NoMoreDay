#version 430
layout(location = 0) in vec2 vertexPos; 

struct Entity {
    vec2 position;
    vec2 velocity;
    float radius;
    int type;
    int id;
    float padding;
};

layout(std430, binding = 1) buffer EntityBuffer { Entity entities[]; };

uniform mat4 mvp;

out vec4 fragColor;
out vec2 localPos;

void main() {
    uint id = gl_InstanceID;
    Entity e = entities[id];
    
    if (e.radius <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 0.0, 1.0); // Clip
        return;
    }

    localPos = vertexPos * 2.0; // [-1, 1] range
    vec2 worldPos = vertexPos * (e.radius * 2.0) + e.position;
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
    
    // Hardcoded color for now (Red for enemies)
    fragColor = vec4(1.0, 0.3, 0.3, 1.0);
}
