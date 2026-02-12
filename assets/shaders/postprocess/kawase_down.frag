#version 430 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSource;

void main() {
    vec2 texel = 1.0 / vec2(textureSize(uSource, 0));
    vec3 c = texture(uSource, vTexCoord).rgb * 4.0;
    c += texture(uSource, vTexCoord + vec2(-texel.x, texel.y)).rgb;
    c += texture(uSource, vTexCoord + vec2(texel.x, texel.y)).rgb;
    c += texture(uSource, vTexCoord + vec2(texel.x, -texel.y)).rgb;
    c += texture(uSource, vTexCoord + vec2(-texel.x, -texel.y)).rgb;
    FragColor = vec4(c / 8.0, 1.0);
}
