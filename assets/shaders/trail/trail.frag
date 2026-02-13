#version 430

in float vProgress;
in vec4 vColor;
in vec2 vTexCoord;

out vec4 finalColor;

void main() {
    float edgeFade = smoothstep(0.0, 0.1, vTexCoord.y) *
                     smoothstep(1.0, 0.9, vTexCoord.y);
    float tailFade = smoothstep(1.0, 0.7, vProgress);

    float alpha = vColor.a * edgeFade * tailFade;
    if (alpha < 0.01) {
        discard;
    }

    finalColor = vec4(vColor.rgb, alpha);
}
