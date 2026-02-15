#version 430
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;

struct HoloBladeInstance {
    vec2 position;
    float rotation;
    float scale;
    vec4 holoColor;
    float rimStrength;
    float noiseSpeed;
    vec2 padding;
};

// Binding source: RenderConstants::HoloBladeBinding::INSTANCE (14)
layout(std430, binding = 14) readonly buffer InstanceBuffer {
    HoloBladeInstance instances[];
};

out vec2 fragTexCoord;
out vec4 fragHoloColor;
out float fragRimStrength;
out float fragNoiseSpeed;

uniform mat4 mvp;
uniform int uInstanceOffset;

void main() {
    HoloBladeInstance inst = instances[uInstanceOffset + gl_InstanceID];
    
    // Rotation matrix (Note: GLSL mat2 is column-major)
    float s = sin(inst.rotation);
    float c = cos(inst.rotation);
    mat2 rot = mat2(c, s, -s, c);
    
    // Position 0..1 to local space? No, vertexPosition is usually -0.5..0.5
    vec2 pos = vertexPosition.xy * inst.scale;
    pos = rot * pos;
    pos += inst.position;
    
    fragTexCoord = vertexTexCoord;
    fragHoloColor = inst.holoColor;
    fragRimStrength = inst.rimStrength;
    fragNoiseSpeed = inst.noiseSpeed;
    
    gl_Position = mvp * vec4(pos, vertexPosition.z, 1.0);
}
