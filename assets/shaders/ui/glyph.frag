#version 430 core

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D uFontAtlas;

out vec4 finalColor;

void main() {
    // Universal font mask: works for Alpha-only, Grayscale, and RGBA fonts.
    // tex.a handles transparent background fonts.
    // tex.r handles grayscale fonts where alpha is 1.0.
    // Do not use tex.a * tex.r which causes quadratic alpha falloff under bilinear filtering.
    vec4 tex = texture(uFontAtlas, fragTexCoord);
    float mask = (tex.a < 1.0 || tex.r == 0.0) ? tex.a : tex.r;
    
    finalColor = vec4(fragColor.rgb, fragColor.a * mask);
    
    if (finalColor.a < 0.01) discard;
}

