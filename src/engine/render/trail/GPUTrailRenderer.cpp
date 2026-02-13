#include "engine/render/trail/GPUTrailRenderer.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"

#include <algorithm>
#include <cstring>

namespace NoMoreDay::render {

GPUTrailRenderer &GPUTrailRenderer::Get() {
  static GPUTrailRenderer instance;
  return instance;
}

void GPUTrailRenderer::Init(int maxTrails, int maxPointsPerTrail) {
  if (m_initialized) {
    return;
  }

  m_maxTrails = std::clamp(maxTrails, 1, NoMoreDay::Constants::GPU::MAX_TRAILS);
  m_maxPointsPerTrail =
      std::clamp(maxPointsPerTrail, 2,
                 NoMoreDay::Constants::GPU::MAX_TRAIL_POINTS_PER_TRAIL);

  m_slots.clear();
  m_slots.resize(static_cast<size_t>(m_maxTrails));
  for (auto &slot : m_slots) {
    slot.points.resize(static_cast<size_t>(m_maxPointsPerTrail));
    slot.header.maxPoints = m_maxPointsPerTrail;
  }

  m_headerSSBO.Create(static_cast<size_t>(m_maxTrails) *
                      sizeof(components::GPUTrailHeader));
  m_pointsSSBO.Create(static_cast<size_t>(m_maxTrails) *
                      static_cast<size_t>(m_maxPointsPerTrail) *
                      sizeof(components::GPUTrailPoint));

  m_trailShader =
      LoadShader("assets/shaders/trail/trail.vert", "assets/shaders/trail/trail.frag");
  if (m_trailShader.id == 0) {
    LOG_ERROR("GPUTrailRenderer: Failed to load trail shader.");
    return;
  }

  m_mvpLoc = GetShaderLocation(m_trailShader, "mvp");
  m_trailIndexLoc = GetShaderLocation(m_trailShader, "trailIndex");
  m_maxPointsPerTrailLoc = GetShaderLocation(m_trailShader, "uMaxPointsPerTrail");

  m_dummyVAO = rlLoadVertexArray();
  m_initialized = true;

  LOG_INFO("GPUTrailRenderer: Initialized (maxTrails={}, maxPointsPerTrail={})",
           m_maxTrails, m_maxPointsPerTrail);
}

void GPUTrailRenderer::Shutdown() {
  if (!m_initialized) {
    return;
  }

  if (m_trailShader.id != 0) {
    UnloadShader(m_trailShader);
    m_trailShader = {0};
  }
  if (m_dummyVAO != 0) {
    rlUnloadVertexArray(m_dummyVAO);
    m_dummyVAO = 0;
  }

  m_headerSSBO.Release();
  m_pointsSSBO.Release();
  m_slots.clear();
  m_initialized = false;
}

int GPUTrailRenderer::AllocateTrail(const components::GPUTrailHeader &config) {
  if (!m_initialized) {
    return -1;
  }

  for (int i = 0; i < m_maxTrails; ++i) {
    TrailSlot &slot = m_slots[static_cast<size_t>(i)];
    if (slot.active) {
      continue;
    }

    slot.active = true;
    slot.header = config;
    slot.header.headIndex = 0;
    slot.header.pointCount = 0;
    slot.header.maxPoints = std::clamp(config.maxPoints, 2, m_maxPointsPerTrail);
    slot.header.maxLifetime = std::max(0.01f, config.maxLifetime);
    slot.header.widthStart = std::max(0.1f, config.widthStart);
    slot.header.widthEnd = std::max(0.0f, config.widthEnd);
    std::fill(slot.points.begin(), slot.points.end(), components::GPUTrailPoint{});
    return i;
  }

  return -1;
}

void GPUTrailRenderer::FreeTrail(int trailId) {
  if (!IsTrailIdValid(trailId)) {
    return;
  }

  TrailSlot &slot = m_slots[static_cast<size_t>(trailId)];
  if (!slot.active) {
    return;
  }

  slot.active = false;
  slot.header = {};
  slot.header.maxPoints = m_maxPointsPerTrail;
  std::fill(slot.points.begin(), slot.points.end(), components::GPUTrailPoint{});
}

void GPUTrailRenderer::AppendPoint(int trailId, Vector2 position, Vector2 direction,
                                   float width, uint32_t color) {
  if (!IsTrailIdValid(trailId)) {
    return;
  }

  TrailSlot &slot = m_slots[static_cast<size_t>(trailId)];
  if (!slot.active || slot.header.maxPoints <= 0) {
    return;
  }

  if (Vector2LengthSqr(direction) < 1e-6f) {
    direction = {1.0f, 0.0f};
  } else {
    direction = Vector2Normalize(direction);
  }

  int writeIndex = 0;
  if (slot.header.pointCount <= 0) {
    writeIndex = 0;
    slot.header.headIndex = 0;
    slot.header.pointCount = 1;
  } else {
    writeIndex = (slot.header.headIndex + 1) % slot.header.maxPoints;
    slot.header.headIndex = writeIndex;
    if (slot.header.pointCount < slot.header.maxPoints) {
      ++slot.header.pointCount;
    }
  }

  components::GPUTrailPoint point = {};
  point.posX = position.x;
  point.posY = position.y;
  point.dirX = direction.x;
  point.dirY = direction.y;
  point.width = (width > 0.0f) ? width : slot.header.widthStart;
  point.lifetime = slot.header.maxLifetime;
  point.colorPacked = color;
  point.flags = 0;
  slot.points[static_cast<size_t>(writeIndex)] = point;
}

void GPUTrailRenderer::Update(float dt) {
  if (!m_initialized || dt <= 0.0f) {
    return;
  }

  for (TrailSlot &slot : m_slots) {
    if (!slot.active || slot.header.pointCount <= 0) {
      continue;
    }

    for (int i = 0; i < slot.header.pointCount; ++i) {
      const int idx =
          (slot.header.headIndex - i + slot.header.maxPoints) % slot.header.maxPoints;
      auto &point = slot.points[static_cast<size_t>(idx)];
      point.lifetime -= dt;
    }

    while (slot.header.pointCount > 0) {
      const int tailIdx =
          (slot.header.headIndex - (slot.header.pointCount - 1) +
           slot.header.maxPoints) %
          slot.header.maxPoints;
      if (slot.points[static_cast<size_t>(tailIdx)].lifetime > 0.0f) {
        break;
      }
      --slot.header.pointCount;
    }

    if (slot.header.pointCount <= 0) {
      slot.header.headIndex = 0;
      slot.header.pointCount = 0;
    }
  }
}

void GPUTrailRenderer::Render(const Camera2D &camera) {
  if (!m_initialized || m_trailShader.id == 0) {
    return;
  }

  std::vector<components::GPUTrailHeader> headers(static_cast<size_t>(m_maxTrails));
  std::vector<components::GPUTrailPoint> points(
      static_cast<size_t>(m_maxTrails) * static_cast<size_t>(m_maxPointsPerTrail));

  for (int i = 0; i < m_maxTrails; ++i) {
    const TrailSlot &slot = m_slots[static_cast<size_t>(i)];
    auto &header = headers[static_cast<size_t>(i)];
    if (slot.active) {
      header = slot.header;
    } else {
      header = {};
      header.maxPoints = m_maxPointsPerTrail;
    }

    const size_t dstOffset =
        static_cast<size_t>(i) * static_cast<size_t>(m_maxPointsPerTrail);
    if (slot.active) {
      std::memcpy(points.data() + dstOffset, slot.points.data(),
                  static_cast<size_t>(m_maxPointsPerTrail) *
                      sizeof(components::GPUTrailPoint));
    }
  }

  m_headerSSBO.Update(headers.data(), headers.size() * sizeof(components::GPUTrailHeader));
  m_pointsSSBO.Update(points.data(), points.size() * sizeof(components::GPUTrailPoint));

  m_headerSSBO.BindBase(RenderConstants::TrailBinding::HEADERS);
  m_pointsSSBO.BindBase(RenderConstants::TrailBinding::POINTS);

  Matrix mvp = systems::GPUParticleSystem::Get().BuildMVP(camera);

  BeginShaderMode(m_trailShader);
  SetShaderValueMatrix(m_trailShader, m_mvpLoc, mvp);
  if (m_maxPointsPerTrailLoc >= 0) {
    SetShaderValue(m_trailShader, m_maxPointsPerTrailLoc, &m_maxPointsPerTrail,
                   SHADER_UNIFORM_INT);
  }

  rlEnableVertexArray(m_dummyVAO);
  rlDisableDepthTest();
  rlDisableBackfaceCulling();

  for (int pass = 0; pass < 2; ++pass) {
    BeginBlendMode(pass == 1 ? BLEND_ADDITIVE : BLEND_ALPHA);
    for (int i = 0; i < m_maxTrails; ++i) {
      const auto &slot = m_slots[static_cast<size_t>(i)];
      if (!slot.active || slot.header.pointCount < 2) {
        continue;
      }

      const auto &headPoint =
          slot.points[static_cast<size_t>(slot.header.headIndex)];
      const uint32_t blendMode = (headPoint.flags >> 8u) & 0x3u;
      if (static_cast<int>(blendMode) != pass) {
        continue;
      }

      if (m_trailIndexLoc >= 0) {
        SetShaderValue(m_trailShader, m_trailIndexLoc, &i, SHADER_UNIFORM_INT);
      }
      const int vertexCount = slot.header.pointCount * 2;
      utils::GPUUtils::DrawArrays(RenderConstants::GL::TRIANGLE_STRIP, 0,
                                  vertexCount);
    }
    EndBlendMode();
  }

  rlDisableVertexArray();
  EndShaderMode();
}

void GPUTrailRenderer::ClearAll() {
  for (auto &slot : m_slots) {
    slot.active = false;
    slot.header = {};
    slot.header.maxPoints = m_maxPointsPerTrail;
    std::fill(slot.points.begin(), slot.points.end(), components::GPUTrailPoint{});
  }
}

int GPUTrailRenderer::GetActiveTrailCount() const {
  return static_cast<int>(
      std::count_if(m_slots.begin(), m_slots.end(),
                    [](const TrailSlot &slot) { return slot.active; }));
}

bool GPUTrailRenderer::IsTrailIdValid(int trailId) const {
  return trailId >= 0 && trailId < m_maxTrails;
}

} // namespace NoMoreDay::render
