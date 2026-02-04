#version 330

// === TALENT NODE SHADER ===
// Procedural rendering for Astrolabe talent nodes.
// Handles different states (Locked, Available, Activated, FullyActivated, Sealed).

in vec2 fragTexCoord;
in vec4 fragColor;

uniform float uTime;
uniform int uStatus;       // 0=Locked, 1=Available, 2=Activated, 3=FullyActivated, 4=Sealed
uniform float uProgress;   // 0.0 ~ 1.0 (Points/MaxPoints)
uniform vec4 uBaseColor;

out vec4 finalColor;

// Constants
const int STATUS_LOCKED = 0;
const int STATUS_AVAILABLE = 1;
const int STATUS_ACTIVATED = 2;
const int STATUS_FULLY_ACTIVATED = 3;
const int STATUS_SEALED = 4;

const vec3 C_AMBER = vec3(1.0, 0.84, 0.0);   // #FFD700
const vec3 C_CYAN  = vec3(0.0, 1.0, 1.0);
const vec3 C_GOLD  = vec3(1.0, 0.84, 0.0);
const vec3 C_PURPLE = vec3(0.5, 0.0, 0.5);   // #800080
const vec3 C_GREY  = vec3(0.3, 0.3, 0.3);

// Utils
float hash(float n) { return fract(sin(n) * 43758.5453123); }

float noise(vec2 x) {
    vec2 p = floor(x);
    vec2 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0;
    return mix(mix(hash(n), hash(n + 1.0), f.x),
               mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y);
}

float fbm(vec2 p) {
    float f = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 4; i++) {
        f += amp * noise(p);
        p = p * 2.02;
        amp *= 0.5;
    }
    return f;
}

void main() {
    // UV centered at 0.5 -> -0.5 to 0.5
    vec2 uv = fragTexCoord - 0.5;
    float r = length(uv);
    float angle = atan(uv.y, uv.x); // -PI to PI
    
    // Normalize angle to 0 ~ 1
    float normAngle = (angle / 6.28318) + 0.5;
    
    vec3 color = vec3(0.0);
    float alpha = 1.0;
    
    // Base Circle Mask
    float circle = smoothstep(0.45, 0.44, r);
    float border = smoothstep(0.45, 0.44, r) - smoothstep(0.40, 0.39, r);
    float inner = smoothstep(0.40, 0.39, r);
    
    if (uStatus == STATUS_LOCKED) {
        // Dark Grey, faint border
        color = C_GREY * 0.2;
        color += border * C_GREY * 0.5;
        alpha = circle; // Faint
    } 
    else if (uStatus == STATUS_AVAILABLE) {
        // Amber Pulsing
        float pulse = 0.5 + 0.5 * sin(uTime * 3.0);
        color = mix(C_AMBER * 0.1, C_AMBER * 0.3, pulse) * inner;
        // Stronger border
        color += border * C_AMBER * (0.8 + 0.2 * pulse);
        alpha = circle;
    }
    else if (uStatus == STATUS_ACTIVATED) {
        // Activated: Progress Ring + Inner Fill
        
        // Inner Fill: Base Color (usually Greyish until full)
        color = uBaseColor.rgb * 0.2 * inner;
        
        // Progress Ring (Cyan)
        float progressAngle = uProgress * 6.28318;
        
        // Calculate angle from top (0, 1) going clockwise
        // x/y swapped: Top (0,1) is 0, Right (1,0) is PI/2
        float curAngle = atan(uv.x, uv.y); 
        if (curAngle < 0.0) curAngle += 6.28318;
        
        float ringMask = step(curAngle, progressAngle);
        
        // Ring visual
        vec3 ringColor = C_CYAN * 1.5; // Bright Cyan
        color += border * ringMask * ringColor;
        color += border * (1.0 - ringMask) * C_GREY * 0.5; // Inactive part of ring
        
        alpha = circle;
    }
    else if (uStatus == STATUS_FULLY_ACTIVATED) {
        // Saturated Gold, Radial Rays
        
        // Rays
        float rays = 0.5 + 0.5 * sin(angle * 12.0 + uTime * 2.0);
        
        // Inner Glow
        float glow = exp(-r * 2.0);
        
        vec3 baseGold = C_GOLD;
        color = baseGold * inner * (0.8 + 0.2 * rays);
        
        // Border Shine
        color += border * C_GOLD * 2.0;
        
        // Center Hotspot
        color += smoothstep(0.1, 0.0, r) * vec3(1.0);
        
        alpha = circle;
    }
    else if (uStatus == STATUS_SEALED) {
        // Purple Sealed, Dynamic Noise
        float n = fbm(uv * 10.0 + vec2(uTime * 0.5));
        
        vec3 sealColor = mix(C_PURPLE * 0.2, C_PURPLE * 0.8, n);
        color = sealColor * circle;
        
        // Dark chain/lock pattern? 
        // Keeping it simple as per spec: "Purple noise"
        
        // Add a darker border
        color += border * C_PURPLE * 0.5;
        
        alpha = circle;
    }
    
    finalColor = vec4(color, alpha * fragColor.a);
}
