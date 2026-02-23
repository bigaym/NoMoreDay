#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform float uTime;
uniform vec2 uResolution;
uniform vec2 uOffset;
uniform float uZoom;
uniform vec2 uCameraOffset;
uniform vec2 uGalaxyCenter;
uniform float uGalaxyScale;
uniform int uQualityTier;

out vec4 finalColor;

const vec3 C_BG        = vec3(0.002, 0.004, 0.008); 
const vec3 C_CORE_IN   = vec3(1.00,  0.95,  0.80);  // Warm white/yellow core
const vec3 C_CORE_OUT  = vec3(0.65,  0.85,  1.00);  // Bright celestial blue
const vec3 C_ARM_BASE  = vec3(0.05,  0.15,  0.45);  // Deep space blue
const vec3 C_ARM_BRIGHT= vec3(0.15,  0.65,  1.00);  // Bright cyan/blue
const vec3 C_H2_REGION = vec3(1.00,  0.15,  0.45);  // Pink star-forming regions
const vec3 C_DUST      = vec3(0.02,  0.03,  0.04);  // Subtle dust

float hash(float n) { return fract(sin(n) * 43758.5453123); }
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
    float a = 0.5;
    for (int i = 0; i < 4; ++i) { 
        f += a * noise(p);
        p = m2 * p * 2.02;
        a *= 0.5;
    }
    return f;
}

float ridged(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += (1.0 - abs(noise(p) * 2.0 - 1.0)) * a;
        p = m2 * p * 2.03;
        a *= 0.5;
    }
    return v;
}

mat2 rot2(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat2(c, -s, s, c);
}

void main() {
    vec2 screenPos = vec2(gl_FragCoord.x, uResolution.y - gl_FragCoord.y);
    vec2 worldPos = (screenPos - uCameraOffset) / uZoom + uOffset;
    vec2 p = (worldPos - uGalaxyCenter) * uGalaxyScale;
    vec2 uv = p * 0.3333; 
    float r = max(length(uv), 0.0001);
    float bhRadius = 0.015;

    // Gravitational Lensing effect for background structures
    vec2 lensedUv = uv * (1.0 + (bhRadius * bhRadius) / (r * r + 0.0001) * 0.6);

    // Rigid pattern rotation (stops the galaxy from winding up into a smear over time)
    float baseRotation = uTime * 0.05; 
    vec2 l = rot2(baseRotation) * lensedUv;
    
    // Gentle organic warp (reduced to prevent center from distorting too much)
    l += vec2(fbm(l * 1.5) - 0.5, fbm(l * 1.5 + vec2(4.1, -2.3)) - 0.5) * 0.04 * r;
    
    float rad = max(length(l), 0.0001);
    float ang = atan(l.y, l.x);

    // --- Core / Bulge ---
    float coreVoid = smoothstep(bhRadius + 0.008, bhRadius + 0.05, r);
    float bulgeFactor = (exp(-rad * rad * 45.0) * 1.2 + exp(-rad * 5.0) * 0.35) * coreVoid; // Enforce dark void around BH
    vec3 bulgeColor = mix(C_CORE_OUT, C_CORE_IN, smoothstep(0.0, 0.6, bulgeFactor));
    
    // --- Spiral Arms (Density Wave) ---
    float arms = 2.0; 
    float winding = 11.0; // Increased to wrap more tightly and appear more circular
    
    float phaseNoise = (fbm(l * 4.0) - 0.5) * 1.5 * smoothstep(0.05, 1.2, rad);
    float spiralPhase = ang * arms + log(rad + 0.02) * winding + phaseNoise - uTime * 0.04;
    
    // Wider arms
    float armSignal = cos(spiralPhase);
    float armMask = smoothstep(-0.2, 0.8, armSignal);
    
    float spurPhase = ang * 4.0 + log(rad + 0.01) * (winding * 1.5) + fbm(l * 6.0) * 1.5;
    float spurMask = smoothstep(0.5, 1.0, cos(spurPhase)) * 0.5;
    
    float totalArmMask = clamp(armMask + spurMask, 0.0, 1.0);

    // Glowing disk that fills the gaps
    float diskMask = exp(-rad * 2.5) * 0.3; // Reduced to lower the whiteout level

    // --- Gaseous Nebulae ---
    float cloudNoise = fbm(l * 5.0 - vec2(uTime * 0.01));
    float gasDensity = totalArmMask * cloudNoise * exp(-rad * 1.2) * 2.0; 
    gasDensity += 0.30 * fbm(l * 3.0) * exp(-rad * 1.5); // Slightly less ambient gas
    gasDensity += diskMask;
    gasDensity *= coreVoid; // Remove gas from the immediate accretion zone
    
    vec3 gasColor = mix(C_ARM_BASE, C_ARM_BRIGHT, smoothstep(0.1, 0.9, cloudNoise * totalArmMask + diskMask));
    
    // Pink H-II Regions
    float h2Noise = fbm(l * 15.0 + vec2(1.0, -1.0));
    float h2Mask = smoothstep(0.7, 0.95, h2Noise) * totalArmMask * exp(-rad * 1.0);
    gasColor = mix(gasColor, C_H2_REGION, h2Mask);

    // --- Dust Lanes / Extinction ---
    float dustPhase = spiralPhase - 0.5; 
    float dustSignal = cos(dustPhase);
    float dustLane = smoothstep(0.6, 1.0, dustSignal); // Thinner dust lanes
    
    float clumpNoise = ridged(l * 10.0 + vec2(uTime * 0.01));
    float dustDensity = dustLane * clumpNoise * exp(-rad * 1.2) * totalArmMask * 1.2;
    dustDensity += ridged(l * 15.0) * exp(-rad * 3.0) * 0.5;
    
    // --- Compositing ---
    vec3 finalRGB = C_BG;
    
    finalRGB += gasColor * gasDensity;
    finalRGB += bulgeColor * bulgeFactor;
    
    float extinction = exp(-dustDensity * 2.0); // Softer extinction
    finalRGB *= extinction;
    
    // Forward scattering 
    finalRGB += C_DUST * dustDensity * 0.5 * bulgeFactor; 
    
    float eventHorizonMask = smoothstep(bhRadius - 0.003, bhRadius + 0.003, r);
    finalRGB *= eventHorizonMask; 
    
    float vignette = smoothstep(2.6, 1.2, r);
    finalRGB *= vignette;

    finalColor = vec4(finalRGB, 1.0);
}
