#version 430

in vec4 fragColor;
in flat uint vFlags;
in vec2 vTexCoord;

out vec4 finalColor;

void main() {
    uint shapeId = vFlags & 0xFFu;
    float d = distance(vTexCoord, vec2(0.5));
    float shapeAlpha = smoothstep(0.5, 0.2, d);
    if (shapeId == 2u) {
        shapeAlpha = clamp((1.0 - (abs(vTexCoord.x - 0.5) + abs(vTexCoord.y - 0.5)) * 2.5) * 3.0, 0.0, 1.0);
    } else if (shapeId == 3u) {
        shapeAlpha = clamp(1.0 - abs(d - 0.35) * 12.0, 0.0, 1.0);
    }

    float alpha = fragColor.a * shapeAlpha;
    if (alpha < 0.01) {
        discard;
    }
    finalColor = vec4(fragColor.rgb * alpha, 1.0);
}
