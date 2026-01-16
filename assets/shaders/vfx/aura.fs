#version 430
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D noiseTexture; // vfx_aura_noise.png
uniform vec4 auraColor;         // Cyan usually
uniform float time;
uniform float intensity;        // 0.0 to 1.0 (Sword Intent Stack normalized)

out vec4 finalColor;

void main()
{
    // Double scrolling noise for "flame" effect
    vec2 uv1 = fragTexCoord + vec2(0.0, -time * 0.5);
    vec2 uv2 = fragTexCoord * 0.8 + vec2(sin(time * 0.2), -time * 0.8);
    
    float noise1 = texture(noiseTexture, uv1).r;
    float noise2 = texture(noiseTexture, uv2).r;
    
    // Combine noise
    float combinedNoise = (noise1 + noise2) * 0.5;
    
    // Mask to create "aura" shape (fade at edges of the quad)
    float dist = length(fragTexCoord - 0.5);
    float alphaMask = smoothstep(0.5, 0.2, dist); 
    
    // Intensity modulation
    float threshold = 0.6 - (intensity * 0.4); 
    float flame = smoothstep(threshold, threshold + 0.2, combinedNoise);
    
    vec4 color = auraColor * flame;
    color.a *= alphaMask * intensity * 1.5; // Boost alpha slightly
    
    finalColor = color * fragColor;
}