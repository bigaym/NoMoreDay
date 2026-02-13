#include "engine/render/particle/ParticleTextureManager.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"

#include <algorithm>

namespace NoMoreDay::render {
namespace {
constexpr int kGLClampToEdge = 0x812F;
}

ParticleTextureManager &ParticleTextureManager::Get() {
  static ParticleTextureManager manager;
  return manager;
}

void ParticleTextureManager::Init(int maxLayers, int layerSize) {
  if (m_initialized) {
    return;
  }
  if (!utils::GPUUtils::IsInitialized()) {
    LOG_WARN("ParticleTextureManager: GPUUtils is not initialized.");
    return;
  }

  m_maxLayers = std::max(1, maxLayers);
  m_layerSize = std::max(1, layerSize);
  m_loadedLayers = 0;

  utils::GPUUtils::GenTextures(1, &m_textureArrayId);
  if (m_textureArrayId == 0) {
    LOG_ERROR("ParticleTextureManager: Failed to create texture array.");
    return;
  }

  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY,
                               m_textureArrayId);
  utils::GPUUtils::TexStorage3D(RenderConstants::GL::TEXTURE_2D_ARRAY, 1,
                                GL_RGBA8, m_layerSize, m_layerSize,
                                m_maxLayers);
  utils::GPUUtils::TexParameteri(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                 GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  utils::GPUUtils::TexParameteri(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                 GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  utils::GPUUtils::TexParameteri(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                 GL_TEXTURE_WRAP_S, kGLClampToEdge);
  utils::GPUUtils::TexParameteri(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                 GL_TEXTURE_WRAP_T, kGLClampToEdge);
  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY, 0);

  m_initialized = true;
  LOG_INFO(
      "ParticleTextureManager: Initialized Texture2DArray id={} layers={} size={}x{}",
      m_textureArrayId, m_maxLayers, m_layerSize, m_layerSize);
}

void ParticleTextureManager::Shutdown() {
  if (m_textureArrayId != 0 && utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::DeleteTextures(1, &m_textureArrayId);
  }
  m_textureArrayId = 0;
  m_loadedLayers = 0;
  m_initialized = false;
}

int ParticleTextureManager::LoadLayer(const std::string &path) {
  if (!m_initialized || m_textureArrayId == 0) {
    LOG_WARN("ParticleTextureManager: LoadLayer called before Init.");
    return -1;
  }
  if (m_loadedLayers >= m_maxLayers) {
    LOG_WARN("ParticleTextureManager: Layer capacity exceeded (max={}).",
             m_maxLayers);
    return -1;
  }
  if (!FileExists(path.c_str())) {
    LOG_WARN("ParticleTextureManager: Texture not found: {}", path);
    return -1;
  }

  Image image = LoadImage(path.c_str());
  if (image.data == nullptr) {
    LOG_WARN("ParticleTextureManager: Failed to load image: {}", path);
    return -1;
  }

  if (image.width != m_layerSize || image.height != m_layerSize) {
    ImageResize(&image, m_layerSize, m_layerSize);
  }
  ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

  const int layer = m_loadedLayers;
  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY,
                               m_textureArrayId);
  utils::GPUUtils::TexSubImage3D(RenderConstants::GL::TEXTURE_2D_ARRAY, 0, 0, 0,
                                 layer, m_layerSize, m_layerSize, 1, GL_RGBA,
                                 GL_UNSIGNED_BYTE, image.data);
  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY, 0);
  UnloadImage(image);

  ++m_loadedLayers;
  LOG_INFO("ParticleTextureManager: Loaded layer {} from {}", layer, path);
  return layer;
}

void ParticleTextureManager::Bind(uint32_t textureUnit) const {
  if (!m_initialized || m_textureArrayId == 0) {
    return;
  }
  utils::GPUUtils::ActiveTexture(RenderConstants::GL::TEXTURE0 + textureUnit);
  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY,
                               m_textureArrayId);
}

void ParticleTextureManager::Unbind(uint32_t textureUnit) const {
  utils::GPUUtils::ActiveTexture(RenderConstants::GL::TEXTURE0 + textureUnit);
  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY, 0);
}

} // namespace NoMoreDay::render
