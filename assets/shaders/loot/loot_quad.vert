#version 430 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

layout(std430, binding = 15) readonly buffer LootInstanceBuffer {
  uvec4 lootWords[];
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

vec4 unpackColor(uint packedColor) {
  // unpackUnorm4x8 returns bytes low->high as x,y,z,w; our packed color is
  // high->low as R,G,B,A, so remap to RGBA explicitly.
  vec4 unpacked = unpackUnorm4x8(packedColor);
  return vec4(unpacked.w, unpacked.z, unpacked.y, unpacked.x);
}

void main() {
  uint lootIndex = visibleIndices[gl_InstanceID];
  uint base = lootIndex * 2u;
  uvec4 w0 = lootWords[base + 0u];
  uvec4 w1 = lootWords[base + 1u];

  vec2 worldPos = vec2(uintBitsToFloat(w0.x), uintBitsToFloat(w0.y));
  vec2 labelOffset = vec2(uintBitsToFloat(w0.z), uintBitsToFloat(w0.w));
  uint itemId = w1.x;
  uint rarityColor = w1.y;
  float glowIntensity = uintBitsToFloat(w1.z);
  uint flags = w1.w;

  bool isGold = (flags & 1u) != 0u;
  vec2 size = isGold ? vec2(72.0, 20.0) : vec2(92.0, 24.0);
  vec2 center = worldPos + labelOffset;
  vec2 finalPos = center + aPos * size;

  gl_Position = uMVP * vec4(finalPos, 0.0, 1.0);
  vUV = aUV;
  vRarityColor = unpackColor(rarityColor);
  vGlow = glowIntensity;
  vFlags = flags;
  vItemId = itemId;
}
