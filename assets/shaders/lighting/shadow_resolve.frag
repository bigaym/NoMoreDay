#version 430 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uShadowSdfTex;
uniform float uShadowSoftness;
uniform vec2 uSdfTexelSize;

float resolveShadowFactor(float sdf, float softness) {
    return clamp(sdf / max(softness, 0.0001), 0.0, 1.0);
}

void main() {
    float softness = max(uShadowSoftness, 0.0001);

    float c = resolveShadowFactor(texture(uShadowSdfTex, vTexCoord).r, softness);
    float x0 = resolveShadowFactor(texture(uShadowSdfTex, vTexCoord + vec2(-uSdfTexelSize.x, 0.0)).r, softness);
    float x1 = resolveShadowFactor(texture(uShadowSdfTex, vTexCoord + vec2(uSdfTexelSize.x, 0.0)).r, softness);
    float y0 = resolveShadowFactor(texture(uShadowSdfTex, vTexCoord + vec2(0.0, -uSdfTexelSize.y)).r, softness);
    float y1 = resolveShadowFactor(texture(uShadowSdfTex, vTexCoord + vec2(0.0, uSdfTexelSize.y)).r, softness);

    float shadowFactor = clamp((c + x0 + x1 + y0 + y1) * 0.2, 0.0, 1.0);
    fragColor = vec4(vec3(shadowFactor), 1.0);
}
