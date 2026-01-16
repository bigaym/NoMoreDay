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
    } else {

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
        // Using max(dx, dy) creates a sharp intersection
        dist = max(dx, dy);
    }
    
    // SDF Rendering logic
    // dist < 0 : Inside
    // dist > 0 : Outside
    
    // Core: Sharp inside
    // Glow: Fade outside
    
    float alpha = 0.0;
    vec3 color = vec3(0.0);
    
    // Hard edge with antialiasing
    // 1.0 inside, 0.0 outside
    float coreAlpha = 1.0 - smoothstep(-0.02, 0.0, dist);
    
    // Glow
    // Fade out as dist increases
    float glowDist = 0.2; // How far the glow extends
    float glowAlpha = 1.0 - smoothstep(0.0, glowDist, dist);
    
    // Inside Glow (hot core)
    // As we get deeper inside (more negative), it gets hotter
    float innerGlow = smoothstep(-passRadius, -0.0, dist);
    
    // Combine
    vec3 coreMix = mix(passCoreColor.rgb, vec3(1.0), 0.5 * (1.0 - innerGlow)); // Whiter at center
    vec3 glowMix = passGlowColor.rgb;
    
    // Composite
    if (dist <= 0.0) {
        // Inside
        color = coreMix;
        alpha = passCoreColor.a * coreAlpha;
    } else {
        // Outside (Glow only)
        color = glowMix;
        alpha = passGlowColor.a * glowAlpha * 0.8; // Glow is weaker
    }
    
    finalColor = vec4(color, alpha);
}
