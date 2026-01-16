#version 430

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;      // Background/Screen texture
uniform sampler2D normalMap;     // Normal map for distortion wave
uniform float time;
uniform float distortionStrength;

// Vignette / Ink Smear support
uniform float vignetteStrength; // 0.0 to 1.0
uniform vec3 vignetteColor;     // Usually dark cyan or black

out vec4 finalColor;

void main()
{
    // Sample normal map with scaling and scrolling
    vec2 uv = fragTexCoord * 3.0 + vec2(time * 0.1, sin(time * 0.2) * 0.1);
    vec3 normal = texture(normalMap, uv).rgb * 2.0 - 1.0;
    
    // Offset screen UVs based on normal vector
    vec2 offset = normal.xy * distortionStrength;
    
    // Smoothly fade out distortion towards the edges of the quad (for particle usage)
    // For fullscreen usage, we might want to disable this edgeFade or control it.
    // Assuming this shader is used for both, we keep it but maybe we need a flag?
    // For now, let's keep the edge fade for the distortion part as it usually looks better even fullscreen 
    // to avoid clamping artifacts at screen edges if not clamped properly.
    float edgeFade = smoothstep(0.5, 0.3, length(fragTexCoord - vec2(0.5)));
    offset *= edgeFade;
    
    // Sample the background screen texture
    vec4 screenColor = texture(texture0, fragTexCoord + offset);
    
    // Apply Vignette
    vec2 d = abs(fragTexCoord - 0.5) * 2.0; // 0 center, 1 edge
    d = pow(d, vec2(2.0)); // Curve
    float dist = length(d);
    float vignette = smoothstep(0.4, 1.4, dist) * vignetteStrength;
    
    // Mix screen color with vignette color
    vec3 mixedColor = mix(screenColor.rgb, vignetteColor, vignette);
    
    // Apply vertex color tint
    finalColor = vec4(mixedColor, screenColor.a) * fragColor;
}
