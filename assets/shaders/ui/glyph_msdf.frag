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
    // 兜底：uScreenPxRange 未注入（默认 0）时按 atlas 的典型 distanceRange=4
    // 以 1:1 纹理密度近似，避免 alpha 恒为 0.5 导致文字批退化成半透明色块。
    // 注意：这里刻意不用 fwidth/textureSize 做精确估算——带导数指令的变体
    // 在部分驱动上与 GL timer query 交互异常（S1a 单 GPU timer 回归测试中
    // 触发 CpuFallback）；正常注入链下 uScreenPxRange 总是有效的，常量兜底
    // 仅为防御性回退。
    float pxr = uScreenPxRange > 0.0 ? uScreenPxRange : 4.0;
    float a = clamp((sd - 0.5) * pxr + 0.5, 0.0, 1.0);
    finalColor = vec4(fragColor.rgb, fragColor.a * a);
    if (finalColor.a < 0.01) discard;
}
