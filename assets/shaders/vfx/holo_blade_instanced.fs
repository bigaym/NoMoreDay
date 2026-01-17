#version 430

in vec2 fragTexCoord;
in vec4 fragHoloColor;
in float fragRimStrength;
in float fragNoiseSpeed;

uniform sampler2D texture0;    // Base blade texture (alpha mask)
uniform sampler2D noiseTex;    // Energy noise texture
uniform float time;

out vec4 finalColor;

void main()
{
    // Sample base alpha mask
    vec4 texel = texture(texture0, fragTexCoord);
    if (texel.a < 0.05) discard;

    // Dual scrolling noise for organic energy effect
    float n1 = texture(noiseTex, fragTexCoord * 2.0 + vec2(0.3, time * fragNoiseSpeed)).r;
    float n2 = texture(noiseTex, fragTexCoord * 1.5 - vec2(time * 0.2 * fragNoiseSpeed, 0.4)).r;
    float energy = (n1 + n2) * 0.5;

    // 2D Rim/Edge Light based on UV distance from center
    float edgeDist = length(fragTexCoord - vec2(0.5));
    float rim = smoothstep(0.2, 0.5, edgeDist) * fragRimStrength;

    // Suble high-frequency flicker
    float flicker = sin(time * 30.0) * 0.03 + 0.97;

    // Create the holographic look
    vec3 baseCore = fragHoloColor.rgb * energy * 1.5;
    baseCore += fragHoloColor.rgb * rim;
    baseCore += texel.rgb * 0.3;

    finalColor = vec4(baseCore * flicker, texel.a * fragHoloColor.a);
}
