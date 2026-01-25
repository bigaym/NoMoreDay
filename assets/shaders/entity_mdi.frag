#version 430 core

in vec2 vTexCoord;
in vec2 vLocalPos;
flat in int vTextureIndex;
flat in uint vFlags;
flat in float vGlow;
flat in uint vStatusMask;
flat in float vStatusTimer;

uniform sampler2DArray entityTextures;

out vec4 fragColor;

void main() {
    // 基础颜色 (纹理或 SDF)
    vec4 baseColor;
    if (vTextureIndex >= 0) {
        baseColor = texture(entityTextures, vec3(vTexCoord, float(vTextureIndex)));
        if (baseColor.a < 0.1) discard;
    } else {
        float distSq = dot(vLocalPos, vLocalPos);
        if (distSq > 1.0) discard;
        baseColor = vec4(1.0, 0.3, 0.3, 1.0);
    }

    // 状态特效发光
    vec3 statusGlow = vec3(0.0);
    if ((vStatusMask & 1u) != 0u) { // Frozen
        statusGlow += vec3(0.2, 0.6, 1.0) * (0.3 + 0.2 * sin(vStatusTimer * 6.28));
    }
    if ((vStatusMask & 2u) != 0u) { // Burning
        statusGlow += vec3(1.0, 0.5, 0.1) * (0.4 + 0.2 * sin(vStatusTimer * 12.56));
    }
    
    // 稀有度发光
    if (vGlow > 0.0) {
        statusGlow += vec3(vGlow * 0.4, vGlow * 0.4, vGlow * 0.6);
    }
    
    fragColor = vec4(baseColor.rgb + statusGlow, baseColor.a);
}