#version 430

in vec4 fragColor;
in flat uint vFlags;
in vec2 vTexCoord;
in flat int vTextureIndex;
in vec2 vAtlasUV;
in flat uint vBlendMode;

uniform sampler2DArray particleAtlas;
uniform int uBlendPass;

out vec4 finalColor;

void main() {
    if (int(vBlendMode) != uBlendPass) {
        discard;
    }

    float alpha = fragColor.a;

    if (vTextureIndex >= 0) {
        vec4 texColor = texture(particleAtlas, vec3(vAtlasUV, float(vTextureIndex)));
        vec3 rgb = fragColor.rgb * texColor.rgb;
        if (vBlendMode == 1u) {
            rgb *= 1.2;
        }
        finalColor = vec4(rgb, alpha * texColor.a);
    } else {
        uint shapeId = vFlags & 0xFFu;
        float d = distance(vTexCoord, vec2(0.5));

        if (shapeId == 0u) {
            alpha *= smoothstep(0.5, 0.2, d);
        } else if (vFlags == 1u) {
            alpha *= smoothstep(0.5, 0.1, d);
        } else if (vFlags == 2u) {
            float spark = 1.0 - (abs(vTexCoord.x - 0.5) + abs(vTexCoord.y - 0.5)) * 2.5;
            alpha *= clamp(spark * 3.0, 0.0, 1.0);
        } else if (vFlags == 3u) {
            float ring = 1.0 - abs(d - 0.35) * 12.0;
            alpha *= clamp(ring, 0.0, 1.0);
        } else if (vFlags == 4u) {
            alpha *= step(d, 0.3);
        } else if (vFlags == 5u) {
            alpha *= smoothstep(0.5, 0.15, d);
        } else if (vFlags == 6u) {
            alpha *= smoothstep(0.5, 0.05, d);
        } else if (vFlags == 7u) {
            float cross = max(
                1.0 - abs(vTexCoord.x - 0.5) * 6.0,
                1.0 - abs(vTexCoord.y - 0.5) * 6.0
            );
            alpha *= clamp(cross * 2.0, 0.0, 1.0);
        } else if (vFlags == 8u) {
            float elongated = smoothstep(0.5, 0.2, abs(vTexCoord.y - 0.5)) *
                              smoothstep(0.5, 0.1, abs(vTexCoord.x - 0.5));
            alpha *= elongated;
        } else if (vFlags == 13u) {
            alpha *= smoothstep(0.5, 0.1, d);
        } else {
            alpha *= smoothstep(0.5, 0.25, d);
        }

        finalColor = vec4(fragColor.rgb, alpha);
    }

    if (finalColor.a < 0.01) {
        discard;
    }
}
