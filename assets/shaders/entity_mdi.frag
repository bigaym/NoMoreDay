#version 430 core
#include "generated/material_abi.glslinc"

in vec2 vTexCoord;
in vec2 vLocalPos;
flat in int vTextureIndex;
flat in uint vFlags;
flat in float vGlow;
flat in uint vStatusMask;
flat in float vTime;

uniform sampler2DArray entityTextures;
uniform sampler2DArray materialNormalArray;
uniform sampler2DArray materialMaskArray;
uniform sampler2DArray materialDetailArray;
uniform int uMaterialCount;
uniform int uMaterialQualityLevel;
uniform int uNormalLightingEnabled;
uniform int uSpecularEnabled;
uniform float uShadowFactor;
uniform int uLinearPipeline;

out vec4 fragColor;

int DecodeSlot(float slotValue) {
    return int(floor(slotValue + 0.5));
}

float ComputeSpecular(vec3 N, vec3 L, vec3 V, float roughness, float strength) {
    if (strength <= 0.0) {
        return 0.0;
    }
    vec3 H = normalize(L + V);
    float ndoth = max(dot(N, H), 0.0);
    float shininess = mix(48.0, 6.0, clamp(roughness, 0.0, 1.0));
    return pow(ndoth, shininess) * strength;
}

float DistributionGGX(float NdotH, float roughness) {
    float a = max(0.05, roughness);
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(0.0001, 3.14159265 * denom * denom);
}

float GeometrySchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) * 0.125;
    return NdotX / max(0.0001, NdotX * (1.0 - k) + k);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 BrdfLite(vec3 baseColor, vec3 N, vec3 L, vec3 V, float roughness,
              float metallic, float ao, vec3 F0, float rimSuppress) {
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySchlickGGX(NdotL, roughness) * GeometrySchlickGGX(NdotV, roughness);
    vec3 F = FresnelSchlick(HdotV, F0);
    float rimFactor = mix(1.0, rimSuppress, step(0.7, 1.0 - NdotV));
    F *= rimFactor;

    vec3 kd = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kd * baseColor / 3.14159265;
    vec3 spec = (D * G * F) / max(0.0001, 4.0 * NdotL * NdotV);
    return (diffuse + spec) * NdotL * max(0.0, ao);
}

void main() {
    // 基础颜色 (纹理或 SDF)
    vec4 baseColor;
    if (vTextureIndex >= 0) {
        baseColor = texture(entityTextures, vec3(vTexCoord, float(vTextureIndex)));
        if (baseColor.a < 0.1) discard;
        if (uLinearPipeline != 0) {
            baseColor.rgb = pow(baseColor.rgb, vec3(2.2));
        }
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
    
    uint materialId = (vFlags >> 16u) & 0xFFFFu;
    bool hasMaterial = materialId > 0u && materialId < uint(max(uMaterialCount, 0));

    vec3 litColor = baseColor.rgb;
    if (uMaterialQualityLevel > 0 && uNormalLightingEnabled != 0 && hasMaterial) {
        MaterialData mat = materials[materialId];
        vec3 N = normalize(vec3(vLocalPos * 0.6, 1.0));
        vec3 L = normalize(vec3(0.35, 0.45, 0.82));
        vec3 V = vec3(0.0, 0.0, 1.0);

        float roughness = clamp(mat.pbrParams.x + mat.fresnelControl.z, 0.0, 1.0);
        float metallic = clamp(mat.pbrParams.y, 0.0, 1.0);
        float ao = clamp(mat.pbrParams.z, 0.0, 1.0);
        float rimSuppress = clamp(mat.fresnelControl.y, 0.0, 1.0);
        float f0 = clamp(mat.fresnelControl.x, 0.0, 1.0);

        int normalSlot = DecodeSlot(mat.textureSlots.y);
        if (normalSlot >= 0) {
            vec3 packedNormal = texture(materialNormalArray, vec3(vTexCoord, float(normalSlot))).xyz;
            N = normalize(packedNormal * 2.0 - 1.0);
        }

        if (uMaterialQualityLevel >= 2) {
            int maskSlot = DecodeSlot(mat.textureSlots.z);
            if (maskSlot >= 0) {
                vec4 maskTex = texture(materialMaskArray, vec3(vTexCoord, float(maskSlot)));
                roughness = maskTex.r;
                metallic = maskTex.g;
                ao = min(ao, maskTex.b);
            }
            if (uMaterialQualityLevel >= 3) {
                int detailSlot = DecodeSlot(mat.textureSlots.w);
                if (detailSlot >= 0) {
                    vec3 detailN = texture(materialDetailArray, vec3(vTexCoord * 2.0, float(detailSlot))).xyz * 2.0 - 1.0;
                    N = normalize(mix(N, detailN, 0.2));
                }
            }
        }

        if (uMaterialQualityLevel == 1 || uSpecularEnabled == 0) {
            float diffuse = max(dot(N, L), 0.0);
            litColor *= (0.2 + 0.8 * diffuse) * ao * clamp(uShadowFactor, 0.0, 1.0);
        } else {
            vec3 F0 = mix(vec3(f0), litColor, metallic);
            vec3 brdf = BrdfLite(litColor, N, L, V, roughness, metallic, ao, F0, rimSuppress);
            litColor = brdf * clamp(uShadowFactor, 0.0, 1.0) + mat.emissiveAndIntensity.rgb * mat.emissiveAndIntensity.w;
        }
    }

    fragColor = vec4(litColor + statusGlow, baseColor.a);
}
