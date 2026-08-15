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
    
    // Anti-aliasing width (half-pixel step for exact 1-pixel AA transition)
    float aa = max(0.5 * fwidth(d), 0.35);
    float shapeAlpha = clamp(-d / aa + 0.5, 0.0, 1.0);
    
    if (shapeAlpha <= 0.0) discard;
    
    // Inner Border: 1.0 at outer border band, 0.0 at interior background
    float borderDist = d + fragBorderWidth;
    float borderMask = clamp(borderDist / aa + 0.5, 0.0, 1.0);
    
    // Mix background and border color based on mask
    vec3 mixedRGB = mix(fragBgColor.rgb, fragBorderColor.rgb, borderMask);
    float mixedAlpha = mix(fragBgColor.a, fragBorderColor.a, borderMask);
    
    // Final output with Shape Alpha (AA at outer edge)
    finalColor = vec4(mixedRGB, mixedAlpha * shapeAlpha);
}

