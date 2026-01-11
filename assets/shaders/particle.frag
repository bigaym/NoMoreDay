#version 430

// Inputs from vertex shader
in vec4 fragColor;
in flat uint vFlags;
in vec2 vTexCoord;

// Output
out vec4 finalColor;

void main() {
    // Distance from center (for circular shapes)
    float d = distance(vTexCoord, vec2(0.5));
    
    // Start with vertex color alpha
    float alpha = fragColor.a;
    
    // === Shape Rendering Based on Flags ===
    
    if (vFlags == 0u) {
        // Default: Soft circle with smooth edge
        alpha *= smoothstep(0.5, 0.3, d);
    }
    else if (vFlags == 1u) {
        // Soft glow: Very gradual fade
        alpha *= smoothstep(0.5, 0.0, d);
    }
    else if (vFlags == 2u) {
        // Spark / Diamond shape
        float spark = 1.0 - (abs(vTexCoord.x - 0.5) + abs(vTexCoord.y - 0.5)) * 2.0;
        alpha *= clamp(spark * 2.0, 0.0, 1.0);
    }
    else if (vFlags == 3u) {
        // Ring shape
        float ring = 1.0 - abs(d - 0.35) * 8.0;
        alpha *= clamp(ring, 0.0, 1.0);
    }
    else if (vFlags == 4u) {
        // Hard circle (no gradient)
        alpha *= step(d, 0.4);
    }
    else if (vFlags == 5u) {
        // Soft ink splat
        alpha *= smoothstep(0.5, 0.2, d);
    }
    else if (vFlags == 13u) {
        // Ink with extra soft edges (legacy compatibility)
        alpha *= smoothstep(0.5, 0.15, d);
    }
    else {
        // Fallback: standard circle
        alpha *= smoothstep(0.5, 0.35, d);
    }
    
    // Discard fully transparent pixels
    if (alpha < 0.01) {
        discard;
    }
    
    // Output final color with calculated alpha
    finalColor = vec4(fragColor.rgb, alpha);
}
