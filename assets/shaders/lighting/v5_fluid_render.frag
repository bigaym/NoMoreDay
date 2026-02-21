#version 430 core

in vec2 vLocalUv;
in vec2 vWorldPos;
in vec4 vColor;
in float vDensity;

out vec4 FragColor;

uniform sampler2D uRadianceMap;
uniform int uUseRadiance;
uniform vec2 uCameraOffset;
uniform vec2 uScreenSize;
uniform float uRestDensity;

void main() {
    vec2 centered = vLocalUv * 2.0 - 1.0;
    float radial = dot(centered, centered);
    if (radial > 1.0) {
        discard;
    }

    float alpha = smoothstep(1.0, 0.35, radial);
    vec3 color = vColor.rgb;
    if (uUseRadiance != 0 && uScreenSize.x > 0.0 && uScreenSize.y > 0.0) {
        vec2 uv = (vWorldPos - uCameraOffset) / uScreenSize;
        if (uv.x >= 0.0 && uv.y >= 0.0 && uv.x <= 1.0 && uv.y <= 1.0) {
            vec3 indirect = texture(uRadianceMap, uv).rgb;
            color += indirect * 0.22;
        }
    }

    float densityFactor = clamp(vDensity / max(uRestDensity, 0.001), 0.35, 2.2);
    color *= densityFactor;
    FragColor = vec4(color, alpha * 0.72);
}
