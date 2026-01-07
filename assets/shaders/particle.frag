#version 430

in vec4 fragColor;
in flat uint vFlags;
in vec2 vTexCoord;

out vec4 finalColor;

void main() {
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(vTexCoord, center);
    vec4 col = fragColor;

    // Check lowest 3 bits for shape type (0-7)
    uint shapeType = vFlags & 0x7u;

    if (shapeType == 0u) {
        // Soft Circle (Glow)
        // Smoothstep from 0.5 down to 0.0
        float alpha = 1.0 - smoothstep(0.0, 0.5, dist);
        // Exponential falloff for glow
        alpha = pow(alpha, 1.5);
        col.a *= alpha;
    } 
    else if (shapeType == 1u) {
        // Hard Square
        // Just use full quad, maybe small border?
        // No modification needed
    }
    else if (shapeType == 2u) {
        // Spark / Diamond
        float d = abs(vTexCoord.x - 0.5) + abs(vTexCoord.y - 0.5);
        float alpha = 1.0 - smoothstep(0.0, 0.5, d);
        col.a *= alpha;
    }
    else if (shapeType == 3u) {
        // Ring
        float d = abs(dist - 0.35); // Ring at 0.35 radius
        float alpha = 1.0 - smoothstep(0.0, 0.15, d);
        col.a *= alpha;
    }
    else if (shapeType == 4u) {
        // Star (4-point)
        // Distance to axes
        float d = min(abs(vTexCoord.x - 0.5), abs(vTexCoord.y - 0.5));
        // Fade out as we go away from center
        float core = 1.0 - smoothstep(0.0, 0.5, dist);
        float ray = 1.0 - smoothstep(0.0, 0.05, d); 
        col.a *= max(core, ray * (1.0 - dist*2.0));
    }

    if (col.a < 0.01) discard;
    finalColor = col;
}
