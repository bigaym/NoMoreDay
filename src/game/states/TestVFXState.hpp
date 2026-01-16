#pragma once

#include "engine/scene/State.hpp"
#include "raylib.h"
#include <entt/entt.hpp>

namespace NoMoreDay {

class TestVFXState : public IState {
public:
  using IState::IState;

  void OnEnter() override;
  void OnExit() override;
  bool OnUpdate(float dt) override;
  void OnRender() override;

  bool IsTransparent() const override { return false; }

private:
  Camera2D m_camera = {0};
  Shader m_trailShader;
  Shader m_holoShader;
  Shader m_distortionShader;

  Texture2D m_noiseTex;
  Texture2D m_trailMask;
  Texture2D m_distortionNormal;
  Texture2D m_baseSword;

  RenderTexture2D m_screenCapture;

  float m_time = 0.0f;
};

} // namespace NoMoreDay
