#version 430 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uSceneTex;
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
uniform int uLightCount;
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

float calcAttenuation(float dist, float radius) {
    float normalizedDist = dist / radius;
    if (normalizedDist >= 1.0) {
        return 0.0;
    }
    float d2 = normalizedDist * normalizedDist;
    float atten = 1.0 - d2;
    return atten * atten;
}

void main() {
    vec4 sceneColor = texture(uSceneTex, vTexCoord);
    vec2 worldPos = vTexCoord * uScreenSize + uCameraOffset;

    vec3 totalLight = uAmbientColor * uAmbientIntensity;

    for (int i = 0; i < uLightCount; ++i) {
        vec2 lightPos = lights[i].position;
        float radius = lights[i].radius;
        float intensity = lights[i].intensity;
        vec3 lightColor = lights[i].color.rgb;

        float dist = distance(worldPos, lightPos);
        float atten = calcAttenuation(dist, radius);
        totalLight += lightColor * intensity * atten;
    }

    fragColor = vec4(sceneColor.rgb * totalLight, sceneColor.a);
}
