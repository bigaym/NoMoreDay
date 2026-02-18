#pragma once

#include <cstdint>

namespace NoMoreDay::render {

enum class ShaderVariant : uint8_t {
  Default = 0,
  Ink,
  Hologram,
  Fire,
  Ice,
  Lightning,
  Dissolve,
  Count
};

enum class BlendMode : uint8_t {
  Alpha = 0,
  Additive,
  Multiply,
  Count
};

struct MaterialInstance {
  float baseColorR = 1.0f;
  float baseColorG = 1.0f;
  float baseColorB = 1.0f;
  float baseColorA = 1.0f;

  float emissiveR = 0.0f;
  float emissiveG = 0.0f;
  float emissiveB = 0.0f;
  float emissiveIntensity = 0.0f;

  float distortion = 0.0f;
  BlendMode blendMode = BlendMode::Alpha;
  ShaderVariant shader = ShaderVariant::Default;
  uint8_t padding0 = 0;

  // textureSlots: [albedo, reserved1, roughness, reserved3]
  int16_t textureSlots[4] = {-1, -1, -1, -1};
  int16_t normalMapSlot = -1;
  float roughness = 0.6f;
  float specular = 0.2f;
  float ao = 1.0f;
  float heightBias = 0.0f;
  float detailNormalScale = 1.0f;
};

namespace MaterialPresets {

constexpr MaterialInstance Default() { return MaterialInstance{}; }

constexpr MaterialInstance InkSplash() {
  MaterialInstance m = Default();
  m.baseColorR = 0.08f;
  m.baseColorG = 0.08f;
  m.baseColorB = 0.14f;
  m.baseColorA = 0.86f;
  m.shader = ShaderVariant::Ink;
  m.blendMode = BlendMode::Alpha;
  return m;
}

constexpr MaterialInstance FireGlow() {
  MaterialInstance m = Default();
  m.baseColorR = 1.0f;
  m.baseColorG = 0.31f;
  m.baseColorB = 0.12f;
  m.baseColorA = 1.0f;
  m.emissiveR = 1.0f;
  m.emissiveG = 0.5f;
  m.emissiveB = 0.1f;
  m.emissiveIntensity = 2.0f;
  m.blendMode = BlendMode::Additive;
  m.shader = ShaderVariant::Fire;
  return m;
}

constexpr MaterialInstance IceCrystal() {
  MaterialInstance m = Default();
  m.baseColorR = 0.31f;
  m.baseColorG = 0.71f;
  m.baseColorB = 1.0f;
  m.baseColorA = 0.8f;
  m.emissiveR = 0.5f;
  m.emissiveG = 0.8f;
  m.emissiveB = 1.0f;
  m.emissiveIntensity = 1.5f;
  m.shader = ShaderVariant::Ice;
  return m;
}

constexpr MaterialInstance LightningArc() {
  MaterialInstance m = Default();
  m.baseColorR = 1.0f;
  m.baseColorG = 1.0f;
  m.baseColorB = 0.39f;
  m.baseColorA = 1.0f;
  m.emissiveR = 1.0f;
  m.emissiveG = 1.0f;
  m.emissiveB = 0.5f;
  m.emissiveIntensity = 3.0f;
  m.blendMode = BlendMode::Additive;
  m.shader = ShaderVariant::Lightning;
  return m;
}

constexpr MaterialInstance HoloBlade() {
  MaterialInstance m = Default();
  m.baseColorR = 0.78f;
  m.baseColorG = 0.90f;
  m.baseColorB = 1.0f;
  m.baseColorA = 0.63f;
  m.emissiveR = 0.8f;
  m.emissiveG = 0.95f;
  m.emissiveB = 1.0f;
  m.emissiveIntensity = 1.0f;
  m.shader = ShaderVariant::Hologram;
  return m;
}

constexpr MaterialInstance ShadowVoid() {
  MaterialInstance m = Default();
  m.baseColorR = 0.16f;
  m.baseColorG = 0.04f;
  m.baseColorB = 0.24f;
  m.baseColorA = 1.0f;
  m.distortion = 0.5f;
  m.shader = ShaderVariant::Dissolve;
  return m;
}

constexpr MaterialInstance DistortionShockwave() {
  MaterialInstance m = Default();
  m.baseColorR = 0.0f;
  m.baseColorG = 0.0f;
  m.baseColorB = 0.0f;
  m.baseColorA = 0.0f;
  m.distortion = 1.0f;
  return m;
}

} // namespace MaterialPresets

} // namespace NoMoreDay::render
