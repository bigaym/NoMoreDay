#pragma once
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <vector>

namespace NoMoreDay::components {

struct MotionTrailPoint {
  Vector2 position;
  float alpha;
  float timeAlive;
  float angle;
};

struct MotionTrail {
  std::vector<MotionTrailPoint> points;
  float maxWidth = 10.0f;
  Color color = WHITE;
  float lifetime = 0.5f; // Total time a point stays alive
  float updateTimer = 0.0f;
  float minDistance = 5.0f; // Distance between points to record
  bool isActive = true;
  
  // Particle Trail Option
  bool useParticles = false;
  float emitInterval = 0.02f; // Frequent emission for smooth flow
  float emitTimer = 0.0f;
  float particleSize = 3.0f;
  Color coreColor = WHITE; // Bright center color

  // GPU Trail Path (Phase 3)
  int gpuTrailId = -1;
  bool useGPUTrail = false;
};


// JSON integration not strictly necessary for VFX-only components unless
// persisted, but good for consistency.
inline void to_json(nlohmann::json &j, const MotionTrail &t) {
  j = nlohmann::json{{"maxWidth", t.maxWidth},
                     {"lifetime", t.lifetime},
                     {"minDistance", t.minDistance}};
}

inline void from_json(const nlohmann::json &j, MotionTrail &t) {
  j.at("maxWidth").get_to(t.maxWidth);
  j.at("lifetime").get_to(t.lifetime);
  j.at("minDistance").get_to(t.minDistance);
}

} // namespace NoMoreDay::components
