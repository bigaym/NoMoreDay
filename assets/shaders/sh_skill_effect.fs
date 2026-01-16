#version 430

in vec2 localPos;
in vec4 passCoreColor;
in vec4 passGlowColor;
in vec2 passDirection; // Direction the projectile is facing
in float passAngle;    // Sector Half-Angle? No, assuming full angle spread for now.
in float passRadius;   // The radius boundary in local space (e.g. 0.8)
in float passSoftness;
in float passType;

out vec4 finalColor;

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
    } else {
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
    vec3 bodyColor = passCoreColor.rgb;  // Cyan/Ink Body
    
    // Mix Body and Rim
    vec3 finalInnerColor = mix(bodyColor, rimColor, rimFactor);
    float finalInnerAlpha = passCoreColor.a; // Keep opacity
    
    // 2. Glow Logic (Outside dist > 0)
    // Reduce brightness
    float glowFactor = 1.0 - smoothstep(0.0, passRadius * 0.5, dist); // Quicker falloff
    vec3 finalGlowColor = passGlowColor.rgb;
    float finalGlowAlpha = passGlowColor.a * glowFactor * 0.4; // Reduced intesity (was 0.8)
    
    // Composite
    if (dist <= 0.0) {
        color = finalInnerColor;
        alpha = finalInnerAlpha;
    } else {
        color = finalGlowColor;
        alpha = finalGlowAlpha;
    }
    
    finalColor = vec4(color, alpha);
}
