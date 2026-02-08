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

    float noise = sin((uv.x + cameraOffset.x * 0.0002) * 19.0 + time * 0.6) *
                  cos((uv.y + cameraOffset.y * 0.0002) * 17.0 - time * 0.4);
    float fog = smoothstep(0.1, 1.0, uv.y) * 0.4 + noise * 0.08;

    vec3 fogColor = vec3(0.03, 0.02, 0.06);
    finalColor = vec4(mix(base.rgb, fogColor, clamp(fog, 0.0, 0.7)), base.a);
}
