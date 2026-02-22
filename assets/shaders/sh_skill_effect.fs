#version 430

in vec2 localPos;
in vec4 passCoreColor;
in vec4 passGlowColor;
in vec2 passDirection; // Direction the projectile is facing
in float passAngle;    // Sector Half-Angle? No, assuming full angle spread for now.
in float passRadius;   // The radius boundary in local space (e.g. 0.8)
flat in uint passFlags;
in float passType;
uniform float uTime;

out vec4 finalColor;

#include "vfx/vfx_element_switch.glslinc"

// --- SDF Functions (2D) ---

// Circle
float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

// Box
float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// Pie (Sector)
// p: point
// c: sin/cos of aperture geometry (half-angle)
// r: radius
float sdPie(vec2 p, vec2 c, float r) {
    p.x = abs(p.x);
    float l = length(p) - r;
    float m = length(p - c * clamp(dot(p, c), 0.0, r));
    return max(l, m * sign(c.y * p.x - c.x * p.y));
}

// Rotation
vec2 rotate(vec2 v, float a) {
	float s = sin(a);
	float c = cos(a);
	return mat2(c, -s, s, c) * v;
}

void main() {
    // 1. Rotate uvs so that (1,0) aligns with passDirection
    // passDirection is the facing vector.
    // We want to rotate 'localPos' such that 'passDirection' becomes the X axis.
    // actually, sdPie is usually symmetric around Y axis or X axis.
    // Let's assume standard sdPie is symmetric around Y axis (vertical).
    // Or X axis. Let's assume X axis for standard "forward".
    
    // Angle of passDirection
    float angle = atan(passDirection.y, passDirection.x);
    // Rotate localPos by angle (which results in -angle rotation due to matrix implementation)
    vec2 p = rotate(localPos, angle);
    vec2 pBase = localPos;
    
    float dist = 0.0;
    
    if (passType < 0.5) { 
        // Type 0: Fan/Sector
        // passAngle is the full spread in radians (e.g. 60 deg).
        // sdPie expects sin/cos of HALF angle.
        float halfAngle = passAngle * 0.5;
        vec2 c = vec2(cos(halfAngle), sin(halfAngle)); // aperture
        dist = sdPie(p, c, passRadius);
    } else if (passType < 1.5) {
        // Type 1: Annulus / Ring / Circle
        // Just a circle for now
        dist = sdCircle(p, passRadius);
    } else if (passType < 2.5) {

        // Type 2: Sharp Thrust (Tapered Blade)
        // Aligned with X axis.
        // Center is (0,0). Length is approx 2 * passRadius.
        
        float halfLen = passRadius;
        float baseHalfWidth = passRadius * 0.25;
        
        // Normalize X to [0, 1] range (0 = Back, 1 = Front)
        float t = clamp((p.x + halfLen) / (2.0 * halfLen), 0.0, 1.0);
        
        // Taper Function: Double taper (Diamond-like profile)
        // 0.0 -> 0.2 (Back): Ramp up
        // 0.2 -> 1.0 (Blade): Taper down
        float widthScale = 1.0;
        if (t < 0.2) {
            widthScale = smoothstep(0.0, 0.2, t);
        } else {
            widthScale = mix(1.0, 0.15, (t - 0.2) / 0.8);
        }
        
        float currentHalfWidth = baseHalfWidth * widthScale;
        
        // Approximate SDF
        float dx = abs(p.x) - halfLen;
        float dy = abs(p.y) - currentHalfWidth;
        
        // Combine (Intersection of bounds)
        // Combine (Intersection of bounds)
        // Using max(dx, dy) creates a sharp intersection (Blade)
        float dBlade = max(dx, dy);
        
        // Crossguard (Guard)
        // Position: At the "base" of the blade.
        // Blade goes from -halfLen (Back) to +halfLen (Tip).
        // Taper starts at 0.0 -> 0.2 (Back ramp).
        // Let's place guard at t=0.2 approx?
        // t=0.2 => p.x = -halfLen + 0.4*halfLen = -0.6 * passRadius?
        // Let's place it at x = -halfLen * 0.6
        
        float guardPos = -halfLen * 0.7;
        vec2 guardSize = vec2(passRadius * 0.1, passRadius * 0.45); // Thickness, Width
        
        float dGuard = sdBox(p - vec2(guardPos, 0.0), guardSize);
        
        // Union: min(dBlade, dGuard)
        dist = min(dBlade, dGuard);
    } else if (passType < 3.5) {
        // Type 3: Crescent Wave (Moon / Sword Wave)
        // Convex shape facing Right (+X).
        
        float rOuter = passRadius;             // The leading edge
        float rInner = passRadius * 0.85;      // The trailing cutter
        float shift = passRadius * 0.25;       // How far back the inner circle is
        
        // Outer Circle (Centered)
        float d1 = length(p) - rOuter;
        
        // Inner Circle (Shifted backwards, i.e., Left)
        // We subtract this shape.
        float d2 = length(p - vec2(-shift, 0.0)) - rInner;
        
        // Result is Intersection of Outer AND NOT Inner
        dist = max(d1, -d2);
        
        // Hard clip the back to prevent wrapping artifacts if parameters are extreme
        // Ensure x > -rOuter
        // dist = max(dist, -p.x - rOuter);
    } else if (passType < 4.5) {
        // Type 4: Shield Ring (Annulus)
        float ringRadius = passRadius * 0.84;
        float ringHalfWidth = passRadius * 0.11;
        dist = abs(length(p) - ringRadius) - ringHalfWidth;
    } else if (passType < 5.5) {
        // Type 5: Elliptical Shield Ring
        const vec2 ellipseAxis = vec2(0.82, 1.08); // X narrower, Y taller.
        vec2 ep = vec2(pBase.x / ellipseAxis.x, pBase.y / ellipseAxis.y);
        float ringRadius = passRadius * 0.82;
        float ringHalfWidth = passRadius * 0.11;
        dist = abs(length(ep) - ringRadius) - ringHalfWidth;
    } else if (passType < 6.5) {
        // Type 6: Magic Array / Formation 
        float rOuter = passRadius * 0.98;
        float rInner = passRadius * 0.85;
        float rCore  = passRadius * 0.25;
        
        // Outer thick ring
        float ring1 = abs(length(p) - rOuter) - passRadius * 0.025;
        // Inner thin ring
        float ring2 = abs(length(p) - rInner) - passRadius * 0.015;
        // Center core ring
        float ring3 = abs(length(p) - rCore) - passRadius * 0.02;
        
        // 6-pointed star (Hexagram) via polar cosine
        float a = atan(p.y, p.x);
        float r = length(p);
        float starDist = abs(r - rInner * (0.60 + 0.40 * cos(a * 6.0))) - passRadius * 0.015;
        
        // Additional geometric lines: Triangles
        float tri1 = abs(max(abs(p.x)*0.866025 + p.y*0.5, -p.y) - rInner*0.4) - passRadius * 0.01;
        
        dist = min(min(ring1, ring2), min(ring3, min(starDist, tri1)));
    } else {
        dist = sdCircle(p, passRadius);
    }
    
    // SDF Rendering logic
    // dist < 0 : Inside
    // dist > 0 : Outside
    
    // Core: Sharp inside
    // Glow: Fade outside
    
    // --- Inverted Logic (Rim Light) ---
    // User Request: "Edges too bright, invert colors"
    // Interpretation: Darker/Colored Inside, Bright/White Edge (Rim)
    
    vec3 color;
    float alpha;
    
    // 1. Core Logic (Inside dist <= 0)
    // We want the deep center to be the Body Color (passCoreColor)
    // We want the edge (dist ~ 0) to be White (Rim)
    
    float rimFactor = smoothstep(-passRadius * 0.8, 0.0, dist); // 0.0 deep inside, 1.0 at edge
    rimFactor = pow(rimFactor, 4.0); // Make the rim sharp/thin
    
    vec3 rimColor = vec3(1.0, 1.0, 1.0); // Pure White Rim
    int elementType = int(passFlags & 0xFu);
    vec3 bodyColor = NmdSelectElementPalette(elementType, passCoreColor.rgb);
    
    // Mix Body and Rim
    vec3 finalInnerColor = mix(bodyColor, rimColor, rimFactor);
    float finalInnerAlpha = passCoreColor.a; // Keep opacity
    
    // 2. Glow Logic (Outside dist > 0)
    // Reduce brightness
    float glowFactor = 1.0 - smoothstep(0.0, passRadius * 0.5, dist); // Quicker falloff
    vec3 finalGlowColor =
        NmdSelectElementPalette(elementType, passGlowColor.rgb);
    float finalGlowAlpha = passGlowColor.a * glowFactor * 0.4; // Reduced intesity (was 0.8)
    if (passType >= 3.5 && passType < 4.5) {
        // Shield ring should have tight glow, not a large breathing haze.
        float ringGlowFactor = 1.0 - smoothstep(0.0, passRadius * 0.14, dist);
        finalGlowAlpha = passGlowColor.a * ringGlowFactor * 0.52;
    }
    if (passType >= 4.5 && passType < 5.5) {
        // Elliptical shield keeps a tighter and cleaner edge glow.
        float ringGlowFactor = 1.0 - smoothstep(0.0, passRadius * 0.12, dist);
        finalGlowAlpha = passGlowColor.a * ringGlowFactor * 0.56;
    }
    if (passType >= 5.5 && passType < 6.5) {
        // Magic Array glow is soft but contained within the outer boundary
        float arrayGlowFactor = 1.0 - smoothstep(0.0, passRadius * 0.2, dist);
        // Base ground illumination from the array's core filling the void
        float groundIllum = 1.0 - smoothstep(passRadius * 0.2, passRadius, length(pBase));
        finalGlowAlpha = max(passGlowColor.a * arrayGlowFactor * 0.6, passGlowColor.a * groundIllum * 0.2);
    }
    
    // Composite
    if (dist <= 0.0) {
        color = finalInnerColor;
        alpha = finalInnerAlpha;
    } else {
        color = finalGlowColor;
        alpha = finalGlowAlpha;
    }

    if (passType >= 2.5 && passType < 3.5) {
        // Skill 2 crescent:
        // Primary emissive focus is on the blade arc; inner disc stays nearly transparent.
        float pr = max(passRadius, 1e-4);
        float px = p.x / pr;

        float edgeMask = 1.0 - smoothstep(pr * 0.02, pr * 0.30, abs(dist));
        float bladeForwardMask = smoothstep(-0.18, 0.95, px);
        vec2 bladeSourceCenter = vec2(pr * 0.64, 0.0);
        float bladeSourceDist = length(p - bladeSourceCenter);
        float bladeSourceMask = 1.0 - smoothstep(pr * 0.16, pr * 1.02, bladeSourceDist);
        float bladeGlowMask = clamp(edgeMask * bladeForwardMask * bladeSourceMask, 0.0, 1.0);

        float outsideDist = max(dist, 0.0);
        float outerAura =
            (1.0 - smoothstep(pr * 0.02, pr * 0.42, outsideDist)) * bladeForwardMask;

        vec3 bladeGlowColor = mix(finalGlowColor, vec3(1.0), 0.48);
        color += bladeGlowColor * (bladeGlowMask * 1.02 + outerAura * 0.58);
        color = mix(color, vec3(1.0), bladeGlowMask * 0.38 + outerAura * 0.16);
        alpha = max(alpha, passGlowColor.a * (bladeGlowMask * 0.52 + outerAura * 0.24));

        // Inner-disc remnant: extremely faint and highly transparent.
        float rInner = pr * 0.85;
        float shift = pr * 0.25;
        vec2 sourceCenter = vec2(-shift, 0.0);
        float sourceDist = length(p - sourceCenter);
        float holeMask = smoothstep(0.0, pr * 0.05, dist);
        float innerMask = 1.0 - smoothstep(rInner * 0.72, rInner * 1.01, sourceDist);
        float sourceMask = holeMask * innerMask;
        float innerRimMask = 1.0 - smoothstep(pr * 0.015, pr * 0.11, abs(sourceDist - rInner));
        float cavityClipMask = smoothstep(rInner * 1.06, rInner * 1.14, sourceDist);

        // Hard suppress cavity glow and inner-rim visibility.
        color *= (1.0 - sourceMask * 0.98);
        alpha *= (1.0 - sourceMask * 0.995);
        alpha *= (1.0 - innerRimMask * 0.90);
        color *= cavityClipMask;
        alpha *= cavityClipMask;
    }

    if (passType >= 4.5 && passType < 5.5) {
        // Moving blue sheen bound to ellipse rim (no sector artifacts).
        const vec2 ellipseAxis = vec2(0.82, 1.08);
        vec2 ep = vec2(pBase.x / ellipseAxis.x, pBase.y / ellipseAxis.y);
        vec2 epDir = normalize(ep + vec2(1e-4, 1e-4));
        vec2 flowDir = normalize(passDirection + vec2(1e-4, 1e-4));
        float theta = atan(ep.y, ep.x);
        float ellipseLen = length(ep);

        float ringMask = 1.0 - smoothstep(passRadius * 0.03, passRadius * 0.16, abs(dist));
        float sweep = pow(max(dot(epDir, flowDir), 0.0), 6.0);
        float trail = pow(max(dot(epDir, -flowDir), 0.0), 9.0);
        float bandA = 0.5 + 0.5 * sin(theta * 10.0 - uTime * 4.8 + ellipseLen * 7.0);
        float bandB = 0.5 + 0.5 * sin(theta * 14.0 + uTime * 5.4 - ellipseLen * 9.0);
        float flowMask =
            ringMask * (sweep * 0.78 + trail * 0.28 + pow(bandA, 4.0) * 0.72 + pow(bandB, 5.0) * 0.58);

        vec3 sheenColor = mix(passGlowColor.rgb, vec3(0.95, 0.99, 1.00), 0.45);
        color += sheenColor * flowMask * 0.72;
        alpha = max(alpha, passGlowColor.a * flowMask * 0.62);

        // Dynamic membrane inside ellipse body.
        float fillMask = 1.0 - smoothstep(passRadius * 0.78, passRadius * 1.04, ellipseLen);
        float membraneNoise = 0.5 + 0.5 * sin(ep.x * 18.0 + ep.y * 13.0 + uTime * 2.7);
        float membrane = fillMask * (0.26 + 0.74 * membraneNoise);
        vec3 membraneColor = mix(vec3(0.26, 0.62, 0.96), vec3(0.70, 0.90, 1.00), membraneNoise * 0.60);
        color = mix(color, membraneColor, membrane * 0.46);
        alpha = max(alpha, passCoreColor.a * membrane * 0.58);

        // Electric rim sparks around the ellipse boundary.
        float arcNoise = 0.5 + 0.5 * sin(theta * 32.0 + uTime * 8.6 +
                                         sin(theta * 7.0 - uTime * 3.7) * 2.1);
        float arcMask = ringMask * pow(arcNoise, 9.0);
        vec3 arcColor = vec3(0.80, 0.95, 1.00);
        color += arcColor * arcMask * 0.88;
        alpha = max(alpha, passGlowColor.a * arcMask * 0.46);

        // Moving specular highlight, similar to a glossy shield shell.
        vec2 specDir = normalize(vec2(cos(uTime * 1.4), sin(uTime * 1.4)));
        float specular = pow(max(dot(epDir, specDir), 0.0), 22.0) * ringMask;
        color += vec3(1.0) * specular * 0.52;
        alpha = max(alpha, passGlowColor.a * specular * 0.35);
    }

    color = clamp(color, vec3(0.0), vec3(1.0));
    alpha = clamp(alpha, 0.0, 1.0);
    
    finalColor = vec4(color, alpha);
}
