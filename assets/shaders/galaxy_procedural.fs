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
uniform vec2 uCameraOffset; // camera.offset (Added for correct world mapping)
uniform vec2 uGalaxyCenter;
uniform float uGalaxyScale;
uniform int uQualityTier;   // 0=Low,1=Medium,2=High,3=Ultra

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
    for (int i = 0; i < 4; i++) { // Optimized: 5 -> 4 octaves
        f += amp * noise(p);
        p = m2 * p * 2.02;
        amp *= 0.5;
    }
    return f;
}

// Cheaper FBM for high frequency details
float fbmLow(vec2 p) {
    float f = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 2; i++) { 
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
        f += pow(1.0 - abs(noise(p) * 2.0 - 1.0), 2.0) * amp;
        p = m2 * p * 2.02;
        amp *= 0.5;
    }
    return f;
}

// === GALAXY STRUCTURE ===
vec3 GetGalaxyStructure(vec2 uv) {
    float r = length(uv);
    float warpAmount = 0.2 * smoothstep(0.05, 0.4, r);
    float warp = fbm(uv * 1.5); // Remove uTime dependence for stability
    vec2 warpedUV = uv + vec2(warp - 0.5) * warpAmount; 
    
    float angle = atan(warpedUV.y, warpedUV.x);
    float dist = length(warpedUV);
    
    float numArms = 2.0;
    float twist = 12.0; 
    float spiralPhase = angle * numArms + twist * log(dist + 0.001); 
    
    // Fixed: Removed -uTime * 0.15 to stop rigid rotation. 
    // Motion is now driven by the differential rotation in main().
    float armSignal = cos(spiralPhase); 
    
    // WIDENED ARMS: broader smoothstep range to fill gaps
    float armDensity = smoothstep(-1.0, 0.9, armSignal); 
    
    // Dust also static relative to arms
    float dustSignal = cos(spiralPhase - 0.5); 
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
    
    // Reverted to standard fbm for quality stability
    float microDetail = fbm(uv * 40.0 - uTime * 0.05); 
    
    // FILLER GAS: Increased intensity and reduced falloff to fill voids
    float ambientGas = 0.6 * fbm(uv * 2.0) * exp(-r * 1.5); 
    
    // ARC BRIDGES: Noise that connects arms
    float bridgeNoise = fbm(uv * 6.0 - vec2(uTime * 0.02));
    float bridge = smoothstep(0.4, 0.8, bridgeNoise) * 0.3 * exp(-r);
    
    // Main Gas Density
    float gasDensity = arm * mix(0.6, 1.4, fbm(uv * 3.0 - uTime * 0.02));
    gasDensity += ambientGas + bridge; 
    
    // Color Mixing
    vec3 gasColor = mix(C_ARM_SEC, C_ARM_PRI, structure.x * (0.6 + 0.4 * microDetail));
    gasColor = mix(C_ARM_SEC * 0.5, gasColor, smoothstep(0.1, 0.5, gasDensity)); 
    
    float dustNoise = ridged_fbm(uv * 4.0 + uTime * 0.02);
    float strongDust = dust * smoothstep(0.2, 0.8, dustNoise);
    
    vec3 finalGas = gasColor * gasDensity * 0.6; 
    finalGas += C_CORE_HOT * core * 0.3; 
    finalGas *= (1.0 - strongDust * 0.8); 
    
    float fade = exp(-r * 0.8);
    return finalGas * fade;
}

// === STAR SPECTRAL ANALYSIS ===
vec3 GetStarColor(float seed) {
    float r = fract(sin(seed * 123.45) * 456.78);
    if (r > 0.98) return vec3(0.6, 0.7, 1.0);      
    if (r > 0.90) return vec3(0.7, 0.85, 1.0);     
    if (r > 0.75) return vec3(0.95, 0.98, 1.0);    
    if (r > 0.60) return vec3(1.0, 1.0, 0.9);      
    if (r > 0.40) return vec3(1.0, 0.9, 0.6);      
    if (r > 0.15) return vec3(1.0, 0.7, 0.4);      
    return vec3(1.0, 0.5, 0.4);                    
}

// === STAR RENDERING ===
vec3 RenderStarLayer(vec2 uv, float scale, vec3 structure, float seedOffset, float densityInfluence) {
    vec2 gridUV = uv * scale;
    vec2 gridID = floor(gridUV);
    vec2 gridFract = fract(gridUV);
    
    vec3 col = vec3(0.0);
    
    for(int y = -1; y <= 1; y++) {
        for(int x = -1; x <= 1; x++) {
            vec2 neighbour = vec2(float(x), float(y));
            vec2 id = gridID + neighbour;
            
            float h = hash12(id + vec2(seedOffset * 15.3)); 

            // OPTIMIZATION: Early exit
            if (h > 0.45) continue;
            
            vec2 pos = neighbour + vec2(hash12(id * 1.54), hash12(id * 2.56));
            vec2 diff = pos - gridFract;
            float dist = length(diff);
            
            float brightness = 0.0;
            
            float spectralSeed = h * 10.0;
            vec3 starColor = GetStarColor(spectralSeed);
            float twinkleSpeed = 2.0 + h * 3.0; 
            float twinkleBase = 0.7 + 0.3 * sin(uTime * twinkleSpeed + h * 6.28);
            
            float prob = 0.1;
            float bulgeBias = 0.0;

            if (densityInfluence > 0.01) {
                vec2 starWorldUV = (id + vec2(0.5)) / scale;
                float distFromGalCenter = length(starWorldUV);
                vec3 localStruct = GetGalaxyStructure(starWorldUV);
                
                bulgeBias = exp(-distFromGalCenter * 5.0) * 1.2;
                float structureDensity = max(localStruct.x, localStruct.z * 2.0 + bulgeBias); 
                prob = mix(0.1, structureDensity, densityInfluence); 
                prob = max(prob, bulgeBias * 0.5); 
            }
            
            // FIX: Defined size correctly
            float size = 0.002 + h * 0.003; 
            
            if (h < (prob * 0.8 + 0.05) * 0.5) { 
                brightness = size / (dist * dist + 0.00001);
                brightness *= smoothstep(1.0, 0.1, dist); 
                
                if (h > 0.99) { brightness *= 3.0; starColor += 0.2; }
                
                col += starColor * brightness * twinkleBase * 1.2;
            }
        }
    }
    return col;
}

void main() {
    // 1. Transform Setup
    vec2 screenPos = vec2(gl_FragCoord.x, uResolution.y - gl_FragCoord.y);
    vec2 screenCenter = uCameraOffset; // Correctly use the camera's screen-space offset
    vec2 worldPos = (screenPos - screenCenter) / uZoom + uOffset;
    vec2 p = (worldPos - uGalaxyCenter) * uGalaxyScale;
    // 2. Local UV and Radial calc
    vec2 uv = p * 0.3333; 
    float r = length(uv);
    
    // --- DIFFERENTIAL ROTATION (Keplerian-ish) ---
    // Inner parts rotate (orbit) faster than outer parts.
    // This creates natural spiraling and avoids the "Rigid Body" look.
    float distSq = r * r + 0.01;
    // Steeper curve for more contrast between inner and outer
    float keplerSpeed = 0.5 * pow(distSq, -0.4); 
    
    // Slowed down significantly for majestic scale
    float baseRot = uTime * -0.005; // Reversed background drift
    float diffRot = uTime * keplerSpeed * -0.08; // Reversed orbit speed
    
    float totalRotAngle = baseRot + diffRot;
    float cr = cos(totalRotAngle); 
    float sr = sin(totalRotAngle);
    
    // Apply rotation to the domain
    uv = mat2(cr, sr, -sr, cr) * uv;
    // Recalculate angle after rotation for disk alignment
    float angle = atan(uv.y, uv.x); 
    
    // --- BLACK HOLE & LENSING ---
    float bhRadius = 0.026; 
    
    // Gravitational Lensing (Pinched UVs)
    float distToCenter = r;
    float lensStrength = 0.06;
    vec2 lensedUV = uv * (1.0 - 0.5 * exp(-r * 10.0)); 

    // 3. Render Background Galaxy (Lensed)
    vec3 structData = GetGalaxyStructure(lensedUV);
    vec3 accColor = vec3(0.0);
    
    // Nebula & Stars (quality-tier gated to reduce cost on low/mid)
    if (uQualityTier <= 0) {
        accColor += RenderNebula(lensedUV, structData) * 0.65;
        accColor += RenderStarLayer(lensedUV, 50.0, structData, 2.0, 0.45) * 0.35;
    } else if (uQualityTier == 1) {
        accColor += RenderNebula(lensedUV, structData) * 0.72;
        accColor += RenderStarLayer(lensedUV, 60.0, structData, 2.0, 0.7) * 0.5;
        accColor += RenderStarLayer(lensedUV, 180.0, structData, 3.0, 0.35) * 0.2;
    } else {
        accColor += RenderNebula(lensedUV, structData) * 0.8;
        accColor += RenderStarLayer(lensedUV, 15.0, structData, 1.0, 1.0) * 0.1;
        accColor += RenderStarLayer(lensedUV, 60.0, structData, 2.0, 0.8) * 0.6;
        accColor += RenderStarLayer(lensedUV, 150.0, structData, 3.0, 0.5) * 0.4;
        accColor += RenderStarLayer(lensedUV, 350.0, structData, 10.0, 0.0) * 0.3;
    }
    
    // --- ACCRETION DISK (Volumetric + Chromatic + Bloom) ---
    if (uQualityTier >= 2 && r > bhRadius) {
        // Scaled to match the 25% radius reduction
        float diskFade = exp(-(r - bhRadius) * 65.0); 
        
        // Fixed: Lowered threshold and added smooth fade-out to prevent the "Circle Line" artifact
        if (diskFade > 0.0001) {
             // Coordinate transformation for the disk
            float diffRotDisk = (2.0 / (r * 15.0)); 
            float currentAngle = angle + uTime * 0.8 + diffRotDisk;
            vec2 diskFlowUV = vec2(cos(currentAngle), sin(currentAngle)) * (r * 10.0);
            
            // Texture Sampling with "Heat" distortion
            float flowNoise = fbm(diskFlowUV - vec2(uTime * 0.5));
            float detailNoise = noise(diskFlowUV * 3.0 + uTime);
            float matterDensity = flowNoise * 0.6 + detailNoise * 0.4;
            
            // Edges
            float innerEdge = smoothstep(bhRadius, bhRadius + 0.002, r);
            float doppler = 1.0 + 0.4 * sin(angle + 0.5); 
            
            // Color Mapping (Cyan/White Energy)
            vec3 cHot = vec3(0.8, 0.95, 1.0);   // Cyan-White Core
            vec3 cMid = vec3(0.2, 0.6, 0.9);   // Electric Blue
            vec3 cOut = vec3(0.1, 0.0, 0.1);   // Dark Violet
            
            vec3 matterColor = mix(cOut, cMid, smoothstep(0.0, 0.4, diskFade));
            matterColor = mix(matterColor, cHot, smoothstep(0.4, 0.9, diskFade * matterDensity));
            
            float intensity = matterDensity * diskFade * innerEdge * doppler * 6.0;
            
            // CRITICAL FIX: Force fade to 0 before the hard cut-off
            float hardCutoffFade = smoothstep(0.0001, 0.005, diskFade);
            intensity *= hardCutoffFade;

            // CHROMATIC ABERRATION (Simulated)
            // Shift red channel slightly outwards, blue slightly inwards
            float abrIntensity = intensity; 
            // Simple channel separation for high energy look
            vec3 diskFinal = matterColor * intensity;
            diskFinal.r *= smoothstep(0.0, 1.0, 1.0 + 0.1 * sin(uTime * 10.0 + r * 100.0)); // Sparkle
            
            accColor += diskFinal;

            // FAKE BLOOM / GLOW
            // Add a wide, soft glow around the disk
            float bloomMask = exp(-(r - bhRadius) * 20.0) * innerEdge;
            accColor += cMid * bloomMask * 0.5 * hardCutoffFade; // Apply fade here too
        }
    }

    // --- PHOTON RING ---
    float pRingGlow = 0.005 / (abs(r - bhRadius - 0.005) + 0.0001);
    pRingGlow *= smoothstep(0.05, 0.0, abs(r - bhRadius));
    if (uQualityTier <= 1) {
        vec3 simpleRing = vec3(0.75, 0.85, 0.95) * pRingGlow;
        simpleRing += C_CORE_HOT * smoothstep(0.02, 0.0, abs(r - bhRadius)) * 0.45;
        accColor += simpleRing * 0.9;
    } else {
        vec3 wRingColor = vec3(1.0, 0.9, 0.8);
        vec3 rRingColor = vec3(1.0, 0.2, 0.1);
        vec3 bRingColor = vec3(0.1, 0.5, 1.0);
        float rOffset = 0.001;
        float gR = 0.005 / (abs(r - bhRadius - 0.005 + rOffset) + 0.0001);
        float gB = 0.005 / (abs(r - bhRadius - 0.005 - rOffset) + 0.0001);
        vec3 finalRing = wRingColor * pRingGlow;
        finalRing += rRingColor * gR * 0.5;
        finalRing += bRingColor * gB * 0.5;
        finalRing += C_CORE_HOT * smoothstep(0.02, 0.0, abs(r - bhRadius)) * 0.8;
        accColor += finalRing * 1.5;
    }

    // 4. Final Final Passage
    vec3 outColor = C_INK_BG + accColor;
    
    // Perfect Horizon
    float horizonMask = smoothstep(bhRadius - 0.002, bhRadius, r); 
    outColor *= horizonMask;
    
    // Void Depth
    float voidDepth = smoothstep(bhRadius + 0.08, bhRadius, r);
    outColor = mix(outColor, vec3(0.0), voidDepth * 0.5);

    // --- POST PROCESSING (Vignette & Grain) ---
    // Vignette
    float vig = 1.0 - smoothstep(1.0, 2.5, r * 1.5);
    outColor *= vig;
    
    // Boundary fade (Original)
    outColor *= smoothstep(2.5, 1.2, r); 
    
    finalColor = vec4(outColor, 1.0);
}
