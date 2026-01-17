#version 430

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform float time;
uniform float intensity; // 0.0 to 1.0 (how strong the gold flow is)

out vec4 finalColor;

void main() {
    vec4 baseColor = texture(texture0, fragTexCoord);
    if (baseColor.a < 0.01) discard;

    // Golden flowing light effect
    // We create a diagonal moving "band" of light
    float flow = fract(time * 0.5);
    
    // Diagonal coordinate
    float d = (fragTexCoord.x + fragTexCoord.y) * 0.5;
    
    // Shine band logic
    float shine = smoothstep(flow - 0.2, flow, d) * (1.0 - smoothstep(flow, flow + 0.2, d));
    
    // Add pulsing base glow
    float pulse = 0.5 + 0.5 * sin(time * 3.0);
    
    vec3 goldColor = vec3(1.0, 0.85, 0.3); // Bright Gold
    
    // Composite
    // baseColor + shine * goldColor * intensity
    // We also want to tint the base color slightly towards gold if it's at max stacks
    vec3 tintedBase = mix(baseColor.rgb, goldColor, intensity * 0.2 * pulse);
    
    vec3 finalRGB = tintedBase + goldColor * shine * intensity * 1.5;
    
    finalColor = vec4(finalRGB, baseColor.a);
}
