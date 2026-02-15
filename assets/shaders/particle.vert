#version 430
#include "generated/gpu_abi.glslinc"

layout(location = 0) in vec2 vertexPos;

layout(std430, binding = GPU_PARTICLE_SSBO_BINDING) readonly buffer CompactBuffer {
    GPUParticle particles[];
};

uniform mat4 mvp;

out vec4 fragColor;
out flat uint vFlags;
out vec2 vTexCoord;
out flat int vTextureIndex;
out vec2 vAtlasUV;
out flat uint vBlendMode;

void main() {
    uint id = gl_InstanceID;
    GPUParticle p = particles[id];

    if (p.lifetime <= 0.0 || p.scale <= 0.0) {
        gl_Position = vec4(-9999.0, -9999.0, 0.0, 1.0);
        return;
    }

    vec4 col = vec4(
        float(p.colorPacked & 0xFFu) / 255.0,
        float((p.colorPacked >> 8) & 0xFFu) / 255.0,
        float((p.colorPacked >> 16) & 0xFFu) / 255.0,
        float((p.colorPacked >> 24) & 0xFFu) / 255.0
    );

    float lifetimeRatio = clamp(p.lifetime / p.maxLifetime, 0.0, 1.0);
    col.a *= lifetimeRatio;

    fragColor = col;
    vFlags = p.flags;
    vTexCoord = vertexPos + 0.5;

    vTextureIndex = (p.texInfo << 16) >> 16;
    int subUVPacked = (p.texInfo >> 16) & 0xFFFF;
    int subRows = (subUVPacked >> 8) & 0xFF;
    int subCols = subUVPacked & 0xFF;

    uint frameCount = p.animData & 0xFFFFu;
    vBlendMode = (p.animData >> 16) & 0xFFu;

    if (vTextureIndex >= 0 && frameCount > 0u && subRows > 0 && subCols > 0) {
        float progress = 1.0 - lifetimeRatio;
        int currentFrame = int(progress * float(frameCount - 1u));
        currentFrame = clamp(currentFrame, 0, int(frameCount) - 1);

        int row = currentFrame / subCols;
        int colIdx = currentFrame % subCols;
        float cellW = 1.0 / float(subCols);
        float cellH = 1.0 / float(subRows);

        vec2 uvBase = vec2(float(colIdx) * cellW, float(row) * cellH);
        vAtlasUV = uvBase + (vertexPos + 0.5) * vec2(cellW, cellH);
    } else if (vTextureIndex >= 0) {
        vAtlasUV = vertexPos + 0.5;
    } else {
        vAtlasUV = vec2(0.0);
    }

    float cosR = cos(p.rotation);
    float sinR = sin(p.rotation);
    mat2 rotMat = mat2(cosR, sinR, -sinR, cosR);

    float lifetimeScale = sqrt(lifetimeRatio);
    float finalScale = p.scale * lifetimeScale;
    vec2 worldPos = rotMat * vertexPos * finalScale + p.position;

    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
}
