#pragma once

#include "raylib.h"

#include <cstdint>
#include <span>
#include <vector>

namespace NoMoreDay::render::lighting {

class GlobalHeightField final {
public:
  /**
   * @brief Pure projection DTO for one height-field rasterization stamp.
   *
   * Produced by the Game adapter (HeightFieldAdapter) from ECS components and
   * consumed by the Engine's texel rasterization. `Kind::Tile` stamps the full
   * tile rect [tileX,tileY]-[tileX+1,tileY+1] in tile space; `Kind::Disc`
   * stamps an ellipse around (worldX, worldY) with worldRadius.
   */
  struct HeightStamp {
    enum class Kind : uint8_t { Tile = 0, Disc = 1 };

    Kind kind = Kind::Disc;
    int tileX = 0;
    int tileY = 0;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldRadius = 0.0f;
    float height = 0.0f;
    bool dynamic = false;
  };

  struct Config {
    int textureWidth = 1024;
    int textureHeight = 1024;
    int chunkSize = 64;
    float worldOriginX = 0.0f;
    float worldOriginY = 0.0f;
    float worldWidth = 5000.0f;
    float worldHeight = 5000.0f;
    // World-space size of one terrain tile (previously the game GRID_TILE_SIZE).
    float tileWorldSize = 10.0f;
  };

  struct Stats {
    uint32_t dirtyChunkCount = 0u;
    uint32_t uploadedChunkCount = 0u;
    uint32_t dynamicChunkCount = 0u;
    bool didFullRebuild = false;
  };

  bool Initialize(const Config &config);
  void Shutdown();

  void RequestFullRebuild() { m_pendingFullRebuild = true; }
  void Update(std::span<const HeightStamp> stamps);

  [[nodiscard]] uint32_t GetTextureId() const { return m_texture.id; }
  [[nodiscard]] const Config &GetConfig() const { return m_config; }
  [[nodiscard]] const Stats &GetLastStats() const { return m_lastStats; }

  [[nodiscard]] float SampleNormalizedHeight(float worldX, float worldY) const;

private:
  struct ChunkCoord {
    int32_t x = 0;
    int32_t y = 0;

    [[nodiscard]] bool operator==(const ChunkCoord &rhs) const noexcept {
      return x == rhs.x && y == rhs.y;
    }
  };

  struct ChunkCoordHash {
    [[nodiscard]] std::size_t operator()(
        const ChunkCoord &coord) const noexcept;
  };

  [[nodiscard]] bool EnsureTexture();
  void BuildTerrainAndStatic(std::span<const HeightStamp> stamps);
  void ClearDynamicLayerForPreviousChunks();
  void BuildDynamicLayer(std::span<const HeightStamp> stamps);
  void ComposeDirtyChunks();
  void UploadDirtyChunks();

  [[nodiscard]] bool WorldToTexel(float worldX, float worldY, int &tx, int &ty) const;
  [[nodiscard]] ChunkCoord TexelToChunk(int tx, int ty) const;
  [[nodiscard]] uint32_t FlattenChunk(const ChunkCoord &coord) const;
  void MarkChunkDirty(const ChunkCoord &coord);
  void StampDiscMax(std::vector<uint16_t> &layer, float worldX, float worldY,
                    float worldRadius, float normalizedHeight,
                    bool trackDynamicChunk);
  void StampTileRectMax(std::vector<uint16_t> &layer, int tileX, int tileY,
                        float normalizedHeight);
  void UploadChunkRect(const ChunkCoord &coord);

  Config m_config = {};
  Stats m_lastStats = {};
  bool m_initialized = false;
  bool m_pendingFullRebuild = true;

  Texture2D m_texture = {};
  std::vector<uint16_t> m_baseLayer;
  std::vector<uint16_t> m_dynamicLayer;
  std::vector<uint16_t> m_compositedLayer;

  std::vector<uint8_t> m_dirtyChunks;
  std::vector<uint8_t> m_dynamicChunkMarks;
  std::vector<uint32_t> m_prevDynamicChunks;
  std::vector<uint32_t> m_currDynamicChunks;

  std::vector<Color> m_uploadScratch;
};

} // namespace NoMoreDay::render::lighting
