#include "engine/render/resource/TextureArrayManager.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::render {
namespace {

constexpr uint32_t kGLRgba8 = 0x8058;
constexpr uint32_t kGLRgba = 0x1908;
constexpr uint32_t kGLUnsignedByte = 0x1401;
constexpr uint32_t kGLTextureMinFilter = 0x2801;
constexpr uint32_t kGLTextureMagFilter = 0x2800;
constexpr uint32_t kGLTextureWrapS = 0x2802;
constexpr uint32_t kGLTextureWrapT = 0x2803;
constexpr uint32_t kGLLinear = 0x2601;
constexpr uint32_t kGLClampToEdge = 0x812F;

constexpr std::array<const char *, 4> kDefaultTexturePaths = {
    "assets/textures/defaults/albedo_white.png",
    "assets/textures/defaults/normal_flat.png",
    "assets/textures/defaults/mask_neutral.png",
    "assets/textures/defaults/detail_flat.png"};

constexpr std::array<std::array<unsigned char, 4>, 4> kDefaultPixels = {
    std::array<unsigned char, 4>{255, 255, 255, 255},
    std::array<unsigned char, 4>{128, 128, 255, 255},
    std::array<unsigned char, 4>{153, 0, 255, 0},
    std::array<unsigned char, 4>{128, 128, 255, 255}};

} // namespace

TextureArrayManager &TextureArrayManager::Get() {
  static TextureArrayManager manager;
  return manager;
}

size_t TextureArrayManager::SemanticIndex(TextureArraySemantic semantic) {
  return static_cast<size_t>(semantic);
}

TextureArrayManager::ArrayState *
TextureArrayManager::GetState(TextureArraySemantic semantic) {
  return &m_states[SemanticIndex(semantic)];
}

const TextureArrayManager::ArrayState *
TextureArrayManager::GetState(TextureArraySemantic semantic) const {
  return &m_states[SemanticIndex(semantic)];
}

void TextureArrayManager::Initialize(int maxLayers, int layerSize) {
  if (m_initialized) {
    return;
  }

  m_maxLayers = std::max(2, maxLayers);
  m_layerSize = std::max(1, layerSize);

  for (size_t i = 0; i < m_states.size(); ++i) {
    if (!BuildState(m_states[i], static_cast<TextureArraySemantic>(i), m_maxLayers,
                    m_layerSize)) {
      LOG_ERROR("TextureArrayManager: failed to build semantic {}", i);
      Shutdown();
      return;
    }
  }

  m_initialized = true;
  LOG_INFO("TextureArrayManager: initialized layers={} size={}x{}", m_maxLayers,
           m_layerSize, m_layerSize);
}

void TextureArrayManager::Shutdown() {
  for (auto &state : m_states) {
    DestroyState(state);
  }
  m_initialized = false;
}

bool TextureArrayManager::BuildState(ArrayState &state,
                                     TextureArraySemantic semantic, int maxLayers,
                                     int layerSize) {
  DestroyState(state);

  state.maxLayers = std::max(2, maxLayers);
  state.layerSize = std::max(1, layerSize);
  state.loadedLayers = 0;
  state.activeLayers = 0;
  state.defaultLayer = -1;
  state.freeLayers.clear();
  state.occupied.assign(static_cast<size_t>(state.maxLayers), false);
  state.sourcePaths.clear();
  state.pathToLayer.clear();

  if (utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::GenTextures(1, &state.textureId);
    if (state.textureId == 0) {
      LOG_ERROR("TextureArrayManager: failed to create texture array for semantic {}",
                static_cast<int>(semantic));
      return false;
    }
    utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                 state.textureId);
    utils::GPUUtils::TexStorage3D(RenderConstants::GL::TEXTURE_2D_ARRAY, 1,
                                  kGLRgba8, state.layerSize, state.layerSize,
                                  state.maxLayers);
    utils::GPUUtils::TexParameteri(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                   kGLTextureMinFilter, kGLLinear);
    utils::GPUUtils::TexParameteri(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                   kGLTextureMagFilter, kGLLinear);
    utils::GPUUtils::TexParameteri(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                   kGLTextureWrapS, kGLClampToEdge);
    utils::GPUUtils::TexParameteri(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                   kGLTextureWrapT, kGLClampToEdge);
    utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY, 0);
  }

  return EnsureDefaultLayer(state, semantic);
}

void TextureArrayManager::DestroyState(ArrayState &state) {
  if (state.textureId != 0 && utils::GPUUtils::IsInitialized()) {
    utils::GPUUtils::DeleteTextures(1, &state.textureId);
  }
  state = {};
}

bool TextureArrayManager::UploadDefaultPixel(ArrayState &state,
                                             TextureArraySemantic semantic,
                                             int layer) {
  if (!utils::GPUUtils::IsInitialized() || state.textureId == 0) {
    return true;
  }

  const auto &pixel = kDefaultPixels[SemanticIndex(semantic)];
  std::vector<unsigned char> image(static_cast<size_t>(state.layerSize) *
                                       static_cast<size_t>(state.layerSize) * 4u,
                                   0u);
  for (size_t i = 0; i < image.size(); i += 4) {
    image[i + 0] = pixel[0];
    image[i + 1] = pixel[1];
    image[i + 2] = pixel[2];
    image[i + 3] = pixel[3];
  }

  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY,
                               state.textureId);
  utils::GPUUtils::TexSubImage3D(
      RenderConstants::GL::TEXTURE_2D_ARRAY, 0, 0, 0, layer, state.layerSize,
      state.layerSize, 1, kGLRgba, kGLUnsignedByte, image.data());
  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY, 0);
  return true;
}

bool TextureArrayManager::EnsureDefaultLayer(ArrayState &state,
                                             TextureArraySemantic semantic) {
  const std::string defaultPath = kDefaultTexturePaths[SemanticIndex(semantic)];
  const int layer = 0;
  state.loadedLayers = 1;
  state.activeLayers = 1;
  state.defaultLayer = layer;
  state.occupied[static_cast<size_t>(layer)] = true;
  state.pathToLayer[defaultPath] = layer;
  state.sourcePaths.push_back(defaultPath);

  if (!utils::GPUUtils::IsInitialized() || state.textureId == 0) {
    return true;
  }

  bool uploaded = false;
  if (FileExists(defaultPath.c_str())) {
    Image image = LoadImage(defaultPath.c_str());
    if (image.data != nullptr) {
      if (image.width != state.layerSize || image.height != state.layerSize) {
        ImageResize(&image, state.layerSize, state.layerSize);
      }
      ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                   state.textureId);
      utils::GPUUtils::TexSubImage3D(
          RenderConstants::GL::TEXTURE_2D_ARRAY, 0, 0, 0, layer,
          state.layerSize, state.layerSize, 1, kGLRgba, kGLUnsignedByte,
          image.data);
      utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY, 0);
      UnloadImage(image);
      uploaded = true;
    }
  }

  if (!uploaded) {
    UploadDefaultPixel(state, semantic, layer);
    LOG_WARN("TextureArrayManager: default texture missing, generated fallback {}",
             defaultPath);
  }

  return true;
}

int TextureArrayManager::LoadLayerInternal(ArrayState &state, const std::string &path,
                                           bool allowMissing) {
  if (path.empty()) {
    return state.defaultLayer;
  }

  const auto existing = state.pathToLayer.find(path);
  if (existing != state.pathToLayer.end()) {
    return existing->second;
  }

  int layer = -1;
  if (!state.freeLayers.empty()) {
    layer = state.freeLayers.back();
    state.freeLayers.pop_back();
  } else if (state.loadedLayers < state.maxLayers) {
    layer = state.loadedLayers++;
  } else {
    LOG_WARN("TextureArrayManager: layer capacity exceeded (max={})",
             state.maxLayers);
    return state.defaultLayer;
  }

  if (!FileExists(path.c_str())) {
    if (allowMissing) {
      state.freeLayers.push_back(layer);
      return state.defaultLayer;
    }
    LOG_ERROR("TextureArrayManager: texture not found {}", path);
    state.freeLayers.push_back(layer);
    return -1;
  }

  if (utils::GPUUtils::IsInitialized() && state.textureId != 0) {
    Image image = LoadImage(path.c_str());
    if (image.data == nullptr) {
      LOG_ERROR("TextureArrayManager: failed to load image {}", path);
      state.freeLayers.push_back(layer);
      return -1;
    }
    if (image.width != state.layerSize || image.height != state.layerSize) {
      ImageResize(&image, state.layerSize, state.layerSize);
    }
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

    utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY,
                                 state.textureId);
    utils::GPUUtils::TexSubImage3D(
        RenderConstants::GL::TEXTURE_2D_ARRAY, 0, 0, 0, layer, state.layerSize,
        state.layerSize, 1, kGLRgba, kGLUnsignedByte, image.data);
    utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY, 0);
    UnloadImage(image);
  }

  state.occupied[static_cast<size_t>(layer)] = true;
  ++state.activeLayers;
  state.pathToLayer[path] = layer;
  state.sourcePaths.push_back(path);
  return layer;
}

int TextureArrayManager::LoadLayer(TextureArraySemantic semantic,
                                   const std::string &path) {
  if (!m_initialized) {
    Initialize(m_maxLayers, m_layerSize);
  }
  ArrayState *state = GetState(semantic);
  const int layer = LoadLayerInternal(*state, path, false);
  if (layer < 0) {
    return state->defaultLayer;
  }
  return layer;
}

void TextureArrayManager::ReleaseLayer(TextureArraySemantic semantic, int layer) {
  if (!m_initialized) {
    return;
  }
  ArrayState *state = GetState(semantic);
  if (layer < 0 || layer >= state->maxLayers || layer == state->defaultLayer) {
    return;
  }
  if (!state->occupied[static_cast<size_t>(layer)]) {
    return;
  }

  state->occupied[static_cast<size_t>(layer)] = false;
  state->freeLayers.push_back(layer);
  state->activeLayers = std::max(0, state->activeLayers - 1);

  for (auto it = state->pathToLayer.begin(); it != state->pathToLayer.end();) {
    if (it->second == layer) {
      const std::string removedPath = it->first;
      it = state->pathToLayer.erase(it);
      state->sourcePaths.erase(std::remove(state->sourcePaths.begin(),
                                           state->sourcePaths.end(), removedPath),
                               state->sourcePaths.end());
    } else {
      ++it;
    }
  }
}

int TextureArrayManager::ResolveLayerOrDefault(TextureArraySemantic semantic,
                                               int requestedLayer) const {
  if (!m_initialized) {
    return 0;
  }
  const ArrayState *state = GetState(semantic);
  if (requestedLayer < 0 || requestedLayer >= state->maxLayers) {
    return state->defaultLayer;
  }
  if (!state->occupied[static_cast<size_t>(requestedLayer)]) {
    return state->defaultLayer;
  }
  return requestedLayer;
}

int TextureArrayManager::GetDefaultLayer(TextureArraySemantic semantic) const {
  const ArrayState *state = GetState(semantic);
  return state->defaultLayer;
}

int TextureArrayManager::GetLayerCount(TextureArraySemantic semantic) const {
  const ArrayState *state = GetState(semantic);
  return state->activeLayers;
}

unsigned int TextureArrayManager::GetTextureId(TextureArraySemantic semantic) const {
  const ArrayState *state = GetState(semantic);
  return state->textureId;
}

void TextureArrayManager::Bind(TextureArraySemantic semantic,
                               uint32_t textureUnit) const {
  const ArrayState *state = GetState(semantic);
  if (state->textureId == 0) {
    return;
  }
  utils::GPUUtils::ActiveTexture(RenderConstants::GL::TEXTURE0 + textureUnit);
  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY,
                               state->textureId);
}

void TextureArrayManager::Unbind(uint32_t textureUnit) const {
  utils::GPUUtils::ActiveTexture(RenderConstants::GL::TEXTURE0 + textureUnit);
  utils::GPUUtils::BindTexture(RenderConstants::GL::TEXTURE_2D_ARRAY, 0);
}

bool TextureArrayManager::HotReloadLayers(TextureArraySemantic semantic,
                                          const std::vector<std::string> &paths) {
  if (!m_initialized) {
    Initialize(m_maxLayers, m_layerSize);
  }

  ArrayState staging = {};
  if (!BuildState(staging, semantic, m_maxLayers, m_layerSize)) {
    return false;
  }

  for (const std::string &path : paths) {
    if (path.empty()) {
      continue;
    }
    if (LoadLayerInternal(staging, path, false) < 0) {
      DestroyState(staging);
      LOG_WARN("TextureArrayManager: hot reload rejected path={}", path);
      return false;
    }
  }

  ArrayState *active = GetState(semantic);
  ArrayState old = std::move(*active);
  *active = std::move(staging);
  DestroyState(old);
  return true;
}

bool TextureArrayManager::RebuildForResize(int width, int height) {
  if (!m_initialized) {
    return false;
  }
  if (width <= 0 || height <= 0) {
    return false;
  }

  bool success = true;
  for (size_t i = 0; i < m_states.size(); ++i) {
    const TextureArraySemantic semantic = static_cast<TextureArraySemantic>(i);
    ArrayState staging = {};
    if (!BuildState(staging, semantic, m_maxLayers, m_layerSize)) {
      success = false;
      continue;
    }

    const auto previousPaths = m_states[i].sourcePaths;
    for (const std::string &path : previousPaths) {
      const std::string defaultPath = kDefaultTexturePaths[i];
      if (path == defaultPath || path.empty()) {
        continue;
      }
      (void)LoadLayerInternal(staging, path, true);
    }

    ArrayState old = std::move(m_states[i]);
    m_states[i] = std::move(staging);
    DestroyState(old);
  }

  if (success) {
    LOG_INFO("TextureArrayManager: rebuilt after resize {}x{}", width, height);
  }
  return success;
}

} // namespace NoMoreDay::render
