#version 430

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float time;
uniform vec2 cameraOffset;
uniform float zoom;
uniform vec2 screenSize;

void main() {
    vec4 base = texture(texture0, fragTexCoord) * fragColor;
    vec2 uv = gl_FragCoord.xy / max(screenSize, vec2(1.0));

    float pulse = 0.5 + 0.5 * sin(time * 0.8 + uv.y * 8.0);
    vec3 tint = vec3(0.78, 0.92, 1.08);
    vec3 deepTint = vec3(0.10, 0.24, 0.34) * (0.16 + 0.10 * pulse);

    vec3 result = base.rgb * tint + deepTint;
    finalColor = vec4(result, base.a);
}
