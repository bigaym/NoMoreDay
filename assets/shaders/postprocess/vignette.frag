#version 430 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSource;
uniform float uIntensity;
uniform float uRadius;

void main() {
    vec3 color = texture(uSource, vTexCoord).rgb;
    vec2 uv = vTexCoord * 2.0 - 1.0;
    float dist = length(uv);
    float vignette = smoothstep(uRadius, uRadius - 0.45, dist);
    color *= mix(1.0, vignette, uIntensity);
    FragColor = vec4(color, 1.0);
}
