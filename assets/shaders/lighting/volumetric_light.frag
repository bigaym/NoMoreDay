#version 430 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uSceneTex;
uniform int uLightCount;
uniform int uSampleCount;
uniform float uScattering;
uniform float uDecay;
uniform float uExposure;
uniform vec2 uCameraOffset;
uniform vec2 uScreenSize;

struct GPULight {
    vec2 position;
    float radius;
    float intensity;
    vec4 color;
};

layout(std430, binding = 9) readonly buffer LightBuffer {
    GPULight lights[];
};

void main() {
    vec3 scene = texture(uSceneTex, vTexCoord).rgb;
    vec2 worldPos = vTexCoord * uScreenSize + uCameraOffset;

    vec3 scatterSum = vec3(0.0);
    int sampleCount = max(uSampleCount, 1);

    for (int i = 0; i < uLightCount; ++i) {
        vec2 lightPos = lights[i].position;
        float radius = max(lights[i].radius, 1e-4);
        float intensity = max(lights[i].intensity, 0.0);
        vec3 lightColor = lights[i].color.rgb;

        vec2 toLight = lightPos - worldPos;
        float distToLight = length(toLight);
        if (distToLight > radius * 2.0 || intensity <= 0.0) {
            continue;
        }

        vec2 stepDir = toLight / float(sampleCount);
        vec2 samplePos = worldPos;
        float weight = 1.0;
        vec3 lightScatter = vec3(0.0);
        for (int s = 0; s < sampleCount; ++s) {
            float dist = length(samplePos - lightPos);
            float radial = 1.0 - clamp(dist / radius, 0.0, 1.0);
            radial = radial * radial;
            lightScatter += lightColor * (intensity * radial * weight);
            weight *= uDecay;
            samplePos += stepDir;
        }

        scatterSum += lightScatter / float(sampleCount);
    }

    vec3 result = scene + scatterSum * uScattering * uExposure;
    fragColor = vec4(result, 1.0);
}
