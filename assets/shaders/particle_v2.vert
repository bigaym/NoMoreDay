#version 430

// Vertex attributes - simple quad
layout(location = 0) in vec2 vertexPos;  // [-0.5, 0.5] range

// Particle structure - MUST match compute shader
struct Particle {
    vec2 position;
    vec2 velocity;
    vec2 acceleration;
    uint color;
    float lifetime;
    float maxLifetime;
    float scale;
    uint flags;
    float growthRate;
    float pad[4];
};

// Compacted particles buffer (only alive particles)
layout(std430, binding = 0) readonly buffer CompactBuffer {
    Particle particles[];
};

// MVP matrix for 2D camera transform
uniform mat4 mvp;

// Outputs to fragment shader
out vec4 fragColor;
out flat uint vFlags;
out vec2 vTexCoord;

void main() {
    // Get particle data using instance ID
    uint id = gl_InstanceID;
    Particle p = particles[id];
    
    // Safety check - should not happen with proper compaction
    if (p.lifetime <= 0.0 || p.scale <= 0.0) {
        gl_Position = vec4(-9999.0, -9999.0, 0.0, 1.0);
        return;
    }
    
    // Unpack color from uint (RGBA format, little-endian)
    vec4 col = vec4(
        float(p.color & 0xFFu) / 255.0,           // R
        float((p.color >> 8) & 0xFFu) / 255.0,    // G
        float((p.color >> 16) & 0xFFu) / 255.0,   // B
        float((p.color >> 24) & 0xFFu) / 255.0    // A
    );
    
    // Apply lifetime-based alpha fade
    float lifetimeRatio = clamp(p.lifetime / p.maxLifetime, 0.0, 1.0);
    col.a *= lifetimeRatio;
    
    // Pass to fragment shader
    fragColor = col;
    vFlags = p.flags;
    vTexCoord = vertexPos + 0.5;  // Convert to [0, 1] range
    
    // Calculate world position
    // vertexPos is in [-0.5, 0.5], scale by particle size
    // Note: p.scale was used as Radius in V1, here it's Diameter. Multiply by 2.0 to match visual size.
    vec2 worldPos = vertexPos * (p.scale * 2.0) + p.position;
    
    // Apply MVP transform
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
}
