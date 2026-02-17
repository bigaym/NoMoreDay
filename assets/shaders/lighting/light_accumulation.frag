#version 430 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uSceneTex;
uniform sampler2D uShadowMaskTex;
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
uniform int uLightCount;
uniform int uShadowEnabled;
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
    uint lightType;
};

layout(std430, binding = 9) readonly buffer LightBuffer {
    GPULight lights[];
};

float calcAttenuation(float dist, float radius) {
    float normalizedDist = dist / radius;
    if (normalizedDist >= 1.0) {
        return 0.0;
    }
    float d2 = normalizedDist * normalizedDist;
    float atten = 1.0 - d2;
    return atten * atten;
}

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
    vec4 sceneColor = texture(uSceneTex, vTexCoord);
    vec2 worldPos = vTexCoord * uScreenSize + uCameraOffset;

    vec3 totalLight = uAmbientColor * uAmbientIntensity;

    for (int i = 0; i < uLightCount; ++i) {
        vec2 lightPos = vec2(lights[i].posX, lights[i].posY);
        float radius = lights[i].radius;
        float intensity = lights[i].intensity;
        vec3 lightColor = vec3(lights[i].colorR, lights[i].colorG, lights[i].colorB);
        uint lightType = lights[i].lightType;

        float dist = distance(worldPos, lightPos);
        float atten = calcAttenuation(dist, radius);
        if (atten <= 0.0 || intensity <= 0.0) {
            continue;
        }

        float shadowFactor = 1.0;
        if (uShadowEnabled != 0) {
            shadowFactor = texture(uShadowMaskTex, vTexCoord).r;
        }

        if (lightType == 2u) {
            // AmbientZone: radial area ambient contribution.
            totalLight += lightColor * intensity * atten;
            continue;
        }

        float spotFactor = 1.0;
        if (lightType == 1u) {
            vec2 toPixel = worldPos - lightPos;
            spotFactor = calcSpotFactor(vec2(lights[i].dirX, lights[i].dirY), toPixel,
                                        lights[i].spotCosHalfAngle);
            if (spotFactor <= 0.0) {
                continue;
            }
        }

        totalLight += lightColor * intensity * atten * spotFactor * shadowFactor;
    }

    fragColor = vec4(sceneColor.rgb * totalLight, sceneColor.a);
}
