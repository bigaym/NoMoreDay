#version 430

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float time;
uniform vec2 cameraOffset;
uniform float zoom;
uniform vec2 screenSize;
uniform vec2 playerPos;      // Player position in screen space (normalized 0-1 or pixels)
uniform float visionRadius;  // Vision radius in pixels

void main() {
    vec4 base = texture(texture0, fragTexCoord) * fragColor;
    
    // Calculate fragment position in pixels
    vec2 fragPos = fragTexCoord * screenSize;
    
    // Calculate distance to player in pixels
    float dist = length(fragPos - playerPos);
    
    // FogOfWar owns the actual visibility mask. Keep this filter as a subtle
    // edge treatment so limited-vision biomes do not apply a second opaque fog.
    float effectiveRadius = max(visionRadius, 1.0);
    float edge = smoothstep(effectiveRadius * 0.7, effectiveRadius, dist);

    // Use a neutral, low-strength tint. This preserves scene colors and alpha
    // while softening the hard grid boundary produced by the fog texture.
    vec3 fogColor = vec3(0.12, 0.12, 0.14);
    float edgeStrength = edge * 0.35;
    finalColor = vec4(mix(base.rgb, fogColor, edgeStrength), base.a);
}
