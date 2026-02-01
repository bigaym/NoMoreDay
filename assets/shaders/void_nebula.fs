#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform float uTime;
uniform vec2 uOffset;
uniform vec2 uResolution;

out vec4 finalColor;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p); vec2 f = fract(p);
    float a = hash(i); float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0)); float d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm(vec2 p) {
    float v = 0.0; float a = 0.5;
    for (int i = 0; i < 5; ++i) {
        v += a * noise(p);
        p = p * 2.0 + vec2(10.0);
        a *= 0.5;
    }
    return v;
}

void main() {
    vec2 uv = fragTexCoord;
    vec2 p = (uv * 2.0 - 1.0);
    float aspect = uResolution.y > 0.0 ? uResolution.x / uResolution.y : 1.0;
    p.x *= aspect;
    
    // Parallax nebula
    vec2 p1 = p * 0.6 + uOffset * 0.0001;
    vec2 p2 = p * 1.5 + uOffset * 0.0002;
    float n1 = fbm(p1 + uTime * 0.02);
    float n2 = fbm(p2 - uTime * 0.01);
    
    // MUCH BRIGHTER COLORS
    vec3 color1 = vec3(0.1, 0.05, 0.25);  // Space Purple
    vec3 color2 = vec3(0.2, 0.1, 0.4);   // Nebula Pink
    vec3 color3 = vec3(0.1, 0.2, 0.5);   // Astral Blue
    
    vec3 nebula = mix(color1, color2, n1);
    nebula = mix(nebula, color3, n2 * 0.6);
    nebula += color2 * pow(n1 * n2, 1.2) * 1.5; // Brighter glow
    
    // Ambient light
    nebula += vec3(0.05, 0.05, 0.1);

    // Dynamic Stars
    float stars = 0.0;
    vec2 starP = p * 20.0 + uOffset * 0.0005;
    float s = hash(floor(starP));
    if (s > 0.99) {
        float pulse = 0.5 + 0.5 * sin(uTime * 3.0 + s * 10.0);
        stars = pow(noise(starP * 5.0), 10.0) * 2.0 * pulse;
    }
    
    finalColor = vec4(nebula + stars, 1.0) * fragColor;
}
