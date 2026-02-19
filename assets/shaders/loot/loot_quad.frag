#version 430

in vec2 vUV;
in vec4 vRarityColor;
in float vGlow;
flat in uint vFlags;
flat in uint vItemId;

uniform int uGlowEnabled;

out vec4 FragColor;

float roundedBox(vec2 p, vec2 b, float r) {
  vec2 q = abs(p) - b + vec2(r);
  return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
  vec2 centered = vUV * 2.0 - 1.0;
  float sdf = roundedBox(centered, vec2(0.95, 0.85), 0.20);

  if (sdf > 0.02) {
    discard;
  }

  float borderWidth = 0.06;
  float borderMask = smoothstep(borderWidth, borderWidth - 0.02, abs(sdf));

  vec3 bg = mix(vec3(0.04, 0.04, 0.05), vRarityColor.rgb * 0.22, 0.35);
  vec3 color = mix(bg, vRarityColor.rgb, borderMask);

  vec2 iconUv = (vUV - vec2(0.08, 0.2)) / vec2(0.24, 0.6);
  bool inIcon = iconUv.x >= 0.0 && iconUv.x <= 1.0 && iconUv.y >= 0.0 && iconUv.y <= 1.0;
  if (inIcon) {
    float hash = fract(sin(float(vItemId) * 12.9898) * 43758.5453);
    vec3 iconColor = mix(vec3(0.82), vRarityColor.rgb, hash);
    color = mix(color, iconColor, 0.85);
  }

  if (uGlowEnabled != 0) {
    float glow = smoothstep(0.65, 0.0, abs(sdf)) * max(vGlow, 0.0) * 0.8;
    color += vRarityColor.rgb * glow;
  }

  float alpha = 0.78 + borderMask * 0.22;
  if ((vFlags & 1u) != 0u) {
    color = mix(color, vec3(0.98, 0.84, 0.25), 0.18);
  }
  FragColor = vec4(color, alpha);
}
