#version 430

in vec4 fragColor;
in flat uint vFlags;
in vec2 vTexCoord;

out vec4 finalColor;

void main() {
    float d = distance(vTexCoord, vec2(0.5));
    float alpha = fragColor.a;

    // Different shapes based on flags
    if (vFlags == 2) { // Spark / Diamond
        float spark = 1.0 - (abs(vTexCoord.x - 0.5) + abs(vTexCoord.y - 0.5)) * 2.0;
        alpha *= clamp(spark * 2.0, 0.0, 1.0);
    } else if (vFlags == 13 || vFlags == 5) { // Ink Splat / Soft Glow
        alpha *= smoothstep(0.5, 0.2, d);
    } else { // Default Circle
        alpha *= smoothstep(0.5, 0.45, d);
    }

    if (alpha < 0.01) discard;

    finalColor = vec4(fragColor.rgb, alpha);
}
