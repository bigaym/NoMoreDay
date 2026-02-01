#version 330

// Hybrid Galaxy Shader: Combines nebula clouds (from parallax) with star points (from pointcloud)

in vec2 fragTexCoord;
in vec4 fragColor;

uniform float uTime;
uniform vec2 uResolution;
uniform vec2 uOffset;
uniform float uZoom;
uniform vec2 uGalaxyCenter;
uniform float uGalaxyScale;

out vec4 finalColor;

// === NOISE FUNCTIONS ===
float hash(float n) { return fract(sin(n) * 43758.5453123); }
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 x) {
    vec2 p = floor(x);
    vec2 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0;
    return mix(mix(hash(n), hash(n + 1.0), f.x),
               mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y);
}

const mat2 m2 = mat2(0.8, -0.6, 0.6, 0.8);
float fbm(vec2 p) {
    float f = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 4; i++) {
        f += amp * noise(p);
        p = m2 * p * 2.02;
        amp *= 0.5;
    }
    return f;
}

// === NEBULA LAYER (from parallax shader) ===
vec3 GetNebulaLayer(vec2 baseUV, vec2 parallaxOffset, float depth, float speedScale, vec3 color) {
    vec2 parallaxUV = baseUV + (parallaxOffset * depth * 0.2);
    
    float angle = uTime * 0.04 * speedScale;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);
    
    vec2 p = rot * parallaxUV * (1.0 + depth * 0.2);
    
    float n = fbm(p * 2.5 + uTime * 0.01);
    
    float r = length(parallaxUV);
    float mask = smoothstep(5.0, 0.1, r);
    
    float armAngle = atan(parallaxUV.y, parallaxUV.x);
    float arm = cos(armAngle * 2.0 + log(r + 0.001) * 2.5 + angle);
    mask *= pow(arm * 0.5 + 0.5, 1.1);
    
    return color * n * mask;
}

// === STAR DENSITY (from pointcloud shader) ===
float GetGalaxyDensity(vec2 uv) {
    float r = length(uv);
    float angle = atan(uv.y, uv.x);
    
    float speed = 0.05;
    float rotAngle = angle - uTime * speed * (1.0 / (r + 0.1));
    
    float spiralPhase = rotAngle * 2.0 + 2.5 * log(r + 0.001);
    float armSignal = cos(spiralPhase);
    
    return smoothstep(-0.3, 1.0, armSignal);
}

// === STAR RENDERING (from pointcloud shader) ===
vec3 RenderStars(vec2 uv, float scale, float density) {
    vec2 gridUV = uv * scale;
    vec2 gridID = floor(gridUV);
    vec2 gridFract = fract(gridUV);
    
    vec3 col = vec3(0.0);
    
    for(int y = -1; y <= 1; y++) {
        for(int x = -1; x <= 1; x++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 id = gridID + neighbor;
            
            float h = hash12(id);
            vec2 pos = neighbor + vec2(hash12(id * 1.5), hash12(id * 2.5));
            vec2 diff = pos - gridFract;
            float dist = length(diff);
            
            float glow = 0.005 / (dist * dist + 0.0002);
            glow *= smoothstep(0.7, 0.0, dist);
            
            float twinkle = 0.6 + 0.4 * sin(uTime * (2.0 + h * 5.0));
            
            vec2 starWorldUV = (id + vec2(0.5)) / scale;
            float galMask = GetGalaxyDensity(starWorldUV);
            float diskMask = smoothstep(5.0, 0.2, length(starWorldUV));
            
            if (h > (1.0 - density * galMask * diskMask)) {
                // Star color: blue-white to golden
                vec3 c1 = vec3(0.5, 0.7, 1.0);
                vec3 c2 = vec3(1.0, 0.9, 0.6);
                vec3 starColor = mix(c1, c2, hash12(id * 3.3));
                
                col += starColor * glow * twinkle * 0.8;
            }
        }
    }
    return col;
}

void main() {
    vec2 screenPos = vec2(gl_FragCoord.x, uResolution.y - gl_FragCoord.y);
    vec2 screenCenter = uResolution * 0.5;
    vec2 worldPos = (screenPos - screenCenter) / uZoom + uOffset;
    
    vec2 galaxyRelative = worldPos - uGalaxyCenter;
    vec2 uv = galaxyRelative * uGalaxyScale;
    vec2 cameraFromCenter = (uOffset - uGalaxyCenter) * uGalaxyScale;
    
    float r = length(uv);
    
    // === DEEP SPACE BACKGROUND ===
    vec3 col = vec3(0.004, 0.006, 0.015);
    
    // === NEBULA CLOUDS (from parallax) ===
    // Purple/blue outer clouds
    col += GetNebulaLayer(uv, cameraFromCenter, 0.1, 0.15, vec3(0.15, 0.06, 0.25)) * 0.5;
    // Blue mid-layer
    col += GetNebulaLayer(uv, cameraFromCenter, 0.5, 1.0, vec3(0.08, 0.2, 0.45)) * 0.8;
    // Purple/pink inner layer
    col += GetNebulaLayer(uv, cameraFromCenter, 1.0, 1.2, vec3(0.45, 0.3, 0.6)) * 0.9;
    
    // === STAR FIELD (from pointcloud) ===
    // Distant background stars
    col += RenderStars(uv, 8.0, 0.08) * 0.15;
    // Medium density stars
    col += RenderStars(uv, 18.0, 0.4) * 0.8;
    // Dense core stars
    if (r < 2.0) {
        col += RenderStars(uv, 35.0, 0.7) * smoothstep(2.0, 0.0, r) * 0.9;
    }
    
    // === BRIGHT CORE GLOW ===
    float coreGlow = 0.15 / (r * r + 0.15);
    col += vec3(1.0, 0.9, 0.7) * coreGlow * 0.6;
    
    // Soft outer halo
    float halo = 0.05 / (r + 0.25);
    col += vec3(0.9, 0.75, 0.5) * halo * 0.25;
    
    finalColor = vec4(col, 1.0);
}
