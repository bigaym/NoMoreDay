#version 430 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uHDRScene;
uniform sampler2D uBloomTexture;
uniform float uBloomIntensity;
uniform float uExposure;

vec3 ACESFilmic(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uHDRScene, vTexCoord).rgb;
    vec3 bloom = texture(uBloomTexture, vTexCoord).rgb;
    vec3 combined = (hdr + bloom * uBloomIntensity) * uExposure;
    vec3 ldr = ACESFilmic(combined);
    ldr = pow(ldr, vec3(1.0 / 2.2));
    FragColor = vec4(ldr, 1.0);
}
