#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D uBaseLayer;
uniform sampler2D uDetailLayer;
uniform int uQualityTier;

out vec4 finalColor;

void main() {
    vec2 uv = fragTexCoord;
    vec3 base = texture(uBaseLayer, uv).rgb;
    vec3 detail = texture(uDetailLayer, uv).rgb;

    float detailGain = (uQualityTier >= 2) ? 1.30 : 1.12;
    vec3 color = base + detail * detailGain;

    // Filmic shaping with slightly stronger micro-contrast.
    color = color / (vec3(1.0) + color * 0.15);
    color = pow(max(color, vec3(0.0)), vec3(0.88));
    color *= 1.06;

    finalColor = vec4(color, 1.0) * fragColor;
}
