#version 430 core
#include "generated/gpu_abi.glslinc"

layout(location = 0) in vec2 aPos;

// Binding 来源: Entity-MDI pass-local binding = 2 (PackedInstances)
layout(std430, binding = 2) readonly buffer PackedInstanceBuffer {
    GPUPackedEntityInstance instances[];
};

uniform mat4 viewProj;
uniform float interpolationFactor;
uniform float uTime;

out vec2 vTexCoord;
out vec2 vLocalPos;
flat out int vTextureIndex;
flat out uint vFlags;
flat out float vGlow;
flat out uint vStatusMask;
flat out float vTime;

void main() {
    // 直接按 gl_InstanceID 读取 packed stream
    GPUPackedEntityInstance inst = instances[gl_InstanceID];

    // 插值位置
    vec2 interpolatedPos = mix(inst.prevPosition, inst.position, interpolationFactor);

    // 解码朝向 sin/cos
    vec2 sc = unpackSnorm2x16(inst.words[0]);
    float s_rot = sc.x;
    float c = sc.y;
    mat2 rot = mat2(c, -s_rot, s_rot, c);

    // 解码渲染尺寸与纹理索引
    float renderRadius = unpackHalf2x16(inst.words[1]).x;
    uint rawTex = (inst.words[1] >> 16u) & 0xFFFFu;
    vTextureIndex = (rawTex == 0xFFFFu) ? -1 : int(rawTex);

    vec2 pos = aPos * (renderRadius * 2.0);
    pos = rot * pos;
    vec2 worldPos = interpolatedPos + pos;

    gl_Position = viewProj * vec4(worldPos, 0.0, 1.0);

    vTexCoord = aPos + 0.5;
    vLocalPos = aPos * 2.0;

    // 解码 Material, Glow, Status Mask, Flags
    uint w2 = inst.words[2];
    uint materialId = w2 & 0xFFFFu;
    vGlow = float((w2 >> 16u) & 0xFFu) / 255.0;
    vStatusMask = (w2 >> 24u) & 0xFFu;

    vFlags = (inst.words[3] & 0xFFFFu) | (materialId << 16u);
    vTime = uTime;
}
