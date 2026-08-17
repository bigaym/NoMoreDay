#version 430 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uSceneTex;
uniform sampler2D uHeightFieldTex;
uniform int uHeightShadowSteps;
uniform int uSelfShadowEnabled;
uniform int uSelfShadowSteps;
uniform int uPomEnabled;
uniform int uPomLayers;
uniform vec2 uCameraOffset;
uniform vec2 uScreenSize;
uniform vec2 uHeightWorldOrigin;
uniform vec2 uHeightWorldSize;
// Light direction for the height-field shadow raymarch, driven by the active
// light data (HeightShadowPass resolves the first directional light). The
// initializer preserves the historical hardcoded direction when the uniform is
// never set (e.g. shader hot reload without a CPU-side value).
uniform vec2 uLightDir = vec2(-0.45, -0.75);

float sampleHeightAtWorld(vec2 worldPos) {
    vec2 size = max(uHeightWorldSize, vec2(1.0));
    vec2 uv = clamp((worldPos - uHeightWorldOrigin) / size, vec2(0.0), vec2(1.0));
    return clamp(texture(uHeightFieldTex, uv).r, 0.0, 1.0);
}

float shadowRaymarch(vec2 uv, int steps) {
    if (steps <= 0) {
        return 1.0;
    }
    vec2 worldPos = uv * uScreenSize + uCameraOffset;
    float h0 = sampleHeightAtWorld(worldPos);
    vec2 lightDir = normalize(uLightDir);
    float occlusion = 0.0;
    float stride = 1.0 / float(max(steps, 1));
    for (int i = 1; i <= steps; ++i) {
        float t = float(i) * stride;
        vec2 sampleUv = uv + lightDir * t * 0.03;
        vec2 sampleWorld = clamp(sampleUv, vec2(0.0), vec2(1.0)) * uScreenSize + uCameraOffset;
        float hs = sampleHeightAtWorld(sampleWorld);
        occlusion = max(occlusion, hs - h0 + t * 0.35);
    }
    return clamp(1.0 - occlusion * 0.75, 0.2, 1.0);
}

float selfShadow(vec2 uv, int steps) {
    if (steps <= 0) {
        return 1.0;
    }
    vec2 worldPos = uv * uScreenSize + uCameraOffset;
    float baseHeight = sampleHeightAtWorld(worldPos);
    vec2 dir = normalize(vec2(0.25, 0.85));
    float acc = 0.0;
    float invSteps = 1.0 / float(max(steps, 1));
    for (int i = 1; i <= steps; ++i) {
        float t = float(i) * invSteps;
        vec2 suv = uv + dir * t * 0.015;
        vec2 sampleWorld = clamp(suv, vec2(0.0), vec2(1.0)) * uScreenSize + uCameraOffset;
        float h = sampleHeightAtWorld(sampleWorld);
        acc += max(0.0, h - baseHeight) * invSteps;
    }
    return clamp(1.0 - acc * 1.1, 0.4, 1.0);
}

vec2 parallaxOcclusionMapping(vec2 uv, int layers) {
    if (layers <= 0) {
        return uv;
    }
    float layerDepth = 1.0 / float(max(layers, 1));
    vec2 viewDir = normalize(vec2(0.2, 0.8));
    vec2 delta = viewDir * 0.01 / float(max(layers, 1));
    vec2 curUv = uv;
    float curDepth = 0.0;
    vec2 worldPos = curUv * uScreenSize + uCameraOffset;
    float sampled = sampleHeightAtWorld(worldPos);
    while (curDepth < sampled && curDepth < 1.0) {
        curUv -= delta;
        curUv = clamp(curUv, vec2(0.0), vec2(1.0));
        worldPos = curUv * uScreenSize + uCameraOffset;
        sampled = sampleHeightAtWorld(worldPos);
        curDepth += layerDepth;
    }
    return curUv;
}

void main() {
    vec2 uv = vTexCoord;
    if (uPomEnabled != 0) {
        uv = parallaxOcclusionMapping(uv, uPomLayers);
    }

    vec4 scene = texture(uSceneTex, uv);
    float shadow = shadowRaymarch(uv, uHeightShadowSteps);
    if (uSelfShadowEnabled != 0) {
        shadow *= selfShadow(uv, uSelfShadowSteps);
    }

    fragColor = vec4(scene.rgb * shadow, scene.a);
}
