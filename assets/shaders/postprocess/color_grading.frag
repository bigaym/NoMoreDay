#version 430 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSceneTexture;
uniform sampler2D uLutTexture;
uniform float uIntensity;
uniform int uLutSize;

vec2 LutUv(vec3 color, float blueSlice, float size) {
    float x = (color.r * (size - 1.0) + blueSlice * size + 0.5) / (size * size);
    float y = (color.g * (size - 1.0) + 0.5) / size;
    return vec2(x, y);
}

void main() {
    vec3 scene = clamp(texture(uSceneTexture, vTexCoord).rgb, 0.0, 1.0);
    float size = max(float(uLutSize), 1.0);
    float blue = scene.b * (size - 1.0);
    float blue0 = floor(blue);
    float blue1 = min(blue0 + 1.0, size - 1.0);
    float t = blue - blue0;

    vec3 graded0 = texture(uLutTexture, LutUv(scene, blue0, size)).rgb;
    vec3 graded1 = texture(uLutTexture, LutUv(scene, blue1, size)).rgb;
    vec3 graded = mix(graded0, graded1, t);
    vec3 finalColor = mix(scene, graded, clamp(uIntensity, 0.0, 1.0));
    FragColor = vec4(finalColor, 1.0);
}
