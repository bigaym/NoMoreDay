#version 430
#include "../generated/gpu_abi.glslinc"

layout(location = 0) in vec2 vertexPos;

layout(std430, binding = GPU_PARTICLE_SSBO_BINDING) readonly buffer CompactBuffer {
    GPUParticle particles[];
};

uniform mat4 mvp;

out vec4 fragColor;
out flat uint vFlags;
out vec2 vTexCoord;

void main() {
    GPUParticle p = particles[gl_InstanceID];
    if (p.lifetime <= 0.0 || p.scale <= 0.0) {
        gl_Position = vec4(-9999.0, -9999.0, 0.0, 1.0);
        return;
    }

    float lifetimeRatio = clamp(p.lifetime / max(p.maxLifetime, 0.0001), 0.0, 1.0);
    fragColor = vec4(
        float(p.colorPacked & 0xFFu) / 255.0,
        float((p.colorPacked >> 8) & 0xFFu) / 255.0,
        float((p.colorPacked >> 16) & 0xFFu) / 255.0,
        float((p.colorPacked >> 24) & 0xFFu) / 255.0 * lifetimeRatio
    );
    vFlags = p.flags;
    vTexCoord = vertexPos + 0.5;
    float cosR = cos(p.rotation);
    float sinR = sin(p.rotation);
    mat2 rotMat = mat2(cosR, sinR, -sinR, cosR);
    float finalScale = p.scale * sqrt(lifetimeRatio);
    gl_Position = mvp * vec4(rotMat * vertexPos * finalScale + p.position, 0.0, 1.0);
}
