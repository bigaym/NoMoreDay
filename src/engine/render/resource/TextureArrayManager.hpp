#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::render {

enum class TextureArraySemantic : uint8_t {
  Albedo = 0,
  Normal = 1,
  Mask = 2,
  Detail = 3,
  Roughness = Mask,
};

class TextureArrayManager {
public:
  static TextureArrayManager &Get();

  void Initialize(int maxLayers = 64, int layerSize = 128);
  void Shutdown();

  [[nodiscard]] bool IsInitialized() const { return m_initialized; }

  // Load an image layer and return the assigned layer index.
  int LoadLayer(TextureArraySemantic semantic, const std::string &path);
  // Per-layer linear override. RESERVED/EXPERIMENTAL (T8.1 metadata only):
  // the flag is recorded but decode is gated by the global uLinearPipeline
  // uniform; per-layer shader plumbing is not wired yet, so a non-default
  // value does not change decoding. Kept for the T8.1 API contract and tests.
  int LoadLayer(TextureArraySemantic semantic, const std::string &path,
                bool isLinear);
  void ReleaseLayer(TextureArraySemantic semantic, int layer);

  // Resolve invalid layer ids to semantic-specific defaults.
  [[nodiscard]] int ResolveLayerOrDefault(TextureArraySemantic semantic,
                                          int requestedLayer) const;
  [[nodiscard]] int GetDefaultLayer(TextureArraySemantic semantic) const;
  [[nodiscard]] int GetLayerCount(TextureArraySemantic semantic) const;
  [[nodiscard]] unsigned int GetTextureId(TextureArraySemantic semantic) const;

  // Linear color space metadata query (T8.1)
  [[nodiscard]] static bool GetDefaultLinearForSemantic(TextureArraySemantic semantic);
  // RESERVED/EXPERIMENTAL: metadata queries kept for the T8.1 API contract.
  // Not consumed by the shader pipeline yet — actual decode is gated by the
  // global uLinearPipeline uniform (see particle.frag / entity_mdi.frag).
  [[nodiscard]] bool IsSemanticLinear(TextureArraySemantic semantic) const;
  [[nodiscard]] bool IsLayerLinear(TextureArraySemantic semantic, int layer) const;

  void Bind(TextureArraySemantic semantic, uint32_t textureUnit) const;
  void Unbind(uint32_t textureUnit) const;

  // Safe rebuild path for resize/context-restore scenarios.
  bool RebuildForResize(int width, int height);

  // Atomic hot-reload flow: validate staging resources -> swap.
  bool HotReloadLayers(TextureArraySemantic semantic,
                       const std::vector<std::string> &paths);
  // linearFlags variant: RESERVED/EXPERIMENTAL, see LoadLayer(semantic,path,bool).
  bool HotReloadLayers(TextureArraySemantic semantic,
                       const std::vector<std::string> &paths,
                       const std::vector<bool> &linearFlags);

private:
  TextureArrayManager() = default;

  struct ArrayState {
    unsigned int textureId = 0;
    int maxLayers = 0;
    int layerSize = 0;
    int loadedLayers = 0;
    int activeLayers = 0;
    int defaultLayer = -1;
    bool defaultLinear = true;
    std::vector<int> freeLayers;
    std::vector<bool> occupied;
    std::vector<bool> layerLinear;
    std::vector<std::string> sourcePaths;
    std::unordered_map<std::string, int> pathToLayer;
  };
  [[nodiscard]] static size_t SemanticIndex(TextureArraySemantic semantic);

  bool BuildState(ArrayState &state, TextureArraySemantic semantic, int maxLayers,
                  int layerSize);
  void DestroyState(ArrayState &state);
  int LoadLayerInternal(ArrayState &state, const std::string &path,
                        bool allowMissing, bool isLinear);
  bool EnsureDefaultLayer(ArrayState &state, TextureArraySemantic semantic);
  bool UploadDefaultPixel(ArrayState &state, TextureArraySemantic semantic,
                          int layer);

  ArrayState *GetState(TextureArraySemantic semantic);
  const ArrayState *GetState(TextureArraySemantic semantic) const;

  bool m_initialized = false;
  int m_maxLayers = 64;
  int m_layerSize = 128;
  std::array<ArrayState, 4> m_states;
};

} // namespace NoMoreDay::render
