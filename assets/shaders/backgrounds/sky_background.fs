#version 430

in vec2 fragTexCoord;
out vec4 finalColor;

uniform float time;
uniform vec2 cameraOffset;
uniform float zoom;
uniform vec2 screenSize;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2 uv = gl_FragCoord.xy / max(screenSize, vec2(1.0));
    vec2 worldUV = (uv * 2.0 - 1.0) * max(zoom, 0.001);
    worldUV += cameraOffset * 0.0004;

    float starField = step(0.9965, hash12(floor(worldUV * 240.0)));
    float twinkle = 0.6 + 0.4 * sin(time * 2.2 + hash12(worldUV * 10.0) * 12.56);

    float cloud =
        sin(worldUV.x * 2.6 + time * 0.08) *
        cos(worldUV.y * 2.1 - time * 0.05) * 0.5 + 0.5;

    vec3 base = mix(vec3(0.02, 0.03, 0.08), vec3(0.10, 0.16, 0.30), uv.y);
    vec3 clouds = vec3(0.22, 0.30, 0.42) * cloud * 0.28;
    vec3 stars = vec3(0.85, 0.92, 1.0) * starField * twinkle;

    finalColor = vec4(base + clouds + stars, 1.0);
}

