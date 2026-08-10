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
uniform vec2 uRenderScale;  // Render-to-screen scale (1.0 = full resolution)

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
vec3 GetStarColor(float colorHash) {
    if (colorHash > 0.90) return vec3(0.30, 0.60, 1.00); // Blue (10%)
    if (colorHash > 0.75) return vec3(0.80, 0.90, 1.00); // Blue-White (15%)
    if (colorHash > 0.55) return vec3(1.00, 1.00, 1.00); // White (20%)
    if (colorHash > 0.30) return vec3(1.00, 0.95, 0.70); // Yellow (25%)
    return vec3(1.00, 0.45, 0.45);                       // Red/Orange (30%)
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
            
            float colorHash = fract(h * 31.415);
            vec3 starColor = GetStarColor(colorHash);
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
            
            float sizeHash = fract(h * 13.987);
            float sizeMult = mix(0.5, 3.5, pow(sizeHash, 4.0)); 
            float size = (0.002 + h * 0.003) * sizeMult; 
            
            if (h < (prob * 0.8 + 0.05) * 0.5) { 
                brightness = size / (dist * dist + 0.00001);
                brightness *= exp(-dist * dist * 30.0); // Soft falloff
                
                if (h > 0.99) { brightness *= 3.0; starColor += 0.2; }
                
                col += starColor * brightness * twinkleBase * 1.2;
            }
        }
    }
    return col;
}

// === MODERN TOP-DOWN GALAXY (PHASE 2) ===
mat2 rot2(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat2(c, -s, s, c);
}

float fbm3(vec2 p) {
    float f = 0.0;
    f += 0.5 * noise(p); p = m2 * p * 2.02;
    f += 0.25 * noise(p); p = m2 * p * 2.01;
    f += 0.125 * noise(p);
    return f;
}

vec3 RenderModernStarLayer(vec2 uv, float scale, float threshold, float sizeBase, float twinkleSpeed, float radialBoost) {
    vec2 g = uv * scale;
    vec2 gid = floor(g);
    vec2 gf = fract(g);
    vec3 col = vec3(0.0);

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 cell = gid + vec2(float(x), float(y));
            float h = hash12(cell * 0.713 + vec2(scale * 0.01, scale * 0.07));
            if (h > threshold) continue;

            vec2 jitter = vec2(hash12(cell + 17.3), hash12(cell + 43.9));
            vec2 p = vec2(float(x), float(y)) + jitter - gf;
            float d2 = dot(p, p);

            float colorHash = fract(h * 31.415);
            float sizeHash = fract(h * 13.987);
            float sizeMult = mix(0.9, 3.8, pow(sizeHash, 3.0));
            
            float inv = (sizeBase * sizeMult) / (d2 + 0.0001);
            float shape = exp(-d2 * 20.0); 
            
            float tw = 0.85 + 0.15 * sin(uTime * (1.2 + h * 5.0) * twinkleSpeed + h * 31.4);
            vec3 c = GetStarColor(colorHash);
            col += c * inv * shape * tw * 0.5;
        }
    }
    return col * radialBoost;
}

vec3 RenderModernGalaxy(vec2 uv, float r) {
    vec3 C_BG        = vec3(0.002, 0.004, 0.008); 
    vec3 C_CORE_IN   = vec3(1.00,  0.95,  0.80);  // Warm white/yellow core
    vec3 C_CORE_OUT  = vec3(0.65,  0.85,  1.00);  // Bright celestial blue
    vec3 C_ARM_BASE  = vec3(0.05,  0.15,  0.45);  // Deep space blue
    vec3 C_ARM_BRIGHT= vec3(0.15,  0.65,  1.00);  // Bright cyan/blue
    vec3 C_H2_REGION = vec3(1.00,  0.15,  0.45);  // Pink star-forming regions
    vec3 C_DUST      = vec3(0.02,  0.03,  0.04);  // Subtle dust
    float bhRadius = 0.015;

    // Gravitational Lensing effect for background structures
    vec2 lensedUv = uv * (1.0 + (bhRadius * bhRadius) / (r * r + 0.0001) * 0.6);

    // Rigid pattern rotation
    float baseRotation = uTime * 0.05; 
    vec2 l = rot2(baseRotation) * lensedUv;
    l += vec2(fbm(l * 1.5) - 0.5, fbm(l * 1.5 + vec2(4.1, -2.3)) - 0.5) * 0.04 * r;
    
    // Match stars to the density wave arm rotation speed (~ 0.02), with slight drift
    float driftSpeed = 0.018 + 0.008 * exp(-r * 2.0);
    vec2 lStars = rot2(uTime * driftSpeed) * l + vec2(uTime * 0.002, -uTime * 0.001);

    float rad = max(length(l), 0.0001);
    float ang = atan(l.y, l.x);

    // Core
    float bulgeFactor = exp(-rad * rad * 45.0) * 1.2 + exp(-rad * 5.0) * 0.35;
    vec3 bulgeColor = mix(C_CORE_OUT, C_CORE_IN, smoothstep(0.0, 0.6, bulgeFactor));
    
    // Density Wave Spiral Arms
    float arms = 2.0; 
    float winding = 11.0; 
    float phaseNoise = (fbm(l * 4.0) - 0.5) * 1.5 * smoothstep(0.05, 1.2, rad);
    float spiralPhase = ang * arms + log(rad + 0.02) * winding + phaseNoise - uTime * 0.04;
    
    float armMask = smoothstep(-0.2, 0.8, cos(spiralPhase));
    float spurPhase = ang * 4.0 + log(rad + 0.01) * (winding * 1.5) + fbm(l * 6.0) * 1.5;
    float spurMask = smoothstep(0.5, 1.0, cos(spurPhase)) * 0.5;
    float totalArmMask = clamp(armMask + spurMask, 0.0, 1.0);
    
    float diskMask = exp(-rad * 2.5) * 0.3;

    // Nebulae
    float cloudNoise = fbm(l * 5.0 - vec2(uTime * 0.01));
    float gasDensity = totalArmMask * cloudNoise * exp(-rad * 1.2) * 2.0;
    gasDensity += 0.30 * fbm(l * 3.0) * exp(-rad * 1.5); 
    gasDensity += diskMask;
    vec3 gasColor = mix(C_ARM_BASE, C_ARM_BRIGHT, smoothstep(0.1, 0.9, cloudNoise * totalArmMask + diskMask));
    
    // Pink H2
    float h2Noise = fbm(l * 15.0 + vec2(1.0, -1.0));
    float h2Mask = smoothstep(0.7, 0.95, h2Noise) * totalArmMask * exp(-rad * 1.0);
    gasColor = mix(gasColor, C_H2_REGION, h2Mask);

    // Dust Extinction
    float dustPhase = spiralPhase - 0.5; 
    float dustLane = smoothstep(0.6, 1.0, cos(dustPhase));
    float clumpNoise = ridged_fbm(l * 10.0 + vec2(uTime * 0.01));
    float dustDensity = dustLane * clumpNoise * exp(-rad * 1.2) * totalArmMask * 1.2;
    dustDensity += ridged_fbm(l * 15.0) * exp(-rad * 3.0) * 0.5;

    // Deep Space Background (Distant Galaxies & Clusters)
    vec3 deepSpace = vec3(0.0);
    vec2 dG1 = lStars * 40.0;
    vec2 dCell1 = floor(dG1);
    vec2 dFract1 = fract(dG1);
    float dH1 = hash12(dCell1 * 0.91 + vec2(17.3, -9.1));
    if (dH1 > 0.70) {
        vec2 dPos = dFract1 - vec2(0.5);
        dPos.x *= mix(1.0, 3.0, fract(dH1 * 10.0));
        dPos = rot2(dH1 * 6.28) * dPos;
        float d2 = dot(dPos, dPos);
        float softDot = exp(-d2 * mix(10.0, 25.0, fract(dH1 * 23.0))); 
        vec3 c = GetStarColor(fract(dH1 * 31.0));
        deepSpace += c * softDot * 0.18;
    }
    vec2 dG2 = lStars * 120.0;
    vec2 dCell2 = floor(dG2);
    vec2 dFract2 = fract(dG2);
    float dH2 = hash12(dCell2 * 0.53 + vec2(-4.3, 19.1));
    if (dH2 > 0.60) {
        vec2 dPos = dFract2 - vec2(0.5);
        float d2 = dot(dPos, dPos);
        float softDot = exp(-d2 * 12.0); 
        vec3 c = GetStarColor(fract(dH2 * 41.0));
        deepSpace += c * softDot * 0.08;
    }
    float dsNoise = fbm(lStars * 5.0 + vec2(uTime * 0.005, uTime * -0.005));
    deepSpace += vec3(0.05, 0.12, 0.20) * smoothstep(0.4, 0.9, dsNoise) * 0.06;
    deepSpace *= mix(0.3, 1.0, exp(-rad * 1.5));

    // Stars
    float coreStarBoost = exp(-rad * 6.0) * 0.4;
    float totalStarDensity = clamp(totalArmMask + diskMask + coreStarBoost, 0.0, 1.0);
    float starBoost = 0.50 + 0.4 * exp(-rad * 3.0) + totalStarDensity * 0.80;
    
    float clusterNoise = fbm(l * 14.0 + vec2(-1.6, 3.9));
    float clusterMask = smoothstep(0.56, 0.90, clusterNoise) * totalStarDensity;
    vec3 stars = vec3(0.0);

    // thresholds increased to give less emptiness
    if (uQualityTier == 1) {
        stars += RenderModernStarLayer(lStars, 78.0, 0.25, 0.0016, 0.85, starBoost) * 0.42;
        stars += RenderModernStarLayer(lStars, 195.0, 0.18, 0.0013, 1.0, starBoost * 0.7) * 0.22;
    } else {
        stars += RenderModernStarLayer(lStars, 40.0, 0.25, 0.0022, 0.80, starBoost) * 0.28;
        stars += RenderModernStarLayer(lStars, 96.0, 0.22, 0.0018, 0.95, starBoost) * 0.45;
        stars += RenderModernStarLayer(lStars, 236.0, 0.18, 0.0013, 1.10, starBoost * 0.8) * 0.32;
    }
    
    float armGain = 0.64 + 1.10 * totalStarDensity + 0.92 * clusterMask;
    stars *= mix(0.40, 1.0, totalStarDensity);
    stars *= armGain;
    stars += deepSpace;
    float coreVoid = smoothstep(bhRadius + 0.008, bhRadius + 0.05, r);
    stars *= coreVoid;
    
    float knotField = smoothstep(0.76, 0.95, clusterNoise) * totalStarDensity * exp(-rad * 0.95);
    stars += vec3(0.85, 0.95, 1.0) * knotField * 0.35 * coreVoid;
    stars += vec3(1.0, 0.15, 0.45) * h2Mask * 0.6 * coreVoid; // Pink stars

    // Gargantua-style Lensed Accretion Disk
    vec3 accretion = vec3(0.0);
    if (uQualityTier >= 2 && r < bhRadius + 0.08) {
        float aRaw = atan(uv.y, uv.x);
        vec2 uvTilted = vec2(uv.x, uv.y * 5.0); // Tilt to edge-on
        float rTilted = length(uvTilted);
        
        // Doppler Beaming (approaching side blue, receding red)
        float dopplerShift = uv.x / (r + 0.0001); // -1.0 to 1.0
        float dopplerMultiplier = 1.0 + 0.95 * dopplerShift; 
        
        // Spectrum (closer to Interstellar)
        vec3 cRed = vec3(0.6, 0.05, 0.0);    
        vec3 cOrange = vec3(1.0, 0.4, 0.05); 
        vec3 cYellow = vec3(1.0, 0.8, 0.2);  
        vec3 cWhite = vec3(1.0, 0.95, 1.0);  
        
        float colorPos = dopplerShift * 0.5 + 0.5;
        vec3 baseDiskColor = mix(cRed, cOrange, smoothstep(0.0, 0.4, colorPos));
        baseDiskColor = mix(baseDiskColor, cYellow, smoothstep(0.4, 0.7, colorPos));
        baseDiskColor = mix(baseDiskColor, cWhite, smoothstep(0.7, 1.0, colorPos));
        
        // Texture
        float diskPhase = atan(uvTilted.y, uvTilted.x) + uTime * 2.5;
        float flowDist = log(rTilted + 0.001) * 25.0;
        float diskNoise = fbm(vec2(flowDist, diskPhase * 2.0));
        float matter = smoothstep(0.1, 0.9, diskNoise);
        
        // Front Disk Profile (Extremely sharp inner edge)
        float innerCut = smoothstep(bhRadius - 0.001, bhRadius + 0.0005, rTilted);
        float diskProfile = exp(-(rTilted - bhRadius) * 200.0); // Very narrow bright ring
        diskProfile += exp(-(rTilted - bhRadius) * 50.0) * 0.4; // Fainter outer spread
        float behindMask = 1.0 - (smoothstep(0.0, 0.001, uv.y) * (1.0 - smoothstep(bhRadius-0.0005, bhRadius, r)));
        float frontDisk = innerCut * diskProfile * mix(0.4, 1.0, matter) * behindMask;
        
        // Lensed Back Disk
        float lensRingRadius = bhRadius + 0.0015;
        float lensDist = abs(r - lensRingRadius);
        float verticality = smoothstep(0.1, 0.8, abs(uv.y) / (r + 0.0001));
        float lensedNoise = fbm(vec2(log(r) * 40.0, (aRaw - uTime * 3.0) * 4.0));
        float lensedMatter = smoothstep(0.2, 0.8, lensedNoise);
        float backDisk = smoothstep(0.003 * verticality, 0.000, lensDist) * verticality * mix(0.4, 1.0, lensedMatter);
        
        // Combine with self-shadowing 
        float ehMask = smoothstep(bhRadius - 0.0015, bhRadius, r);
        float finalIntensity = (frontDisk + backDisk * 0.9 * ehMask) * dopplerMultiplier * 1.8;
        
        accretion += mix(baseDiskColor, cWhite, smoothstep(0.6, 1.5, finalIntensity)) * finalIntensity;
        // Photon ring
        accretion += cWhite * smoothstep(0.001, 0.0, abs(r - bhRadius)) * 0.8 * ehMask * dopplerMultiplier;
    }

    // Compositing
    vec3 finalRGB = C_BG;
    finalRGB += gasColor * gasDensity;
    finalRGB += bulgeColor * bulgeFactor;
    
    float extinction = exp(-dustDensity * 2.0); // Softer extinction
    finalRGB *= extinction;
    finalRGB += C_DUST * dustDensity * 0.5 * bulgeFactor; 
    
    finalRGB += stars + accretion;
    float eventHorizonMask = smoothstep(bhRadius - 0.0015, bhRadius + 0.0015, r);
    finalRGB *= eventHorizonMask; 

    float vignette = smoothstep(2.6, 1.15, r) * (1.0 - smoothstep(1.15, 2.45, r * 1.35));
    finalRGB *= vignette;

    return finalRGB;
}

void main() {
    // 1. Transform Setup
    vec2 fc = gl_FragCoord.xy * uRenderScale;
    vec2 screenPos = vec2(fc.x, uResolution.y - fc.y);
    vec2 screenCenter = uCameraOffset; // Correctly use the camera's screen-space offset
    vec2 worldPos = (screenPos - screenCenter) / uZoom + uOffset;
    vec2 p = (worldPos - uGalaxyCenter) * uGalaxyScale;
    // 2. Local UV and Radial calc
    vec2 uv = p * 0.3333; 
    float r = length(uv);

    // Phase 1 keeps the previous implementation intact.
    // Phase 2 enables modern top-down galaxy rendering for medium/high tiers.
    if (uQualityTier >= 1) {
        finalColor = vec4(RenderModernGalaxy(uv, r), 1.0);
        return;
    }
    
    // --- DIFFERENTIAL ROTATION (Keplerian-ish) ---
    // Inner parts rotate (orbit) faster than outer parts.
    // This creates natural spiraling and avoids the "Rigid Body" look.
    float distSq = r * r + 0.01;
    // Steeper curve for more contrast between inner and outer
    float keplerSpeed = 0.5 * pow(distSq, -0.4); 
    
    // Slowed down significantly for majestic scale
    float baseRot = uTime * 0.005; // Reversed background drift
    float diffRot = uTime * keplerSpeed * 0.08; // Reversed orbit speed
    
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
            float currentAngle = angle - uTime * 0.8 + diffRotDisk;
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
