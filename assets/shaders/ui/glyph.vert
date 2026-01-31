#version 430 core

layout(location = 0) in vec3 aPos;      // [0, 1] Range unit quad
layout(location = 1) in vec2 aTexCoord; // [0, 1]

struct GPUGlyphInstance {
    vec2  position;     // 8
    vec2  size;         // 8
    vec2  uvMin;        // 8
    vec2  uvMax;        // 8
    uint  colorPacked;  // 4
    float scale;        // 4
    float padding[2];   // 8
};

// Binding: RenderConstants::Binding::SSBO_GLYPH_INSTANCE (8)
layout(std430, binding = 8) buffer InstanceBuffer {
    GPUGlyphInstance instances[];
};

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec4 fragColor;

void main() {
    GPUGlyphInstance inst = instances[gl_InstanceID];
    
    // Decompress color (RGBA8)
    vec4 color = vec4(
        float(inst.colorPacked & 0xFFu) / 255.0,
        float((inst.colorPacked >> 8) & 0xFFu) / 255.0,
        float((inst.colorPacked >> 16) & 0xFFu) / 255.0,
        float((inst.colorPacked >> 24) & 0xFFu) / 255.0
    );

    // Calculate vertex position in world space
    vec2 worldPos = inst.position + aPos.xy * inst.size * inst.scale;
    
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
    
    // Mix UV based on vertex corner
    fragTexCoord = mix(inst.uvMin, inst.uvMax, aTexCoord);
    
    fragColor = color;
}
