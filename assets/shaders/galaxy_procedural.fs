#version 330

// === CYBER CULTIVATION GALAXY SHADER ===
// Style: "Ink & Void" (Dark Background, Cyan/White Energy, Sharp Filaments)
// Tech: Minimized Void Eye + Sharp Photon Frontier + Subliminal Accretion
// Update: v4.4 - Tightened accretion range to 1/3 (max 0.07 distance)

in vec2 fragTexCoord;
in vec4 fragColor;

uniform float uTime;
uniform vec2 uResolution;
uniform vec2 uOffset;      // camera.target
uniform float uZoom;       // camera.zoom
uniform vec2 uGalaxyCenter;
uniform float uGalaxyScale;

out vec4 finalColor;

// === CONSTANTS & PALETTE ===
const vec3 C_INK_BG    = vec3(0.005, 0.008, 0.012); 
const vec3 C_CORE_HOT  = vec3(0.6, 0.9, 0.95);
const vec3 C_ARM_PRI   = vec3(0.0, 0.75, 0.9);
const vec3 C_ARM_SEC   = vec3(0.1, 0.25, 0.55);

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
    for (int i = 0; i < 5; i++) { 
        f += amp * noise(p);
        p = m2 * p * 2.02;
        amp *= 0.5;
    }
    return f;
}

float ridged_fbm(vec2 p) {
    float f = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 4; i++) {
        float n = 1.0 - abs(noise(p) * 2.0 - 1.0); 
        n = pow(n, 2.0); 
        f += n * amp;
        p = m2 * p * 2.02;
        amp *= 0.5;
    }
    return f;
}

// === GALAXY STRUCTURE ===
vec3 GetGalaxyStructure(vec2 uv) {
    float r = length(uv);
    float warpAmount = 0.2 * smoothstep(0.05, 0.4, r);
    float warp = fbm(uv * 1.5 - uTime * 0.01);
    vec2 warpedUV = uv + vec2(warp - 0.5) * warpAmount; 
    
    float angle = atan(warpedUV.y, warpedUV.x);
    float dist = length(warpedUV);
    
    float numArms = 2.0;
    float twist = 12.0; 
    float spiralPhase = angle * numArms + twist * log(dist + 0.001); 
    
    float armSignal = cos(spiralPhase - uTime * 0.15); 
    float armDensity = smoothstep(-0.8, 0.6, armSignal); 
    
    float dustSignal = cos(spiralPhase - 0.5 - uTime * 0.15); 
    float dustDensity = smoothstep(0.4, 0.9, dustSignal); 
    
    float core = exp(-dist * dist * 8.0);
    
    return vec3(armDensity, dustDensity, core);
}

// === NEBULA RENDERING ===
vec3 RenderNebula(vec2 uv, vec3 structure) {
    float arm = structure.x;
    float dust = structure.y;
    float core = structure.z;
    float r = length(uv);
    float microDetail = fbm(uv * 40.0 + uTime * 0.05); 
    float ambientGas = 0.25 * fbm(uv * 2.0) * exp(-r * 2.0); 
    float gasDensity = arm * mix(0.5, 1.2, fbm(uv * 3.0 + uTime * 0.02));
    gasDensity += ambientGas; 
    vec3 gasColor = mix(C_ARM_SEC, C_ARM_PRI, structure.x * (0.6 + 0.4 * microDetail));
    float dustNoise = ridged_fbm(uv * 4.0 - uTime * 0.02);
    float strongDust = dust * smoothstep(0.2, 0.8, dustNoise);
    vec3 finalGas = gasColor * gasDensity * 0.6; 
    finalGas += C_CORE_HOT * core * 0.3; 
    finalGas *= (1.0 - strongDust * 0.8); 
    float fade = exp(-r * 0.8);
    return finalGas * fade;
}

// === STAR RENDERING ===
vec3 RenderStarLayer(vec2 uv, float scale, vec3 structure, float seedOffset) {
    vec2 gridUV = uv * scale;
    vec2 gridID = floor(gridUV);
    vec2 gridFract = fract(gridUV);
    vec3 col = vec3(0.0);
    for(int y = -1; y <= 1; y++) {
        for(int x = -1; x <= 1; x++) {
            vec2 neighbour = vec2(float(x), float(y));
            vec2 id = gridID + neighbour;
            float h = hash12(id + vec2(seedOffset)); 
            vec2 pos = neighbour + vec2(hash12(id * 1.54), hash12(id * 2.56));
            vec2 diff = pos - gridFract;
            float dist = length(diff);
            float glow = 0.003 / (dist * dist + 0.0001);
            glow *= smoothstep(0.8, 0.0, dist);
            float twinkle = 0.7 + 0.3 * sin(uTime * 2.0 + h * 10.0);
            vec2 starWorldUV = (id + vec2(0.5)) / scale;
            vec3 localStruct = GetGalaxyStructure(starWorldUV);
            float densityProb = max(localStruct.x, localStruct.z * 0.5); 
            float voidCutoff = 0.95 - (densityProb * 0.7); 
            if (h > voidCutoff) { 
                vec3 cStar = mix(C_ARM_SEC, C_CORE_HOT, densityProb * 0.7 + h * 0.3);
                if (h > 0.98) { cStar = vec3(1.0); glow *= 2.0; }
                col += cStar * glow * twinkle;
            }
        }
    }
    return col;
}

void main() {
    // 1. Transform Setup
    vec2 screenPos = vec2(gl_FragCoord.x, uResolution.y - gl_FragCoord.y);
    vec2 screenCenter = uResolution * 0.5;
    vec2 worldPos = (screenPos - screenCenter) / uZoom + uOffset;
    vec2 p = (worldPos - uGalaxyCenter) * uGalaxyScale;
    float theta = uTime * 0.02; 
    float c = cos(theta); float s = sin(theta);
    p = mat2(c, s, -s, c) * p;
    
    // 2. Local UV and Radial calc
    vec2 uv = p * 0.3333; 
    float r = length(uv);
    float angle = atan(uv.y, uv.x);
    
    // --- BLACK HOLE PARAMETERS ---
    float bhRadius = 0.025; 
    
    // Subtle Lensing
    float lensFactor = 0.4 * exp(-r * 5.0);
    vec2 lensingUV = uv * (1.0 + lensFactor);
    
    // 3. Render Components
    vec3 structData = GetGalaxyStructure(lensingUV);
    vec3 accColor = vec3(0.0);
    
    accColor += RenderNebula(lensingUV, structData);
    accColor += RenderStarLayer(lensingUV, 15.0, structData, 1.0) * 0.1;
    accColor += RenderStarLayer(lensingUV, 45.0, structData, 2.0) * 0.7; 
    accColor += RenderStarLayer(lensingUV, 130.0, structData, 3.0) * 0.5; 
    
    // --- REFINED DEVOURING FLOW (Radius tightened to 1/3) ---
    float accretionTwist = angle + uTime * 2.5 + 10.0 * log(r + bhRadius * 0.5);
    float devouringFibers = smoothstep(0.6, 1.0, sin(accretionTwist * 3.0 + fbm(uv * 15.0)));
    // Radius reduced from 0.2 to 0.07 (approx 1/3)
    float innerGlowMask = smoothstep(bhRadius + 0.07, bhRadius + 0.01, r);
    accColor += C_ARM_PRI * devouringFibers * innerGlowMask * 0.5;
    
    // Sharp Photon Frontier (Ring)
    float ringW = 0.0025; 
    float ringIntensity = 0.012 / (abs(r - bhRadius) + ringW);
    ringIntensity *= smoothstep(bhRadius - 0.002, bhRadius, r); 
    ringIntensity *= smoothstep(bhRadius + 0.15, bhRadius, r); 
    accColor += C_CORE_HOT * ringIntensity * 2.5;

    // 4. Final Final Passage
    vec3 outColor = C_INK_BG + accColor;
    
    // PERFECT HORIZON MASK
    float horizonMask = smoothstep(bhRadius, bhRadius + 0.002, r); 
    outColor *= horizonMask;
    
    // Shadow depth near the horizon (Radius tightened from 0.15 to 0.05)
    float voidDepth = smoothstep(bhRadius + 0.05, bhRadius + 0.01, r);
    outColor = mix(outColor, vec3(0.0), voidDepth * 0.7);

    // Boundary fade
    outColor *= smoothstep(2.5, 1.2, r); 
    
    finalColor = vec4(outColor, 1.0);
}
