#version 430 core

// Fused tonemap + vignette + color grading pass.
// Replaces three full-screen passes (tonemap.frag, vignette.frag,
// color_grading.frag) with a single draw: HDR scene + bloom -> ACES tonemap
// -> gamma -> vignette -> LUT color grading. FXAA stays a separate final pass
// because it needs neighborhood sampling.

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uHDRScene;
uniform sampler2D uBloomTexture;
uniform float uBloomIntensity;
uniform float uExposure;

uniform sampler2D uLutTexture;
uniform float uGradingIntensity;
uniform int uLutSize;
uniform int uColorGradingEnabled;

uniform float uVignetteIntensity;
uniform float uVignetteRadius;
uniform int uVignetteEnabled;

vec3 ACESFilmic(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec2 LutUv(vec3 color, float blueSlice, float size) {
    float x = (color.r * (size - 1.0) + blueSlice * size + 0.5) / (size * size);
    float y = (color.g * (size - 1.0) + 0.5) / size;
    return vec2(x, y);
}

void main() {
    vec3 hdr = texture(uHDRScene, vTexCoord).rgb;
    vec3 bloom = texture(uBloomTexture, vTexCoord).rgb;
    vec3 color = (hdr + bloom * uBloomIntensity) * uExposure;

    // Tonemap + gamma
    color = ACESFilmic(color);
    color = pow(color, vec3(1.0 / 2.2));

    // Vignette (matches vignette.frag)
    if (uVignetteEnabled > 0) {
        vec2 uv = vTexCoord * 2.0 - 1.0;
        float dist = length(uv);
        float vignette = smoothstep(uVignetteRadius, uVignetteRadius - 0.45, dist);
        color *= mix(1.0, vignette, uVignetteIntensity);
    }

    // Color grading (matches color_grading.frag)
    if (uColorGradingEnabled > 0) {
        vec3 scene = clamp(color, 0.0, 1.0);
        float size = max(float(uLutSize), 1.0);
        float blue = scene.b * (size - 1.0);
        float blue0 = floor(blue);
        float blue1 = min(blue0 + 1.0, size - 1.0);
        float t = blue - blue0;

        vec3 graded0 = texture(uLutTexture, LutUv(scene, blue0, size)).rgb;
        vec3 graded1 = texture(uLutTexture, LutUv(scene, blue1, size)).rgb;
        vec3 graded = mix(graded0, graded1, t);
        color = mix(scene, graded, clamp(uGradingIntensity, 0.0, 1.0));
    }

    // High frequency interleaved gradient noise dither to eliminate 8-bit banding
    float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    color += (noise - 0.5) * (1.0 / 255.0);

    FragColor = vec4(color, 1.0);
}
