#version 430 core

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D uAtlas;

out vec4 finalColor;

void main() {
    vec4 tex = texture(uAtlas, fragTexCoord);
    
    // Use texture alpha but apply vertex color
    finalColor = vec4(fragColor.rgb, tex.a * fragColor.a);
    
    // Discard fully transparent pixels to avoid depth sorting issues if needed
    if (finalColor.a < 0.01) discard;
}
