#version 430
#include "generated/material_abi.glslinc"

in vec4 fragColor;
in flat uint vFlags;
in vec2 vTexCoord;
in flat int vTextureIndex;
in vec2 vAtlasUV;
in flat uint vBlendMode;

uniform sampler2DArray particleAtlas;
uniform sampler2DArray materialNormalArray;
uniform sampler2DArray materialMaskArray;
uniform sampler2DArray materialDetailArray;
uniform int uBlendPass;
uniform int uMaterialCount;
uniform int uMaterialQualityLevel;
uniform int uNormalLightingEnabled;
uniform int uSpecularEnabled;
uniform float uShadowFactor;
uniform int uLinearPipeline;

out vec4 finalColor;

int DecodeSlot(float slotValue) {
    return int(floor(slotValue + 0.5));
}

float ComputeSpecular(vec3 N, vec3 L, vec3 V, float roughness, float specularStrength) {
    if (specularStrength <= 0.0) {
        return 0.0;
    }
    vec3 H = normalize(L + V);
    float ndoth = max(dot(N, H), 0.0);
    float clampedRoughness = clamp(roughness, 0.0, 1.0);
    float shininess = mix(64.0, 4.0, clampedRoughness);
    return pow(ndoth, shininess) * specularStrength;
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
    uint materialId = (vFlags >> 16u) & 0xFFFFu;
    bool hasMaterial = materialId > 0u && materialId < uint(max(uMaterialCount, 0));

    MaterialData mat;
    uint effectiveBlend = vBlendMode;
    if (hasMaterial) {
        mat = materials[materialId];
    }

    if (int(effectiveBlend) != uBlendPass) {
        discard;
    }

    float alpha = fragColor.a;
    vec3 rgb = fragColor.rgb;

    if (vTextureIndex >= 0) {
        vec4 texColor = texture(particleAtlas, vec3(vAtlasUV, float(vTextureIndex)));
        if (uLinearPipeline != 0) {
            texColor.rgb = pow(texColor.rgb, vec3(2.2));
        }
        rgb *= texColor.rgb;
        alpha *= texColor.a;
        if (effectiveBlend == 1u) {
            rgb *= 1.2;
        }
    } else {
        uint shapeId = vFlags & 0xFFu;
        float d = distance(vTexCoord, vec2(0.5));

        if (shapeId == 0u) {
            alpha *= smoothstep(0.5, 0.2, d);
        } else if (shapeId == 1u) {
            alpha *= smoothstep(0.5, 0.1, d);
        } else if (shapeId == 2u) {
            float spark = 1.0 - (abs(vTexCoord.x - 0.5) + abs(vTexCoord.y - 0.5)) * 2.5;
            alpha *= clamp(spark * 3.0, 0.0, 1.0);
        } else if (shapeId == 3u) {
            float ring = 1.0 - abs(d - 0.35) * 12.0;
            alpha *= clamp(ring, 0.0, 1.0);
        } else if (shapeId == 4u) {
            alpha *= step(d, 0.3);
        } else if (shapeId == 5u) {
            alpha *= smoothstep(0.5, 0.15, d);
        } else if (shapeId == 6u) {
            alpha *= smoothstep(0.5, 0.05, d);
        } else if (shapeId == 7u) {
            float cross = max(
                1.0 - abs(vTexCoord.x - 0.5) * 6.0,
                1.0 - abs(vTexCoord.y - 0.5) * 6.0
            );
            alpha *= clamp(cross * 2.0, 0.0, 1.0);
        } else if (shapeId == 8u) {
            float elongated = smoothstep(0.5, 0.2, abs(vTexCoord.y - 0.5)) *
                              smoothstep(0.5, 0.1, abs(vTexCoord.x - 0.5));
            alpha *= elongated;
        } else if (shapeId == 13u) {
            alpha *= smoothstep(0.5, 0.1, d);
        } else {
            alpha *= smoothstep(0.5, 0.25, d);
        }
    }

    if (hasMaterial) {
        rgb *= mat.baseColor.rgb;
        alpha *= mat.baseColor.a;
        rgb += mat.emissiveAndIntensity.rgb * mat.emissiveAndIntensity.w;

        vec3 N = vec3(0.0, 0.0, 1.0);
        float roughness = clamp(mat.pbrParams.x, 0.0, 1.0);
        float metallic = clamp(mat.pbrParams.y, 0.0, 1.0);
        float ao = clamp(mat.pbrParams.z, 0.0, 1.0);
        float roughnessBias = mat.fresnelControl.z;
        float rimSuppress = clamp(mat.fresnelControl.y, 0.0, 1.0);
        float f0Override = clamp(mat.fresnelControl.x, 0.0, 1.0);
        float detailScale = clamp(mat.textureSlots.w, -1.0, 1024.0);

        if (uMaterialQualityLevel > 0 && uNormalLightingEnabled != 0) {
            int normalSlot = DecodeSlot(mat.textureSlots.y);
            if (normalSlot >= 0) {
                vec3 packedNormal = texture(materialNormalArray, vec3(vAtlasUV, float(normalSlot))).xyz;
                N = normalize(packedNormal * 2.0 - 1.0);
            }

            int maskSlot = DecodeSlot(mat.textureSlots.z);
            if (maskSlot >= 0) {
                vec4 maskTex = texture(materialMaskArray, vec3(vAtlasUV, float(maskSlot)));
                roughness = mix(roughness, maskTex.r, float(uMaterialQualityLevel >= 2));
                metallic = mix(metallic, maskTex.g, float(uMaterialQualityLevel >= 2));
                ao = min(ao, maskTex.b);
            }

            if (uMaterialQualityLevel >= 3 && detailScale >= 0.0) {
                int detailSlot = DecodeSlot(mat.textureSlots.w);
                if (detailSlot >= 0) {
                    vec3 detailNormal = texture(materialDetailArray, vec3(vAtlasUV * 2.0, float(detailSlot))).xyz * 2.0 - 1.0;
                    N = normalize(mix(N, detailNormal, 0.25));
                }
            }
        }

        vec3 L = normalize(vec3(0.35, 0.45, 0.82));
        vec3 V = vec3(0.0, 0.0, 1.0);
        float shadow = clamp(uShadowFactor, 0.0, 1.0);
        roughness = clamp(roughness + roughnessBias, 0.0, 1.0);

        if (uMaterialQualityLevel <= 0) {
            rgb *= ao * shadow;
        } else if (uMaterialQualityLevel == 1 || uSpecularEnabled == 0) {
            float diffuse = max(dot(N, L), 0.0);
            rgb *= (0.2 + 0.8 * diffuse) * ao * shadow;
        } else {
            vec3 F0 = mix(vec3(f0Override), rgb, metallic);
            vec3 lit = BrdfLite(rgb, N, L, V, roughness, metallic, ao, F0, rimSuppress);
            rgb = lit * shadow + mat.emissiveAndIntensity.rgb * mat.emissiveAndIntensity.w;
        }
    }

    finalColor = vec4(rgb, alpha);

    if (finalColor.a < 0.01) {
        discard;
    }
}
