#include "doctest.h"

#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/passes/CompositePass.hpp"
#include "engine/render/passes/DistortionPass.hpp"
#include "engine/render/passes/PostProcessPass.hpp"
#include "engine/render/passes/ScenePass.hpp"
#include "engine/render/passes/UIWorldPass.hpp"
#include "engine/render/passes/VFXPass.hpp"

#include <memory>

TEST_CASE("[Integration] RenderGraph - Offscreen ownership path stays valid") {
  using namespace NoMoreDay::render;

  graph::RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::SceneHdrColor, graph::RenderOwnerTag::UIWorld));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
}

TEST_CASE(
    "[Integration] RenderGraph - Default framebuffer HDR ownership path stays valid") {
  using namespace NoMoreDay::render;

  graph::RenderGraph graph;
  graph.AddPass(std::make_shared<passes::ScenePass>());
  graph.AddPass(std::make_shared<passes::VFXPass>());
  graph.AddPass(std::make_shared<passes::UIWorldPass>());
  graph.AddPass(std::make_shared<passes::PostProcessPass>());
  graph.AddPass(std::make_shared<passes::DistortionPass>());
  graph.AddPass(std::make_shared<passes::CompositePass>(
      graph::RenderResourceTag::DistortionLdrColor,
      graph::RenderOwnerTag::Distortion));

  CHECK_NOTHROW(graph.Build());
  CHECK(!graph.HasValidationErrors());
}
