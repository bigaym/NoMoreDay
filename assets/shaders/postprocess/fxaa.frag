#version 430 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSource;
uniform vec2 uTexelSize;

float FxaaLuma(vec3 rgb) {
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec3 rgbM = texture(uSource, vTexCoord).rgb;
    vec3 rgbNW = texture(uSource, vTexCoord + vec2(-uTexelSize.x, -uTexelSize.y)).rgb;
    vec3 rgbNE = texture(uSource, vTexCoord + vec2(uTexelSize.x, -uTexelSize.y)).rgb;
    vec3 rgbSW = texture(uSource, vTexCoord + vec2(-uTexelSize.x, uTexelSize.y)).rgb;
    vec3 rgbSE = texture(uSource, vTexCoord + vec2(uTexelSize.x, uTexelSize.y)).rgb;
    vec3 rgbN = texture(uSource, vTexCoord + vec2(0.0, -uTexelSize.y)).rgb;
    vec3 rgbS = texture(uSource, vTexCoord + vec2(0.0, uTexelSize.y)).rgb;
    vec3 rgbW = texture(uSource, vTexCoord + vec2(-uTexelSize.x, 0.0)).rgb;
    vec3 rgbE = texture(uSource, vTexCoord + vec2(uTexelSize.x, 0.0)).rgb;

    float lumaM = FxaaLuma(rgbM);
    float lumaNW = FxaaLuma(rgbNW);
    float lumaNE = FxaaLuma(rgbNE);
    float lumaSW = FxaaLuma(rgbSW);
    float lumaSE = FxaaLuma(rgbSE);
    float lumaN = FxaaLuma(rgbN);
    float lumaS = FxaaLuma(rgbS);
    float lumaW = FxaaLuma(rgbW);
    float lumaE = FxaaLuma(rgbE);

    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    lumaMin = min(lumaMin, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    lumaMax = max(lumaMax, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    const float edgeThreshold = 0.166;
    const float edgeThresholdMin = 0.0833;
    if (lumaRange < max(edgeThresholdMin, lumaMax * edgeThreshold)) {
        FragColor = vec4(rgbM, 1.0);
        return;
    }

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    const float dirReduceMul = 1.0 / 8.0;
    const float dirReduceMin = 1.0 / 128.0;
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
                          (0.25 * dirReduceMul), dirReduceMin);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    const float spanMax = 8.0;
    dir = clamp(dir * rcpDirMin, vec2(-spanMax), vec2(spanMax)) * uTexelSize;

    vec3 rgbA = 0.5 * (texture(uSource, vTexCoord + dir * (1.0 / 3.0 - 0.5)).rgb +
                       texture(uSource, vTexCoord + dir * (2.0 / 3.0 - 0.5)).rgb
    );
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(uSource, vTexCoord + dir * -0.5).rgb +
                                     texture(uSource, vTexCoord + dir * 0.5).rgb
    );

    float lumaB = FxaaLuma(rgbB);
    if (lumaB < lumaMin || lumaB > lumaMax) {
        FragColor = vec4(rgbA, 1.0);
    } else {
        FragColor = vec4(rgbB, 1.0);
    }
}
