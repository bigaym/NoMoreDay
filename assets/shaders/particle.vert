#version 430

layout(location = 0) in vec2 vertexPos; // Unit quad vertex [-0.5, 0.5]

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
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 mvp;

out vec4 fragColor;
out flat uint vFlags; // Pass flags to frag
out vec2 vTexCoord;   // Pass UVs (derived from vertexPos)

void main() {
    uint id = gl_InstanceID;
    Particle p = particles[id];
    
    if (p.lifetime <= 0.0 || p.scale <= 0.0) {
        // Discard by moving out of clip space
        gl_Position = vec4(-10.0, -10.0, 0.0, 1.0);
        return;
    }

    // Unpack RGBA8 color from uint
    // Raylib Color is { r, g, b, a }, as uint (little endian) it's 0xAABBGGRR
    vec4 col = vec4(
        float(p.color & 0xFFu) / 255.0,
        float((p.color >> 8) & 0xFFu) / 255.0,
        float((p.color >> 16) & 0xFFu) / 255.0,
        float((p.color >> 24) & 0xFFu) / 255.0
    );
    
    float lifeRatio = p.lifetime / p.maxLifetime; // 1.0 -> 0.0

    // Check for Fade In/Out flag (Bit 3: 0x8)
    if ((p.flags & 8u) != 0u) {
        // Ink Fade: Fade In at start (1.0->0.8), Fade Out at end (0.2->0.0)
        // Fade In: smoothstep(0.8, 1.0, lifeRatio) gives 0->1 as lifeRatio goes 0.8->1.0. 
        // Wait, lifeRatio goes 1.0 -> 0.0.
        // At 1.0 (birth): We want Alpha 0 -> 1. 
        // Let's say Fade In lasts for top 20% of life.
        float fadeIn = 1.0 - smoothstep(0.8, 1.0, lifeRatio);
        
        // Fade Out: We want Alpha 1 -> 0 at end.
        float fadeOut = smoothstep(0.0, 0.2, lifeRatio);

        // Combine
        col.a *= fadeOut * (1.0 - fadeIn); // Incorrect logic above.
        
        // Let's retry logic:
        // We want Alpha to be 1.0 in the middle.
        // Start (1.0): Alpha 0. 
        // End (0.0): Alpha 0.
        
        // Fade In: We want it to be 0 at 1.0, 1 at 0.8.
        // smoothstep(0.8, 1.0, lifeRatio) -> returns 0 at 0.8, 1 at 1.0.
        // So (1.0 - smoothstep(0.8, 1.0, lifeRatio)) -> returns 1 at 0.8, 0 at 1.0. CORRECT.
        
        // Fade Out: We want it to be 0 at 0.0, 1 at 0.2.
        // smoothstep(0.0, 0.2, lifeRatio) -> returns 0 at 0.0, 1 at 0.2. CORRECT.
        
        col.a *= (1.0 - smoothstep(0.8, 1.0, lifeRatio)) * smoothstep(0.0, 0.2, lifeRatio);
    } else {
        // Standard Linear Fade Out
        col.a *= lifeRatio;
    }

    fragColor = col;
    vFlags = p.flags;
    vTexCoord = vertexPos + 0.5; // Map [-0.5, 0.5] to [0.0, 1.0]

    // Instance transform
    vec2 worldPos = vertexPos * p.scale + p.position;
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
}
