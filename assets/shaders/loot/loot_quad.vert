#version 430
#include "generated/gpu_abi.glslinc"

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

layout(std430, binding = 15) readonly buffer LootInstanceBuffer {
  GPULootInstance instances[];
};

layout(std430, binding = 1) readonly buffer VisibleIndexBuffer {
  uint visibleIndices[];
};

uniform mat4 uMVP;

out vec2 vUV;
out vec4 vRarityColor;
out float vGlow;
flat out uint vFlags;
flat out uint vItemId;

vec4 unpackColor(uint packed) {
  float r = float((packed >> 24u) & 0xFFu) / 255.0;
  float g = float((packed >> 16u) & 0xFFu) / 255.0;
  float b = float((packed >> 8u) & 0xFFu) / 255.0;
  float a = float((packed) & 0xFFu) / 255.0;
  return vec4(r, g, b, a);
}

void main() {
  uint lootIndex = visibleIndices[gl_InstanceID];
  GPULootInstance inst = instances[lootIndex];

  bool isGold = (inst.flags & 1u) != 0u;
  vec2 size = isGold ? vec2(72.0, 20.0) : vec2(92.0, 24.0);
  vec2 center = vec2(inst.worldPosX + inst.labelOffsetX, inst.worldPosY + inst.labelOffsetY);
  vec2 worldPos = center + aPos * size;

  gl_Position = uMVP * vec4(worldPos, 0.0, 1.0);
  vUV = aUV;
  vRarityColor = unpackColor(inst.rarityColor);
  vGlow = inst.glowIntensity;
  vFlags = inst.flags;
  vItemId = inst.itemId;
}
