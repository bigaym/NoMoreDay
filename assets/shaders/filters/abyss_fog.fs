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
    
    // Smooth transition for vision
    // visionRadius is in world units or pixels? Spec says radius.
    // If it's world units, we need to multiply by zoom.
    float effectiveRadius = visionRadius; 
    float visibility = 1.0 - smoothstep(effectiveRadius * 0.6, effectiveRadius, dist);
    
    // Add some animated noise to the fog edge
    float noise = sin(fragTexCoord.x * 20.0 + time) * cos(fragTexCoord.y * 20.0 - time * 0.5) * 0.05;
    visibility = clamp(visibility + noise, 0.0, 1.0);

    // Deep abyss color
    vec3 fogColor = vec3(0.02, 0.01, 0.04);
    
    // Mix scene with fog
    finalColor = vec4(mix(fogColor, base.rgb, visibility), base.a);
}
