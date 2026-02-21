#version 430 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

struct FluidParticle {
    vec2 position;
    vec2 velocity;
    float density;
    float pressure;
    vec4 color;
    float lifetime;
    uint flags;
};

layout(std430, binding = 0) readonly buffer ParticlesIn {
    FluidParticle particles[];
};

uniform mat4 uViewProj;
uniform float uParticleRadius;
uniform int uParticleCount;

out vec2 vLocalUv;
out vec2 vWorldPos;
out vec4 vColor;
out float vDensity;

void main() {
    uint idx = uint(gl_InstanceID);
    if (idx >= uint(max(uParticleCount, 0))) {
        gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
        vLocalUv = aUV;
        vWorldPos = vec2(0.0);
        vColor = vec4(0.0);
        vDensity = 0.0;
        return;
    }

    FluidParticle p = particles[idx];
    vec2 world = p.position + aPos * uParticleRadius;
    gl_Position = uViewProj * vec4(world, 0.0, 1.0);

    vLocalUv = aUV;
    vWorldPos = world;
    vColor = p.color;
    vDensity = p.density;
}

