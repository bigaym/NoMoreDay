#version 430

layout(location = 0) in vec2 vertexPos; 

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

// Vert shader reads from the COMPACTED buffer (always use slot 11 in Render)
layout(std430, binding = 11) buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 mvp;

out vec4 fragColor;
out flat uint vFlags; 
out vec2 vTexCoord;   

void main() {
    uint id = gl_InstanceID;
    Particle p = particles[id];
    
    // In this compacted system, Vertex shader is only called for m_aliveCount instances.
    // So all p[id] should be alive. But we keep safety.
    if (p.lifetime <= 0.0) {
        gl_Position = vec4(-5.0);
        return;
    }

    uint c = p.color;
    vec4 col = vec4(
        float(c & 0xFFu) / 255.0,
        float((c >> 8) & 0xFFu) / 255.0,
        float((c >> 16) & 0xFFu) / 255.0,
        float((c >> 24) & 0xFFu) / 255.0
    );
    
    // Alpha fade based on lifetime
    col.a *= clamp(p.lifetime / p.maxLifetime, 0.0, 1.0);

    fragColor = col;
    vFlags = p.flags;
    vTexCoord = vertexPos + 0.5;

    vec2 worldPos = vertexPos * p.scale + p.position;
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
}
