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
uniform sampler2DArray materialRoughnessArray;
uniform int uBlendPass;
uniform int uMaterialCount;
uniform int uMaterialQualityLevel;
uniform int uNormalLightingEnabled;
uniform int uSpecularEnabled;
uniform float uShadowFactor;

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

void main() {
    uint materialId = (vFlags >> 16u) & 0xFFFFu;
    bool hasMaterial = materialId > 0u && materialId < uint(max(uMaterialCount, 0));

    MaterialData mat;
    uint effectiveBlend = vBlendMode;
    if (hasMaterial) {
        mat = materials[materialId];
        uint materialBlend = uint(clamp(mat.detailParams.y, 0.0, 2.0));
        effectiveBlend = (materialBlend == 2u) ? 0u : materialBlend;
    }

    if (int(effectiveBlend) != uBlendPass) {
        discard;
    }

    float alpha = fragColor.a;
    vec3 rgb = fragColor.rgb;

    if (vTextureIndex >= 0) {
        vec4 texColor = texture(particleAtlas, vec3(vAtlasUV, float(vTextureIndex)));
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
        float roughness = clamp(mat.pbrLite.x, 0.0, 1.0);
        float specularStrength = clamp(mat.pbrLite.y, 0.0, 1.0);
        float ao = clamp(mat.pbrLite.z, 0.0, 1.0);

        if (uMaterialQualityLevel > 0 && uNormalLightingEnabled != 0) {
            int normalSlot = DecodeSlot(mat.textureSlots.y);
            if (normalSlot >= 0) {
                vec3 packedNormal = texture(materialNormalArray, vec3(vAtlasUV, float(normalSlot))).xyz;
                N = normalize(packedNormal * 2.0 - 1.0);
            }

            if (uMaterialQualityLevel >= 2) {
                int roughnessSlot = DecodeSlot(mat.textureSlots.z);
                if (roughnessSlot >= 0) {
                    roughness = texture(materialRoughnessArray, vec3(vAtlasUV, float(roughnessSlot))).r;
                }
            } else {
                specularStrength = 0.0;
            }
        } else {
            specularStrength = 0.0;
        }

        if (uSpecularEnabled == 0 || uMaterialQualityLevel < 2) {
            specularStrength = 0.0;
        }

        vec3 L = normalize(vec3(0.35, 0.45, 0.82));
        vec3 V = vec3(0.0, 0.0, 1.0);
        float diffuse = max(dot(N, L), 0.0);
        float specular = ComputeSpecular(N, L, V, roughness, specularStrength);
        float shadow = clamp(uShadowFactor, 0.0, 1.0);
        float brdf = clamp(diffuse + specular, 0.0, 2.0);
        rgb *= (0.25 + 0.75 * brdf) * ao * shadow;
    }

    finalColor = vec4(rgb, alpha);

    if (finalColor.a < 0.01) {
        discard;
    }
}
