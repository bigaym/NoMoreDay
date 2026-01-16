#version 430

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;    // Trail mask/texture
uniform vec4 glowColor;       // Base glow color
uniform float glowIntensity;  // 0.0 to 1.0 (or higher for bloom)

out vec4 finalColor;

void main()
{
    // Sample the trail mask
    vec4 mask = texture(texture0, fragTexCoord);
    
    // Combine mask alpha with vertex alpha (for fading)
    float alpha = mask.a * fragColor.a;
    
    // We don't want to render completely transparent pixels
    if (alpha <= 0.0) discard;
    
    // Calculate final color
    // Mix the vertex color (which often represents the skill element)
    // with a glow color for extra pop
    vec3 color = mix(fragColor.rgb, glowColor.rgb, glowIntensity);
    
    // Apply texture detail
    color *= mask.rgb;
    
    finalColor = vec4(color, alpha);
}
