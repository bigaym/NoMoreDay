#include "game/systems/vfx/TrailSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/vfx/MotionTrailComponent.hpp"
#include "raymath.h"
#include "rlgl.h"

namespace NoMoreDay::systems {

void TrailSystem::Update(entt::registry &registry, float dt) {
  auto view = registry.view<components::MotionTrail, Position>();

  for (auto entity : view) {
    auto &trail = view.get<components::MotionTrail>(entity);
    const auto &pos = view.get<Position>(entity);

    // 1. Update existing points
    for (auto it = trail.points.begin(); it != trail.points.end();) {
      it->timeAlive += dt;
      if (it->timeAlive >= trail.lifetime) {
        it = trail.points.erase(it);
      } else {
        it->alpha = 1.0f - (it->timeAlive / trail.lifetime);
        ++it;
      }
    }

    // 2. Add new point if active and moved far enough
    if (trail.isActive) {
      bool shouldAdd = false;
      if (trail.points.empty()) {
        shouldAdd = true;
      } else {
        float dist =
            Vector2Distance(trail.points.back().position, {pos.x, pos.y});
        if (dist >= trail.minDistance) {
          shouldAdd = true;
        }
      }

      if (shouldAdd) {
        // Calculate angle based on movement if possible
        float angle = 0.0f;
        if (!trail.points.empty()) {
          Vector2 dir = Vector2Normalize(
              Vector2Subtract({pos.x, pos.y}, trail.points.back().position));
          angle = atan2f(dir.y, dir.x);
        } else {
          // Fallback angle if first point
          angle = 0.0f;
        }

        trail.points.push_back({{pos.x, pos.y}, 1.0f, 0.0f, angle});
      }
    }
  }
}

void TrailSystem::Render(entt::registry &registry, Shader trailShader) {
  auto view = registry.view<const components::MotionTrail>();

  for (auto entity : view) {
    const auto &trail = view.get<const components::MotionTrail>(entity);
    if (trail.points.size() < 2)
      continue;

    BeginShaderMode(trailShader);

    // We use RL_TRIANGLES because RL_TRIANGLE_STRIP might not be
    // defined/supported in all rlgl versions
    rlBegin(RL_TRIANGLES);
    for (size_t i = 0; i < trail.points.size() - 1; ++i) {
      const auto &pA = trail.points[i];
      const auto &pB = trail.points[i + 1];

      float progressA = 1.0f - (pA.timeAlive / trail.lifetime);
      float progressB = 1.0f - (pB.timeAlive / trail.lifetime);

      float alphaA = pA.alpha;
      float alphaB = pB.alpha;

      float widthA = trail.maxWidth * progressA;
      float widthB = trail.maxWidth * progressB;

      Vector2 dirA = {cosf(pA.angle), sinf(pA.angle)};
      Vector2 normalA = {-dirA.y, dirA.x};
      Vector2 dirB = {cosf(pB.angle), sinf(pB.angle)};
      Vector2 normalB = {-dirB.y, dirB.x};

      Vector2 vL_A = {pA.position.x + normalA.x * widthA * 0.5f,
                      pA.position.y + normalA.y * widthA * 0.5f};
      Vector2 vR_A = {pA.position.x - normalA.x * widthA * 0.5f,
                      pA.position.y - normalA.y * widthA * 0.5f};
      Vector2 vL_B = {pB.position.x + normalB.x * widthB * 0.5f,
                      pB.position.y + normalB.y * widthB * 0.5f};
      Vector2 vR_B = {pB.position.x - normalB.x * widthB * 0.5f,
                      pB.position.y - normalB.y * widthB * 0.5f};

      float texU_A = (float)i / (trail.points.size() - 1);
      float texU_B = (float)(i + 1) / (trail.points.size() - 1);

      // Triangle 1
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b,
                 (unsigned char)(255 * alphaA));
      rlTexCoord2f(texU_A, 0.0f);
      rlVertex2f(vL_A.x, vL_A.y);
      rlTexCoord2f(texU_A, 1.0f);
      rlVertex2f(vR_A.x, vR_A.y);
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b,
                 (unsigned char)(255 * alphaB));
      rlTexCoord2f(texU_B, 0.0f);
      rlVertex2f(vL_B.x, vL_B.y);

      // Triangle 2
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b,
                 (unsigned char)(255 * alphaA));
      rlTexCoord2f(texU_A, 1.0f);
      rlVertex2f(vR_A.x, vR_A.y);
      rlColor4ub(trail.color.r, trail.color.g, trail.color.b,
                 (unsigned char)(255 * alphaB));
      rlTexCoord2f(texU_B, 1.0f);
      rlVertex2f(vR_B.x, vR_B.y);
      rlTexCoord2f(texU_B, 0.0f);
      rlVertex2f(vL_B.x, vL_B.y);
    }
    rlEnd();

    EndShaderMode();
  }
}

} // namespace NoMoreDay::systems
