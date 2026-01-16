#version 430

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;      // Background/Screen texture
uniform sampler2D normalMap;     // Normal map for distortion wave
uniform float time;
uniform float distortionStrength;

out vec4 finalColor;

void main()
{
    // Sample normal map with scaling and scrolling
    vec2 uv = fragTexCoord * 3.0 + vec2(time * 0.1, sin(time * 0.2) * 0.1);
    vec3 normal = texture(normalMap, uv).rgb * 2.0 - 1.0;
    
    // Offset screen UVs based on normal vector
    vec2 offset = normal.xy * distortionStrength;
    
    // Smoothly fade out distortion towards the edges of the quad
    float edgeFade = smoothstep(0.5, 0.3, length(fragTexCoord - vec2(0.5)));
    offset *= edgeFade;
    
    // Sample the background screen texture
    vec4 screenColor = texture(texture0, fragTexCoord + offset);
    
    // Apply vertex color tint
    finalColor = screenColor * fragColor;
}
