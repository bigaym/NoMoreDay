#pragma once

#include <cstdint>
#include <string>

namespace NoMoreDay::render {

class ParticleTextureManager {
public:
  static ParticleTextureManager &Get();

  void Init(int maxLayers = 64, int layerSize = 128);
  void Shutdown();

  int LoadLayer(const std::string &path);

  void Bind(uint32_t textureUnit) const;
  void Unbind(uint32_t textureUnit) const;

  [[nodiscard]] bool IsInitialized() const { return m_initialized; }
  [[nodiscard]] int GetLayerCount() const { return m_loadedLayers; }

private:
  unsigned int m_textureArrayId = 0;
  int m_maxLayers = 64;
  int m_layerSize = 128;
  int m_loadedLayers = 0;
  bool m_initialized = false;
};

} // namespace NoMoreDay::render
