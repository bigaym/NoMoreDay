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
    float posX;
    float posY;
    float radius;
    float intensity;
    float colorR;
    float colorG;
    float colorB;
    float colorA;
    float dirX;
    float dirY;
    float spotCosHalfAngle;
    float spotOuterCos;
    uint lightType;
    uint shadowMapIndex;
    uint priority;
    uint flags;
};

layout(std430, binding = 9) readonly buffer LightBuffer {
    GPULight lights[];
};

float calcSpotFactor(vec2 lightDir, vec2 toPixelDir, float spotCosHalfAngle) {
    if (spotCosHalfAngle <= -0.9999) {
        return 1.0;
    }
    float cone = dot(normalize(lightDir), normalize(toPixelDir));
    if (cone <= spotCosHalfAngle) {
        return 0.0;
    }
    float denom = max(1e-4, 1.0 - spotCosHalfAngle);
    float t = clamp((cone - spotCosHalfAngle) / denom, 0.0, 1.0);
    return t * t;
}

void main() {
    vec3 scene = texture(uSceneTex, vTexCoord).rgb;
    vec2 worldPos = vTexCoord * uScreenSize + uCameraOffset;

    vec3 scatterSum = vec3(0.0);
    int sampleCount = max(uSampleCount, 1);

    for (int i = 0; i < uLightCount; ++i) {
        vec2 lightPos = vec2(lights[i].posX, lights[i].posY);
        float radius = max(lights[i].radius, 1e-4);
        float intensity = max(lights[i].intensity, 0.0);
        vec3 lightColor = vec3(lights[i].colorR, lights[i].colorG, lights[i].colorB);
        uint lightType = lights[i].lightType;

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
            float spotFactor = 1.0;
            if (lightType == 1u) {
                vec2 toSample = samplePos - lightPos;
                spotFactor = calcSpotFactor(vec2(lights[i].dirX, lights[i].dirY),
                                            toSample, lights[i].spotCosHalfAngle);
                if (spotFactor <= 0.0) {
                    samplePos += stepDir;
                    weight *= uDecay;
                    continue;
                }
            }
            lightScatter += lightColor * (intensity * radial * weight * spotFactor);
            weight *= uDecay;
            samplePos += stepDir;
        }

        scatterSum += lightScatter / float(sampleCount);
    }

    vec3 result = scene + scatterSum * uScattering * uExposure;
    fragColor = vec4(result, 1.0);
}
