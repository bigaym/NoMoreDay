#include "game/application/render/AirWallRenderer.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/systems/world/WorldConstants.hpp"
#include <utility>

namespace NoMoreDay {

AirWallRenderer::AirWallRenderer(AirWallRenderer &&other) noexcept {
  MoveFrom(other);
}

AirWallRenderer &AirWallRenderer::operator=(AirWallRenderer &&other) noexcept {
  if (this != &other) {
    Shutdown();
    MoveFrom(other);
  }
  return *this;
}

bool AirWallRenderer::Initialize(const std::string &backgroundShaderPath) {
  if (backgroundShaderPath.empty()) {
    Shutdown();
    return false;
  }

  if (m_backgroundShader.id != 0 && m_shaderPath == backgroundShaderPath) {
    return true;
  }

  Shutdown();
  m_backgroundShader = LoadShader(nullptr, backgroundShaderPath.c_str());
  if (m_backgroundShader.id == 0) {
    m_shaderPath.clear();
    return false;
  }

  m_shaderPath = backgroundShaderPath;
  m_locTime = GetShaderLocation(m_backgroundShader, "time");
  m_locCameraOffset = GetShaderLocation(m_backgroundShader, "cameraOffset");
  m_locZoom = GetShaderLocation(m_backgroundShader, "zoom");
  m_locScreenSize = GetShaderLocation(m_backgroundShader, "screenSize");
  return true;
}

void AirWallRenderer::Shutdown() {
  if (m_backgroundShader.id != 0) {
    UnloadShader(m_backgroundShader);
  }
  m_backgroundShader = {0};
  m_shaderPath.clear();
  m_locTime = -1;
  m_locCameraOffset = -1;
  m_locZoom = -1;
  m_locScreenSize = -1;
}

void AirWallRenderer::RenderBackground(const Camera2D &camera,
                                       const std::vector<Tile> &grid, int width,
                                       int height, float time) const {
  if (m_backgroundShader.id == 0 || width <= 0 || height <= 0) {
    return;
  }
  if (!HasAnyAirWall(grid)) {
    return;
  }

  const Vector2 cameraOffset = camera.target;
  const float zoom = camera.zoom;
  const float screenSize[2] = {static_cast<float>(GetScreenWidth()),
                               static_cast<float>(GetScreenHeight())};

  BeginShaderMode(m_backgroundShader);
  if (m_locTime >= 0) {
    SetShaderValue(m_backgroundShader, m_locTime, &time, SHADER_UNIFORM_FLOAT);
  }
  if (m_locCameraOffset >= 0) {
    SetShaderValue(m_backgroundShader, m_locCameraOffset, &cameraOffset,
                   SHADER_UNIFORM_VEC2);
  }
  if (m_locZoom >= 0) {
    SetShaderValue(m_backgroundShader, m_locZoom, &zoom, SHADER_UNIFORM_FLOAT);
  }
  if (m_locScreenSize >= 0) {
    SetShaderValue(m_backgroundShader, m_locScreenSize, screenSize,
                   SHADER_UNIFORM_VEC2);
  }

  const int mapPixelW =
      static_cast<int>(width * NoMoreDay::Constants::World::GRID_TILE_SIZE);
  const int mapPixelH =
      static_cast<int>(height * NoMoreDay::Constants::World::GRID_TILE_SIZE);
  DrawRectangle(0, 0, mapPixelW, mapPixelH, WHITE);
  EndShaderMode();
}

bool AirWallRenderer::HasAnyAirWall(const std::vector<Tile> &grid) const noexcept {
  for (const Tile &tile : grid) {
    if (tile.isAirWall) {
      return true;
    }
  }
  return false;
}

void AirWallRenderer::MoveFrom(AirWallRenderer &other) noexcept {
  m_backgroundShader = other.m_backgroundShader;
  m_shaderPath = std::move(other.m_shaderPath);
  m_locTime = other.m_locTime;
  m_locCameraOffset = other.m_locCameraOffset;
  m_locZoom = other.m_locZoom;
  m_locScreenSize = other.m_locScreenSize;

  other.m_backgroundShader = {0};
  other.m_locTime = -1;
  other.m_locCameraOffset = -1;
  other.m_locZoom = -1;
  other.m_locScreenSize = -1;
}

} // namespace NoMoreDay

