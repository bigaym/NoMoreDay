#version 430 core

// Instanced Item Label Shader (Fragment)
// Uses SDF (Signed Distance Field) to draw perfect rounded rectangles.

in vec2 fragTexCoord;    // [0,1]
in vec2 fragSize;        // Pixel dimensions
in vec4 fragBgColor;
in vec4 fragBorderColor;
in float fragBorderWidth;
in float fragCornerRadius;

out vec4 finalColor;

// SDF for a rounded box
// p: point relative to center
// b: half-extents (size / 2)
// r: corner radius
float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    // Convert UV [0,1] to Pixel Coordinates relative to center
    // Center is (0,0), range is [-size/2, +size/2]
    vec2 p = (fragTexCoord - 0.5) * fragSize;
    vec2 halfSize = fragSize * 0.5;
    
    // Calculate Distance Field
    // Ensure radius doesn't exceed half dimensions
    float r = min(fragCornerRadius, min(halfSize.x, halfSize.y));
    float d = sdRoundedBox(p, halfSize, r);
    
    // --- Rendering Logic ---
    
    // 1. Background (Interior)
    // d <= 0 is inside. d > 0 is outside.
    // Use fwidth for anti-aliasing (approx 1 pixel width)
    float aaf = fwidth(d);
    
    // Smoothstep for AA: 1.0 inside, 0.0 outside
    float alphaBg = 1.0 - smoothstep(-aaf, 0.0, d);
    
    // 2. Border
    // Border is centered at d=0 (edge), or interior?
    // Let's make border 'inside' the shape or 'straddling' the edge.
    // Usually UI borders are inner strokes.
    // Inner Stroke: from d = -borderWidth to d = 0
    
    // Distance to the border shell
    // We want 1.0 when -borderWidth < d < 0
    float alphaBorder = smoothstep(-fragBorderWidth - aaf, -fragBorderWidth, d) 
                      - smoothstep(-aaf, 0.0, d);
                      
    // Combined Mixing
    // Base is background color
    vec4 color = fragBgColor;
    color.a *= alphaBg;
    
    // Mix border on top
    // Note: This logic assumes border sits *on top* of background.
    // If we want border to *replace* background at the edge:
    vec4 border = fragBorderColor;
    
    // Better mixing strategy:
    // If inside border region, mix to border color.
    float borderFactor = smoothstep(-fragBorderWidth - aaf, -fragBorderWidth, d); // 0 at inner edge, 1 at outer edge
    
    // But we strictly want the border zone.
    // Let's just use mix() based on implicit areas.
    
    // Correct AA approach for "Inner Border":
    // Shape Alpha = smoothstep(0, -aaf, d) -> 1 inside, 0 outside
    float shapeAlpha = 1.0 - smoothstep(0.0, aaf, d);
    
    if (shapeAlpha <= 0.0) discard;
    
    // Border Mask: 1.0 at border, 0.0 at center
    // Border region is where d > -borderWidth
    float borderMask = smoothstep(-fragBorderWidth - aaf, -fragBorderWidth, d);
    
    // Mix background and border color based on mask
    vec3 mixedRGB = mix(fragBgColor.rgb, fragBorderColor.rgb, borderMask);
    float mixedAlpha = mix(fragBgColor.a, fragBorderColor.a, borderMask);
    
    // Final output with Shape Alpha (AA at outer edge)
    finalColor = vec4(mixedRGB, mixedAlpha * shapeAlpha);
}
