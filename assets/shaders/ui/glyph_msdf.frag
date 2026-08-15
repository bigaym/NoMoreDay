#version 430 core

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D uFontAtlas;
uniform float uScreenPxRange;

out vec4 finalColor;

float median3(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    vec3 msd = texture(uFontAtlas, fragTexCoord).rgb;
    float sd = median3(msd.r, msd.g, msd.b);
    // sd in [0,1]; the glyph edge sits at sd = 0.5. Convert the signed
    // distance into a screen-space alpha with the px-range uniform. The MSDF
    // channels are a distance field, so the median is taken before any clamp;
    // only the final alpha is clamped.
    float a = clamp((sd - 0.5) * uScreenPxRange + 0.5, 0.0, 1.0);
    finalColor = vec4(fragColor.rgb, fragColor.a * a);
    if (finalColor.a < 0.01) discard;
}
