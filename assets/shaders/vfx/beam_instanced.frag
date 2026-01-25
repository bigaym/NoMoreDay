#version 430 core

in vec2 fragTexCoord;
in vec4 fragColor;
in float fragTime;

out vec4 finalColor;

void main() {
    // fragTexCoord.x is 0..1 (Left to Right)
    // fragTexCoord.y is 0..1 (Top to Bottom? or Bottom to Top?)
    // In Raylib default Quad: (0,0) is Top-Left usually?
    // Let's assume input texCoords match the vertex logic.
    // If vertex Y was generated as (y-1.0), then y=0 is top (-height), y=1 is bottom (0).
    // So TexCoord.y 0 is Top (Tip), 1 is Bottom (Base).
    
    float x = fragTexCoord.x - 0.5; // -0.5 to 0.5
    float y = fragTexCoord.y;       // 0.0 (Top) to 1.0 (Base)
    
    // 1. Vertical Gradient (Fade out at top)
    // Strong at bottom, weak at top.
    float vFade = smoothstep(0.0, 0.4, y);
    
    // 2. Horizontal Beam Shape (Gaussian-ish)
    float hDist = abs(x);
    float beamCore = 1.0 - smoothstep(0.0, 0.15, hDist); // Solid core
    float beamGlow = 1.0 - smoothstep(0.0, 0.5, hDist);  // Soft glow
    
    float beamAlpha = max(beamCore, beamGlow * 0.5);
    
    // 3. Pulse Animation
    float pulse = 0.85 + 0.15 * sin(fragTime * 3.0 - y * 4.0); // Moving wave up
    
    // 4. Combine
    vec4 col = fragColor;
    col.a *= vFade * beamAlpha * pulse;
    
    // Add a hot white core at the bottom
    float baseGlow = (1.0 - hDist * 2.0) * smoothstep(0.8, 1.0, y);
    col.rgb += vec3(1.0) * baseGlow * 0.5;
    
    finalColor = col;
}
