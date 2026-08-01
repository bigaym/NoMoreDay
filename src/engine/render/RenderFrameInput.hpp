#pragma once

// Frame-scoped render inputs injected by the caller (the Game layer).
// Zero app/game dependencies: pointers only, forward-declared.
class ResourceManager;

namespace NoMoreDay {
struct RenderContext;
}

namespace NoMoreDay::render {

struct RenderFrameInput {
  ResourceManager *resources = nullptr;
  float renderAlpha = 0.0f;
  NoMoreDay::RenderContext *renderContext = nullptr;
  float cameraZoom = 1.0f;
};

} // namespace NoMoreDay::render
