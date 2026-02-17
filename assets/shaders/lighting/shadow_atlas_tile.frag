#version 430 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform vec2 uTileOriginPx;
uniform float uTileSize;
uniform vec2 uLightPos;
uniform float uLightRadius;
uniform vec2 uCameraOffset;
uniform vec2 uScreenSize;

void main() {
    vec2 tileLocalUv = (gl_FragCoord.xy - uTileOriginPx) / max(uTileSize, 1.0);
    vec2 worldPos = uCameraOffset + tileLocalUv * uScreenSize;
    float dist = length(worldPos - uLightPos);
    float radius = max(uLightRadius, 0.001);
    float depthMask = clamp(1.0 - dist / radius, 0.0, 1.0);
    fragColor = vec4(depthMask, depthMask, depthMask, 1.0);
}
