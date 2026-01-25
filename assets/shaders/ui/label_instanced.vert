#version 430 core

// Instanced Item Label Shader
// Draws rounded rectangles with SDF in fragment shader.
// Supports batching via SSBO.

// Input: Quad Vertices (0-3)
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;

// Uniforms
uniform mat4 mvp; // Model-View-Projection Matrix (Usually Orthographic for UI)

// --- Instance Data Layout (std430) ---
struct GPULabelInstance {
    vec2 position;      // Screen/World Position
    vec2 size;          // Width, Height
    vec4 bgColor;       // Background Color
    vec4 borderColor;   // Border Color
    float borderWidth;  // Border Thickness
    float cornerRadius; // Radius
    vec2 padding;       // 64-byte alignment
};

layout(std430, binding = 4) readonly buffer InstanceBuffer {
    GPULabelInstance instances[];
};

// Output to Fragment Shader
out vec2 fragTexCoord;    // 0.0 to 1.0
out vec2 fragSize;        // Pixel dimensions
out vec4 fragBgColor;
out vec4 fragBorderColor;
out float fragBorderWidth;
out float fragCornerRadius;

void main() {
    GPULabelInstance instance = instances[gl_InstanceID];

    // Calculate vertex position in world/screen space
    // Quad is typically defined as [0,0] to [1,1] or similar in Raylib
    // We scale it by size and translate by position
    
    // Assuming input vertexPosition is a unit quad [0,0] -> [1,1]
    // Raylib's default quad for DrawTexture is usually defined this way.
    // If using custom geometry, ensure it matches.
    
    // Transform: Scale -> Translate
    vec2 worldPos = instance.position + (vertexPosition.xy * instance.size);
    
    // Final position
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
    
    // Pass data to fragment
    fragTexCoord = vertexTexCoord; // [0,1]
    fragSize = instance.size;
    fragBgColor = instance.bgColor;
    fragBorderColor = instance.borderColor;
    fragBorderWidth = instance.borderWidth;
    fragCornerRadius = instance.cornerRadius;
}
