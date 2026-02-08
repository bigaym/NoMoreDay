#version 430

// Inputs from vertex shader
in vec4 fragColor;
in flat uint vFlags;
in vec2 vTexCoord;

// Output
out vec4 finalColor;

void main() {
    // Shape ID is in the lower 8 bits
    uint shapeId = vFlags & 0xFFu;

    // Distance from center (for circular shapes)
    float d = distance(vTexCoord, vec2(0.5));
    
    // Start with vertex color alpha
    float alpha = fragColor.a;
    
    // === Shape Rendering Based on Shape ID ===
    
    if (shapeId == 0u) {
        // Default: Small soft circle with tight edge
        alpha *= smoothstep(0.5, 0.2, d);
    }
    else if (vFlags == 1u) {
        // Soft glow: Very gradual fade but tighter
        alpha *= smoothstep(0.5, 0.1, d);
    }
    else if (vFlags == 2u) {
        // Spark / Diamond shape - sharper and smaller
        float spark = 1.0 - (abs(vTexCoord.x - 0.5) + abs(vTexCoord.y - 0.5)) * 2.5;
        alpha *= clamp(spark * 3.0, 0.0, 1.0);
    }
    else if (vFlags == 3u) {
        // Ring shape - thinner ring
        float ring = 1.0 - abs(d - 0.35) * 12.0;
        alpha *= clamp(ring, 0.0, 1.0);
    }
    else if (vFlags == 4u) {
        // Hard circle (no gradient) - smaller
        alpha *= step(d, 0.3);
    }
    else if (vFlags == 5u) {
        // Soft ink splat - tighter falloff
        alpha *= smoothstep(0.5, 0.15, d);
    }
    else if (vFlags == 6u) {
        // Tiny point - very concentrated
        alpha *= smoothstep(0.5, 0.05, d);
    }
    else if (vFlags == 7u) {
        // Sharp star/cross shape
        float cross = max(
            1.0 - abs(vTexCoord.x - 0.5) * 6.0,
            1.0 - abs(vTexCoord.y - 0.5) * 6.0
        );
        alpha *= clamp(cross * 2.0, 0.0, 1.0);
    }
    else if (vFlags == 8u) {
        // Soft elongated horizontal (for trails)
        float elongated = smoothstep(0.5, 0.2, abs(vTexCoord.y - 0.5)) * 
                          smoothstep(0.5, 0.1, abs(vTexCoord.x - 0.5));
        alpha *= elongated;
    }
    else if (vFlags == 13u) {
        // Ink with extra soft edges (legacy compatibility) - tighter
        alpha *= smoothstep(0.5, 0.1, d);
    }
    else {
        // Fallback: tight standard circle
        alpha *= smoothstep(0.5, 0.25, d);
    }
    
    // Discard fully transparent pixels
    if (alpha < 0.01) {
        discard;
    }
    
    // Output final color with calculated alpha
    finalColor = vec4(fragColor.rgb, alpha);
}
