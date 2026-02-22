#version 430
layout(location = 0) in vec2 vertexPos; 

struct SkillEffect {
    vec2 position;
    vec2 velocity;
    vec4 coreColor;
    vec4 glowColor;
    float radius;
    float sectorAngle;
    uint flags;
    float type;
};

// Binding 来源: RenderConstants::Binding::SSBO_SKILL_EFFECTS (6)
layout(std430, binding = 6) buffer SkillEffectBuffer { SkillEffect effects[]; };

uniform mat4 mvp;

// Outputs to Fragment Shader
out vec2 localPos;      // Local coordinate [-1, 1]
out vec4 passCoreColor;
out vec4 passGlowColor;
out vec2 passDirection; // Direction vector from velocity
out float passAngle;    // Sector Angle (in radians)
out float passRadius;   // Radius for SDF calculation
flat out uint passFlags;
out float passType;

void main() {
    uint id = gl_InstanceID;
    SkillEffect e = effects[id];
    
    if (e.radius <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 0.0, 1.0); // Clip
        return;
    }

    localPos = vertexPos * 2.0; // [-1, 1] range
    
    // Calculate world position
    // We expand the quad slightly larger than radius to allow for soft edges/glow
    // User Feedback: Range too big. Adjusted to match Hitbox more closely.
    // 0.8 * 1.3 = 1.04x Hitbox (Edge)
    float renderRadius = e.radius * 1.3; 
    vec2 worldPos = vertexPos * renderRadius + e.position;
    
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
    
    // Pass attributes
    passCoreColor = e.coreColor;
    passGlowColor = e.glowColor;
    
    if (length(e.velocity) > 0.001) {
        passDirection = normalize(e.velocity);
    } else {
        passDirection = vec2(1.0, 0.0);
    }
    
    passAngle = radians(e.sectorAngle);
    passRadius = e.radius; // Logical radius (1.0 in localPos space relative to e.radius)
    // Actually simpler: Work in Pixel Space? Or Unit Space?
    // Let's stick to Local Space [-1, 1] where 1.0 ~= renderRadius / 2
    // But e.radius is the gameplay radius.
    // If renderRadius = e.radius * 2.5, then e.radius corresponds to localPos length of (e.radius / (e.radius * 1.25)) = 0.8
    // So 0.8 is the hard edge in local space.
    passRadius = 0.8; 
    
    passFlags = e.flags;
    passType = e.type;
}
