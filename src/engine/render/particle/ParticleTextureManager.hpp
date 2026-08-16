#pragma once

#include <cstdint>
#include <string>

namespace NoMoreDay::render {

class ParticleTextureManager {
public:
  static ParticleTextureManager &Get();

  void Init(int maxLayers = 64, int layerSize = 128);
  void Shutdown();

  // isLinear: RESERVED/EXPERIMENTAL (T8.1 metadata only). The flag is recorded
  // but decode is gated by the global uLinearPipeline uniform; per-layer shader
  // plumbing is not wired yet, so the value does not change decoding.
  int LoadLayer(const std::string &path, bool isLinear = false);

  void Bind(uint32_t textureUnit) const;
  void Unbind(uint32_t textureUnit) const;

  [[nodiscard]] bool IsInitialized() const { return m_initialized; }
  [[nodiscard]] int GetLayerCount() const { return m_loadedLayers; }
  [[nodiscard]] bool IsLayerLinear(int layer) const;

private:
  unsigned int m_textureArrayId = 0;
  int m_maxLayers = 64;
  int m_layerSize = 128;
  int m_loadedLayers = 0;
  bool m_initialized = false;
  std::vector<bool> m_layerLinear;
};

} // namespace NoMoreDay::render
