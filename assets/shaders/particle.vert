#version 430

layout(location = 0) in vec2 vertexPos; // Unit quad vertex [-0.5, 0.5]

struct Particle {
    vec2 position;
    vec2 velocity;
    uint color;
    float lifetime;
    float maxLifetime;
    float scale;
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 mvp;

out vec4 fragColor;

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
    
    // Simple fade out
    col.a *= (p.lifetime / p.maxLifetime);
    fragColor = col;

    // Instance transform
    vec2 worldPos = vertexPos * p.scale + p.position;
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
}
