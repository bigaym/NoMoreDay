#version 430 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSceneTexture;
uniform sampler2D uDistortionTexture;
uniform float uDistortionScale;

void main() {
    vec2 distortion = texture(uDistortionTexture, vTexCoord).rg;
    vec2 uv = clamp(vTexCoord + distortion * uDistortionScale, 0.0, 1.0);
    FragColor = texture(uSceneTexture, uv);
}
