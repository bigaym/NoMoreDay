#version 430

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;    // Base blade texture (alpha mask)
uniform sampler2D noiseTex;    // Energy noise texture
uniform float time;
uniform vec4 holoColor;       // The "energy" color
uniform float rimStrength;    // 0..1 edge glow
uniform float noiseSpeed;

out vec4 finalColor;

void main()
{
    // Sample base alpha mask
    vec4 texel = texture(texture0, fragTexCoord);
    if (texel.a < 0.05) discard;

    // Dual scrolling noise for organic energy effect
    float n1 = texture(noiseTex, fragTexCoord * 2.0 + vec2(0.3, time * noiseSpeed)).r;
    float n2 = texture(noiseTex, fragTexCoord * 1.5 - vec2(time * 0.2 * noiseSpeed, 0.4)).r;
    float energy = (n1 + n2) * 0.5;

    // 2D Rim/Edge Light based on UV distance from center
    // Assuming the blade is centered in the texture coordinate space
    float edgeDist = length(fragTexCoord - vec2(0.5));
    float rim = smoothstep(0.2, 0.5, edgeDist) * rimStrength;

    // Suble high-frequency flicker to give it a "digital/spiritual" feel
    float flicker = sin(time * 30.0) * 0.03 + 0.97;

    // Create the holographic look
    // 1. Core energy glow
    vec3 baseCore = holoColor.rgb * energy * 1.2;
    // 2. Add rim glow
    baseCore += holoColor.rgb * rim;
    // 3. Highlight edges using the original texture if it has detailing
    baseCore += texel.rgb * 0.5;

    finalColor = vec4(baseCore * flicker, texel.a * holoColor.a * fragColor.a);
}
