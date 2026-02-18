#version 430 core

in vec2 vTexCoord;
in vec2 vLocalPos;
flat in int vTextureIndex;
flat in uint vFlags;
flat in float vGlow;
flat in uint vStatusMask;
flat in float vTime;

uniform sampler2DArray entityTextures;
uniform int uMaterialQualityLevel;
uniform int uNormalLightingEnabled;
uniform int uSpecularEnabled;
uniform float uShadowFactor;

out vec4 fragColor;

float ComputeSpecular(vec3 N, vec3 L, vec3 V, float roughness, float strength) {
    if (strength <= 0.0) {
        return 0.0;
    }
    vec3 H = normalize(L + V);
    float ndoth = max(dot(N, H), 0.0);
    float shininess = mix(48.0, 6.0, clamp(roughness, 0.0, 1.0));
    return pow(ndoth, shininess) * strength;
}

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
        statusGlow += vec3(0.2, 0.6, 1.0) * (0.3 + 0.2 * sin(vTime * 6.28));
    }
    if ((vStatusMask & 2u) != 0u) { // Burning
        statusGlow += vec3(1.0, 0.5, 0.1) * (0.4 + 0.2 * sin(vTime * 12.56));
    }
    
    // 稀有度发光
    if (vGlow > 0.0) {
        statusGlow += vec3(vGlow * 0.4, vGlow * 0.4, vGlow * 0.6);
    }
    
    vec3 litColor = baseColor.rgb;
    if (uMaterialQualityLevel > 0 && uNormalLightingEnabled != 0) {
        vec3 N = normalize(vec3(vLocalPos * 0.6, 1.0));
        vec3 L = normalize(vec3(0.35, 0.45, 0.82));
        vec3 V = vec3(0.0, 0.0, 1.0);
        float roughness = (uMaterialQualityLevel >= 2) ? 0.6 : 0.85;
        float specularStrength =
            (uSpecularEnabled != 0 && uMaterialQualityLevel >= 2) ? 0.2 : 0.0;
        float diffuse = max(dot(N, L), 0.0);
        float specular = ComputeSpecular(N, L, V, roughness, specularStrength);
        float brdf = clamp(diffuse + specular, 0.0, 2.0);
        litColor *= (0.25 + 0.75 * brdf) * clamp(uShadowFactor, 0.0, 1.0);
    }

    fragColor = vec4(litColor + statusGlow, baseColor.a);
}
