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
        // Ink Fade: 
        // Fade In: Very fast (1.0 -> 0.9) - Ink hits paper
        float fadeIn = 1.0 - smoothstep(0.9, 1.0, lifeRatio);
        
        // Fade Out: Slow and lingering (0.6 -> 0.0) - Ink drying
        float fadeOut = smoothstep(0.0, 0.6, lifeRatio);
        // Apply power curve to make it linger (convex curve)
        fadeOut = pow(fadeOut, 0.8);

        col.a *= fadeIn * fadeOut;
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
