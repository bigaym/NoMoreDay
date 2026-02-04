#version 330

// === TALENT NODE SHADER ===
// Procedural rendering for Astrolabe talent nodes.
// Handles different states (Locked, Available, Activated, FullyActivated, Sealed).

in vec2 fragTexCoord;
in vec4 fragColor;

uniform float uTime;
uniform vec4 uBaseColor; // Global base color (still uniform)

// REMOVED per-node uniforms: uStatus, uProgress, uShapeType
// They are now passed via fragColor (Vertex Color)

out vec4 finalColor;

// --- SDF Shapes ---
float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

float sdRegularPolygon(vec2 p, float r, int n) {
    float a = atan(p.x, p.y) + 3.14159;
    float b = 6.28318 / float(n);
    return cos(floor(0.5 + a/b) * b - a) * length(p) - r;
}

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

// Constants
const int STATUS_LOCKED = 0;
const int STATUS_AVAILABLE = 1;
const int STATUS_ACTIVATED = 2;
const int STATUS_FULLY_ACTIVATED = 3;
const int STATUS_SEALED = 4;

const vec3 C_AMBER = vec3(1.0, 0.84, 0.0);
const vec3 C_CYAN  = vec3(0.0, 1.0, 1.0);
const vec3 C_GOLD  = vec3(1.0, 0.84, 0.0);
const vec3 C_PURPLE = vec3(0.5, 0.0, 0.5);
const vec3 C_GREY  = vec3(0.3, 0.3, 0.3);

void main() {
    // Decode Data from Vertex Color (fragColor)
    // Raylib passes color as normalized vec4 (0.0 - 1.0)
    // R: Status (0/255, 1/255...)
    // G: Shape (0/255, 1/255...)
    // B: Progress (0.0 - 1.0 direct mapping)
    // A: Opacity
    
    int status = int(floor(fragColor.r * 255.0 + 0.5));
    int shapeType = int(floor(fragColor.g * 255.0 + 0.5));
    float progress = fragColor.b;
    float opacity = fragColor.a;

    // UV centered at 0.5 -> -0.5 to 0.5
    vec2 uv = fragTexCoord - 0.5;
    
    // Shape SDF
    float dist = 0.0;
    if (shapeType == 1) dist = sdRegularPolygon(uv, 0.42, 6); // Hexagon
    else if (shapeType == 2) dist = sdRegularPolygon(uv, 0.42, 8); // Octagon
    else dist = sdCircle(uv, 0.45); // Circle

    // Masks
    // Main shape mask
    float shapeMask = 1.0 - smoothstep(-0.01, 0.01, dist);
    // Border mask (outline)
    float borderMask = shapeMask - (1.0 - smoothstep(-0.01, 0.01, dist + 0.06));
    // Inner content mask
    float innerMask = 1.0 - smoothstep(-0.01, 0.01, dist + 0.06);
    
    vec3 color = vec3(1.0, 0.0, 1.0); // Default error color
    float alpha = shapeMask;

    float r = length(uv); // For gradients

    // Visual Logic based on Status
    if (status == STATUS_LOCKED) {
        vec3 lockedCore = vec3(0.08, 0.10, 0.12);
        color = lockedCore * innerMask;
        color += borderMask * vec3(0.35, 0.40, 0.45);
        alpha = shapeMask;
    } 
    else if (status == STATUS_AVAILABLE) {
        float pulse = 0.5 + 0.5 * sin(uTime * 3.0);
        color = mix(C_AMBER * 0.1, C_AMBER * 0.3, pulse) * innerMask;
        color += borderMask * C_AMBER * (0.8 + 0.2 * pulse);
        alpha = shapeMask;
    }
    else if (status == STATUS_ACTIVATED) {
        color = uBaseColor.rgb * 0.2 * innerMask;
        
        // Progress Ring Logic (Radial)
        // Need angle for radial progress
        float angle = atan(uv.y, uv.x); // -PI to PI
        // Remap to 0-1 starting from top?
        // Let's keep it simple: just use shapeMask for now or standard radial if shape is circle
        // For polygons, radial progress is tricky. Let's just fill the border based on progress?
        // Or simpler: Progress dictates how "full" the inner part is?
        // Let's stick to the ring effect but clipped to the border mask.
        
        float progressAngle = progress * 6.28318;
        float curAngle = atan(uv.x, uv.y); 
        if (curAngle < 0.0) curAngle += 6.28318;
        
        float ringFill = step(curAngle, progressAngle);
        
        vec3 ringColor = C_CYAN * 1.5;
        color += borderMask * ringFill * ringColor;
        color += borderMask * (1.0 - ringFill) * C_GREY * 0.5;
        
        alpha = shapeMask;
    }
    else if (status == STATUS_FULLY_ACTIVATED) {
        // Gold
        float rays = 0.5 + 0.5 * sin(atan(uv.y, uv.x) * 12.0 + uTime * 2.0);
        vec3 baseGold = C_GOLD;
        color = baseGold * innerMask * (0.8 + 0.2 * rays);
        color += borderMask * C_GOLD * 2.0;
        color += smoothstep(0.1, 0.0, r) * vec3(1.0); // Highlight center
        alpha = shapeMask;
    }
    else if (status == STATUS_SEALED) {
        float n = fbm(uv * 10.0 + vec2(uTime * 0.5));
        vec3 sealColor = mix(C_PURPLE * 0.2, C_PURPLE * 0.8, n);
        color = sealColor * innerMask;
        color += borderMask * C_PURPLE * 0.5;
        alpha = shapeMask;
    }
    

    finalColor = vec4(color, alpha * opacity);
}


