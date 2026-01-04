#version 430
in vec4 fragColor;
in vec2 localPos;
out vec4 finalColor;

void main() {
    float distSq = dot(localPos, localPos);
    if (distSq > 1.0) discard;
    
    float delta = fwidth(distSq);
    float alpha = 1.0 - smoothstep(1.0 - delta, 1.0, distSq);
    
    finalColor = vec4(fragColor.rgb, fragColor.a * alpha);
}
