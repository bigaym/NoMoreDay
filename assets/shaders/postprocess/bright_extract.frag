#version 430 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSource;
uniform float uThreshold;
uniform float uKnee;

float GetLuma(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec3 color = texture(uSource, vTexCoord).rgb;
    float luma = GetLuma(color);
    float knee = max(0.0001, uKnee);
    float soft = clamp((luma - uThreshold + knee) / (2.0 * knee), 0.0, 1.0);
    float contribution = max(luma - uThreshold, 0.0) + soft * soft * knee;
    contribution /= max(luma, 0.0001);
    FragColor = vec4(color * contribution, 1.0);
}
