#version 430 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform int uSourceCount;
uniform vec2 uCameraOffset;
uniform vec2 uScreenSize;

layout(std430, binding = 13) readonly buffer DistortionSourceBuffer {
    float sourceData[];
};

vec4 FetchSource(int index) {
    int base = index * 4;
    return vec4(sourceData[base + 0], sourceData[base + 1],
                sourceData[base + 2], sourceData[base + 3]);
}

vec2 EvaluateSource(vec2 worldPos, vec4 source) {
    float radius = max(source.z, 1.0);
    vec2 delta = worldPos - source.xy;
    float dist = length(delta);
    if (dist >= radius) {
        return vec2(0.0);
    }

    float norm = dist / radius;
    float ring = exp(-pow((norm - 0.75) * 10.0, 2.0));
    float center = exp(-pow(norm * 2.0, 2.0)) * 0.2;
    float amp = (ring + center) * source.w * 0.02;
    vec2 dir = (dist > 1e-4) ? (delta / dist) : vec2(0.0, 1.0);
    return dir * amp;
}

void main() {
    vec2 worldPos = vTexCoord * uScreenSize + uCameraOffset;
    vec2 distortion = vec2(0.0);

    int count = min(uSourceCount, 32);
    for (int i = 0; i < count; ++i) {
        distortion += EvaluateSource(worldPos, FetchSource(i));
    }

    FragColor = vec4(distortion, 0.0, 1.0);
}
