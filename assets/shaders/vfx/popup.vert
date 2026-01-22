#version 430 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

struct GPUPopupInstance {
    vec2  position;
    vec2  velocity;
    float timer;
    float lifeTime;
    uint  glyphData;   // digit index (0-9) or special (+, -, !)
    uint  colorPacked;
    uint  flags;       // bit0: isCrit, bit1-7: charIndexInString
    float scale;
    float padding[2];
};

layout(std430, binding = 0) buffer InstanceBuffer {
    GPUPopupInstance instances[];
};

uniform mat4 uViewProj;

out vec2 fragTexCoord;
out vec4 fragColor;

void main() {
    GPUPopupInstance inst = instances[gl_InstanceID];
    
    float t = inst.timer / inst.lifeTime;
    float alpha = 1.0 - smoothstep(0.7, 1.0, t);
    
    // Decompress color
    vec4 color = vec4(
        float(inst.colorPacked & 0xFFu) / 255.0,
        float((inst.colorPacked >> 8) & 0xFFu) / 255.0,
        float((inst.colorPacked >> 16) & 0xFFu) / 255.0,
        float((inst.colorPacked >> 24) & 0xFFu) / 255.0
    );

    // Animation: scaling for crits and entry
    float scale = inst.scale;
    if ((inst.flags & 1u) != 0u) {
        scale *= (1.0 + 0.3 * sin(inst.timer * 20.0) * exp(-inst.timer * 2.0));
    }
    
    // Billboard calculation:
    // Move to world space position
    vec2 worldPos = inst.position;
    
    // Offset based on character index in string (to avoid overlap)
    uint charIdx = (inst.flags >> 1) & 0x7Fu;
    uint charCount = (inst.flags >> 8) & 0xFFu;
    
    float charWidth = 4.5 * scale; 
    float totalWidth = float(charCount) * charWidth;
    worldPos.x += (float(charIdx) - float(charCount-1) * 0.5) * charWidth; 
    
    // Final vertex position in world space
    vec2 pos = worldPos + aPos * vec2(12.0, 12.0) * scale;
    
    gl_Position = uViewProj * vec4(pos, 0.0, 1.0);
    
    // Atlas UV: 16 columns per row, 2 rows total
    float col = float(inst.glyphData % 16u);
    float row = float(inst.glyphData / 16u);
    
    float uStart = col / 16.0;
    // PIL Row 0 (Digits) is at the TOP of the image.
    // In Raylib/Common GL setups, V=0.0 is often the TOP of the texture.
    // Row 0 -> vStart = 0.0, Row 1 -> vStart = 0.5
    float vStart = (row == 0.0) ? 0.0 : 0.5;
    
    // Offset UV based on current vertex corner (aTexCoord is 0..1)
    fragTexCoord = vec2(uStart + aTexCoord.x / 16.0, vStart + aTexCoord.y / 2.0);
    
    fragColor = color;
    fragColor.a *= alpha;
}
