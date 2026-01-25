#version 430 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;

// Instance Data (Binding 5)
struct BeamInstance {
    vec2 position; // Bottom Center
    vec2 size;     // width, height
    vec4 color;    // Base Color
    float time;
    float padding[3];
};

layout(std430, binding = 5) buffer InstanceBuffer {
    BeamInstance instances[];
};

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec4 fragColor;
out float fragTime;

void main() {
    BeamInstance inst = instances[gl_InstanceID];
    
    // We assume the base mesh is a quad [0,0] to [1,1]
    // We want to anchor at Bottom Center.
    // X: (0..1) -> (-0.5 .. 0.5) * width + centerX
    // Y: (0..1) -> (0 .. -1) * height + bottomY (Growing Upwards)
    
    vec2 worldPos = vec2(
        (vertexPosition.x - 0.5) * inst.size.x + inst.position.x,
        (vertexPosition.y - 1.0) * inst.size.y + inst.position.y
    );
    
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
    
    fragTexCoord = vertexTexCoord;
    fragColor = inst.color;
    fragTime = inst.time;
}
