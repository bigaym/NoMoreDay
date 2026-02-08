#version 430

in vec2 fragTexCoord;
out vec4 finalColor;

uniform float time;
uniform vec2 cameraOffset;
uniform float zoom;
uniform vec2 screenSize;

void main() {
    vec2 uv = gl_FragCoord.xy / max(screenSize, vec2(1.0));
    vec2 worldUV = (uv * 2.0 - 1.0) * max(zoom, 0.001);
    worldUV += cameraOffset * 0.00035;

    float causticA = sin(worldUV.x * 8.0 + time * 0.9) * cos(worldUV.y * 6.5 - time * 0.7);
    float causticB = cos(worldUV.x * 11.0 - time * 0.6) * sin(worldUV.y * 9.0 + time * 0.5);
    float caustics = (causticA + causticB) * 0.25 + 0.5;

    float depth = clamp(uv.y * 1.2, 0.0, 1.0);
    vec3 deep = vec3(0.01, 0.08, 0.14);
    vec3 shallow = vec3(0.05, 0.22, 0.30);
    vec3 water = mix(shallow, deep, depth);

    vec3 glow = vec3(0.05, 0.22, 0.28) * caustics * 0.35;
    finalColor = vec4(water + glow, 1.0);
}

