#version 430 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uSource;

void main() {
    vec2 texel = 1.0 / vec2(textureSize(uSource, 0));
    vec3 c = texture(uSource, vTexCoord + vec2(-texel.x * 2.0, 0.0)).rgb;
    c += texture(uSource, vTexCoord + vec2(-texel.x, texel.y)).rgb * 2.0;
    c += texture(uSource, vTexCoord + vec2(0.0, texel.y * 2.0)).rgb;
    c += texture(uSource, vTexCoord + vec2(texel.x, texel.y)).rgb * 2.0;
    c += texture(uSource, vTexCoord + vec2(texel.x * 2.0, 0.0)).rgb;
    c += texture(uSource, vTexCoord + vec2(texel.x, -texel.y)).rgb * 2.0;
    c += texture(uSource, vTexCoord + vec2(0.0, -texel.y * 2.0)).rgb;
    c += texture(uSource, vTexCoord + vec2(-texel.x, -texel.y)).rgb * 2.0;
    FragColor = vec4(c / 12.0, 1.0);
}
