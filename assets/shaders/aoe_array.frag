#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Custom uniforms
uniform float time;
uniform float radius;
uniform float thickness;
uniform vec4 baseColor;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Convert uv to [-1, 1] range
    vec2 uv = fragTexCoord * 2.0 - 1.0;
    float dist = length(uv);
    
    // Outer edge and inner edge of the ring
    float outer = 1.0;
    float inner = 1.0 - thickness;
    
    // Main circle ring
    float circle = smoothstep(outer, outer - 0.02, dist) * smoothstep(inner, inner + 0.02, dist);
    
    // Add some "energy" movement
    float angle = atan(uv.y, uv.x);
    float energy = sin(angle * 8.0 + time * 3.0) * 0.5 + 0.5;
    energy *= sin(angle * 3.0 - time * 5.0) * 0.5 + 0.5;
    
    // Rotating runes / dashes
    float dashes = step(0.5, sin(angle * 24.0 + time * 2.0));
    float dashRing = smoothstep(inner + 0.05, inner + 0.03, dist) * smoothstep(inner - 0.02, inner, dist);
    
    // Final alpha composition
    float alpha = circle * (0.6 + 0.4 * energy) + (dashRing * dashes * 0.8);
    
    // Center glow (very faint)
    float glow = exp(-dist * 4.0) * 0.2;
    
    if (alpha + glow < 0.01) discard;

    vec4 color = baseColor;
    color.a *= (alpha + glow);
    
    finalColor = color;
}
