#version 430 core

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D uFontAtlas;

out vec4 finalColor;

float median3(vec3 v) {
    return max(min(v.r, v.g), min(max(v.r, v.g), v.b));
}

float screenPxRange(vec2 texCoord) {
    vec2 texSize = vec2(textureSize(uFontAtlas, 0));
    vec2 unitRange = vec2(4.0) / texSize;
    vec2 screenTexSize = vec2(1.0) / fwidth(texCoord);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    vec3 sdf = texture(uFontAtlas, fragTexCoord).rgb;
    float sd = median3(sdf);
    float dist = sd - 0.5;
    float pxRange = screenPxRange(fragTexCoord);
    float alpha = clamp(dist * pxRange + 0.5, 0.0, 1.0);
    finalColor = vec4(fragColor.rgb, fragColor.a * alpha);
    if (finalColor.a < 0.01) {
        discard;
    }
}
