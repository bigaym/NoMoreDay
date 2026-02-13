#version 430
#include "generated/material_abi.glslinc"

in vec4 fragColor;
in flat uint vFlags;
in vec2 vTexCoord;
in flat int vTextureIndex;
in vec2 vAtlasUV;
in flat uint vBlendMode;

uniform sampler2DArray particleAtlas;
uniform int uBlendPass;
uniform int uMaterialCount;

out vec4 finalColor;

void main() {
    uint materialId = (vFlags >> 16u) & 0xFFFFu;
    bool hasMaterial = materialId > 0u && materialId < uint(max(uMaterialCount, 0));

    MaterialData mat;
    uint effectiveBlend = vBlendMode;
    if (hasMaterial) {
        mat = materials[materialId];
        effectiveBlend = (mat.blendMode == 2u) ? 0u : mat.blendMode;
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
        rgb += mat.emissive.rgb * mat.emissive.w;
    }

    finalColor = vec4(rgb, alpha);

    if (finalColor.a < 0.01) {
        discard;
    }
}
