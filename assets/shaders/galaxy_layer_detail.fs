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

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

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
    for (int i = 0; i < 3; ++i) {
        f += a * noise(p);
        p = m2 * p * 2.02;
        a *= 0.5;
    }
    return f;
}

mat2 rot2(float a) {
    float c = cos(a);
    float s = sin(a);
    return mat2(c, -s, s, c);
}

vec3 starColor(float colorHash) {
    // Proportional spectral distribution
    if (colorHash > 0.90) return vec3(0.30, 0.60, 1.00); // Blue (10%)
    if (colorHash > 0.75) return vec3(0.80, 0.90, 1.00); // Blue-White (15%)
    if (colorHash > 0.55) return vec3(1.00, 1.00, 1.00); // White (20%)
    if (colorHash > 0.30) return vec3(1.00, 0.95, 0.70); // Yellow (25%)
    return vec3(1.00, 0.45, 0.45);                       // Red/Orange (30%)
}

vec3 starLayer(vec2 uv, float scale, float threshold, float sizeBase, float twinkleSpeed, float radialBoost) {
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
            float sizeMult = mix(0.5, 3.5, pow(sizeHash, 4.0)); // Most are dwarfs, rare are bright giants

            // Slightly softer star draw
            float inv = (sizeBase * sizeMult) / (d2 + 0.0001);
            float shape = exp(-d2 * 30.0); // Smooth falloff avoid boxy pixel look
            float tw = 0.72 + 0.28 * sin(uTime * (1.2 + h * 5.0) * twinkleSpeed + h * 31.4);
            col += starColor(colorHash) * inv * shape * tw * 0.5;
        }
    }
    return col * radialBoost;
}

vec3 starLayerWeighted(vec2 uv, float scale, float threshold, float sizeBase,
                       float twinkleSpeed, float radialBoost, float armDensity,
                       float clusterMask, float bgFloor) {
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
            // Increased the minimum star size so they don't vanish into sub-pixels
            float sizeMult = mix(0.9, 3.8, pow(sizeHash, 3.0)); 

            float inv = (sizeBase * sizeMult) / (d2 + 0.0001);
            // Softer shape to avoid pointy pixel noise
            float shape = exp(-d2 * 20.0); 

            // Gently twinkle instead of hard flashing (reduced amplitude)
            float tw = 0.85 + 0.15 * sin(uTime * (1.0 + h * 5.0) * twinkleSpeed + h * 31.4);
            // Glints are mostly for the giant stars, and reduced in intensity
            float glint = smoothstep(0.95, 1.0, sin(uTime * (1.8 + h * 7.0) + h * 51.0)) * sizeMult * 0.3;

            // Arm regions are denser; inter-arm keeps a non-zero floor to avoid emptiness.
            float densityW = mix(bgFloor, 1.0, armDensity);
            densityW += clusterMask * 0.42;

            vec3 c = starColor(colorHash);
            col += c * inv * shape * tw * (densityW + glint) * 0.5;
        }
    }
    return col * radialBoost;
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

    // Match base layer rigid rotation
    float baseRotation = uTime * 0.05; 
    vec2 l = rot2(baseRotation) * lensedUv;
    l += vec2(fbm(l * 1.5) - 0.5, fbm(l * 1.5 + vec2(4.1, -2.3)) - 0.5) * 0.04 * r;

    // Match stars to the density wave arm rotation speed (~ 0.02), with slight drift
    float driftSpeed = 0.018 + 0.008 * exp(-r * 2.0);
    vec2 lStars = rot2(uTime * driftSpeed) * l + vec2(uTime * 0.002, -uTime * 0.001);

    float rad = max(length(l), 0.0001);
    float ang = atan(l.y, l.x);
    
    // Structure identical to base layer
    float arms = 2.0; 
    float winding = 11.0; 
    float phaseNoise = (fbm(l * 4.0) - 0.5) * 1.5 * smoothstep(0.05, 1.2, rad);
    float spiralPhase = ang * arms + log(rad + 0.02) * winding + phaseNoise - uTime * 0.04;
    
    float armMask = smoothstep(-0.2, 0.8, cos(spiralPhase));
    float spurPhase = ang * 4.0 + log(rad + 0.01) * (winding * 1.5) + fbm(l * 6.0) * 1.5;
    float spurMask = smoothstep(0.5, 1.0, cos(spurPhase)) * 0.5;
    
    float armDensity = clamp(armMask + spurMask, 0.0, 1.0);
    float diskDensity = exp(-rad * 2.5) * 0.3; // matches the background fix
    float coreStarBoost = exp(-rad * 6.0) * 0.4; // reduced from 0.8
    float totalStarDensity = clamp(armDensity + diskDensity + coreStarBoost, 0.0, 1.0);
    
    float interArm = 1.0 - totalStarDensity;
    // reduced the core boost from 1.2 to 0.4 and overall density boost to reduce glare
    float starBoost = 0.50 + 0.4 * exp(-rad * 3.0) + totalStarDensity * 0.80;

    // Deep Space Background (Distant Galaxies & Clusters)
    vec3 deepSpace = vec3(0.0);
    
    // Large, highly blurry distant galaxies
    vec2 dG1 = lStars * 40.0;
    vec2 dCell1 = floor(dG1);
    vec2 dFract1 = fract(dG1);
    float dH1 = hash12(dCell1 * 0.91 + vec2(17.3, -9.1));
    if (dH1 > 0.70) {
        vec2 dPos = dFract1 - vec2(0.5);
        dPos.x *= mix(1.0, 3.0, fract(dH1 * 10.0)); // Make them elliptical
        dPos = rot2(dH1 * 6.28) * dPos;
        float d2 = dot(dPos, dPos);
        float softDot = exp(-d2 * mix(10.0, 25.0, fract(dH1 * 23.0))); 
        vec3 c = starColor(fract(dH1 * 31.0));
        deepSpace += c * softDot * 0.18;
    }
    
    // Medium distance star clusters
    vec2 dG2 = lStars * 120.0;
    vec2 dCell2 = floor(dG2);
    vec2 dFract2 = fract(dG2);
    float dH2 = hash12(dCell2 * 0.53 + vec2(-4.3, 19.1));
    if (dH2 > 0.60) {
        vec2 dPos = dFract2 - vec2(0.5);
        float d2 = dot(dPos, dPos);
        float softDot = exp(-d2 * 12.0); 
        vec3 c = starColor(fract(dH2 * 41.0));
        deepSpace += c * softDot * 0.08;
    }

    // Faint cosmic dust/background nebulosity from other systems
    float dsNoise = fbm(lStars * 5.0 + vec2(uTime * 0.005, uTime * -0.005));
    deepSpace += vec3(0.05, 0.12, 0.20) * smoothstep(0.4, 0.9, dsNoise) * 0.06;

    // Apply dust extinction from our own galaxy over the deep space background
    deepSpace *= mix(0.3, 1.0, exp(-rad * 1.5)) * mix(0.5, 1.0, totalStarDensity);

    // Arm-local star-forming cluster mask.
    float clusterNoise = fbm(l * 14.0 + vec2(-1.6, 3.9));
    float clusterMask = smoothstep(0.56, 0.90, clusterNoise) * totalStarDensity;

    vec3 stars = vec3(0.0);
    if (uQualityTier == 0) {
        stars += starLayerWeighted(lStars, 92.0, 0.35, 0.0016, 0.85, starBoost, totalStarDensity, clusterMask, 0.40) * 0.50;
        stars += deepSpace;
    } else if (uQualityTier == 1) {
        stars += starLayerWeighted(lStars, 78.0, 0.40, 0.0019, 0.90, starBoost, totalStarDensity, clusterMask, 0.40) * 0.66;
        stars += starLayerWeighted(lStars, 195.0, 0.30, 0.0015, 1.05, starBoost * 0.82, totalStarDensity, clusterMask, 0.30) * 0.44;
        stars += deepSpace;
    } else {
        stars += starLayerWeighted(lStars, 40.0, 0.35, 0.0023, 0.85, starBoost, totalStarDensity, clusterMask, 0.45) * 0.42;
        stars += starLayerWeighted(lStars, 96.0, 0.30, 0.0019, 1.00, starBoost, totalStarDensity, clusterMask, 0.40) * 0.60;
        stars += starLayerWeighted(lStars, 236.0, 0.25, 0.0015, 1.16, starBoost * 0.82, totalStarDensity, clusterMask, 0.35) * 0.44;
        stars += starLayerWeighted(lStars, 520.0, 0.15, 0.0011, 1.30, starBoost * 0.68, totalStarDensity, clusterMask, 0.30) * 0.28;
        stars += deepSpace;
    }

    // Enforce arm-following distribution
    float armGain = 0.64 + 1.10 * totalStarDensity + 0.92 * clusterMask;
    stars *= mix(0.40, 1.0, totalStarDensity);
    stars *= armGain;

    // Pink HII-like knots
    float h2Noise = fbm(l * 15.0 + vec2(1.0, -1.0));
    float h2Mask = smoothstep(0.7, 0.95, h2Noise) * totalStarDensity * exp(-rad * 1.0);
    stars += vec3(1.0, 0.15, 0.45) * h2Mask * 0.6; 

    // Young blue/white stars
    float knotField = smoothstep(0.76, 0.95, clusterNoise) * totalStarDensity * exp(-rad * 0.95);
    stars += vec3(0.85, 0.95, 1.0) * knotField * 0.35;

    // Slow density breathing to avoid static appearance.
    float breath = 0.94 + 0.10 * sin(uTime * 0.65 + rad * 8.0 + ang * 2.0);
    stars *= breath;

    // Enforce dark void around BH to make accretion disk pop
    float coreVoid = smoothstep(bhRadius + 0.008, bhRadius + 0.05, r);
    stars *= coreVoid;
    // Gargantua-style Lensed Accretion Disk
    vec3 acc = vec3(0.0);
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
        
        acc += mix(baseDiskColor, cWhite, smoothstep(0.6, 1.5, finalIntensity)) * finalIntensity;
        // Photon ring
        acc += cWhite * smoothstep(0.001, 0.0, abs(r - bhRadius)) * 0.8 * ehMask * dopplerMultiplier;
    }

    // High-frequency nebulous wisps
    float wisp = fbm(l * 20.0 + vec2(uTime * 0.015, -uTime * 0.012));
    float wispMask = smoothstep(0.56, 0.90, wisp) * (0.18 + 0.82 * totalStarDensity) * exp(-rad * 0.95);
    vec3 wispColor = mix(vec3(0.15, 0.65, 1.00), vec3(1.00, 0.15, 0.45), h2Mask); 

    vec3 detail = stars + acc + wispColor * wispMask * 0.35;
    detail *= smoothstep(bhRadius - 0.0015, bhRadius + 0.0015, r);
    detail *= (1.0 - smoothstep(1.15, 2.45, r * 1.35));
    detail *= smoothstep(2.6, 1.15, r);

    float alpha = clamp(max(detail.r, max(detail.g, detail.b)), 0.0, 1.0);
    finalColor = vec4(detail, alpha);
}
