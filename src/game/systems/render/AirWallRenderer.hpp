#pragma once

#include "game/components/MapComponent.hpp"
#include "raylib.h"
#include <string>
#include <vector>

namespace NoMoreDay {

class AirWallRenderer {
public:
  AirWallRenderer() = default;
  ~AirWallRenderer() { Shutdown(); }

  AirWallRenderer(const AirWallRenderer &) = delete;
  AirWallRenderer &operator=(const AirWallRenderer &) = delete;

  AirWallRenderer(AirWallRenderer &&other) noexcept;
  AirWallRenderer &operator=(AirWallRenderer &&other) noexcept;

  bool Initialize(const std::string &backgroundShaderPath);
  void Shutdown();

  void RenderBackground(const Camera2D &camera, const std::vector<Tile> &grid,
                        int width, int height, float time) const;

  [[nodiscard]] bool IsReady() const noexcept { return m_backgroundShader.id != 0; }

private:
  [[nodiscard]] bool HasAnyAirWall(const std::vector<Tile> &grid) const noexcept;
  void MoveFrom(AirWallRenderer &other) noexcept;

  Shader m_backgroundShader = {0};
  std::string m_shaderPath;
  int m_locTime = -1;
  int m_locCameraOffset = -1;
  int m_locZoom = -1;
  int m_locScreenSize = -1;
};

} // namespace NoMoreDay

