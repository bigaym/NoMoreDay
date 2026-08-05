#pragma once

#include "engine/render/graph/RenderGraph.hpp"
#include "engine/render/lighting/GlobalHeightField.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"
#include "raylib.h"
#include <entt/entt.hpp>
#include <cstdint>

class ResourceManager;

namespace NoMoreDay::components {
struct GPUShadowCaster;
struct EmissiveStampInput;
}

namespace NoMoreDay::render::core {
class QualityTierManager;
}

namespace NoMoreDay::render::debug {
class RenderProfiler;
}

namespace NoMoreDay::render::resources {
class TransientResourcePool;
}

namespace NoMoreDay::render::graph {

struct RenderContext {
  entt::registry *registry = nullptr;
  ResourceManager *resources = nullptr;
  const Camera2D *camera = nullptr;
  resources::TransientResourcePool *transientPool = nullptr;
  core::QualityTierManager *qualityManager = nullptr;
  debug::RenderProfiler *renderProfiler = nullptr;
  resources::FramebufferHandle hdrSceneBuffer = {};
  // Phase D (RG-2): live LDR output of PostProcessPass, published by the pass
  // at the end of its own Execute so downstream consumers (DistortionPass,
  // CompositePass) resolve the producer's current backing from the graph
  // context instead of a manual SetInputBuffer wiring. Invalid until
  // PostProcessPass::Execute writes it.
  resources::FramebufferHandle postProcessOutput = {};
  uint32_t giDistanceFieldTexture = 0u;
  int giDistanceFieldWidth = 0;
  int giDistanceFieldHeight = 0;
  uint32_t giEmissiveTexture = 0u;
  int giEmissiveWidth = 0;
  int giEmissiveHeight = 0;
  uint32_t giRadianceTexture = 0u;
  int giRadianceWidth = 0;
  int giRadianceHeight = 0;

  // Game-side occluder projection injected before graph execution (filled by the
  // gameplay adapter via the shared OccluderProjector). Points into an
  // Engine-owned staging buffer; count == casters.size(). Consumed by
  // OccluderExtractPass/ShadowBuildPass instead of reading game components.
  const components::GPUShadowCaster *occluders = nullptr;
  uint32_t occluderCount = 0u;
  uint32_t occluderStaticCount = 0u;
  uint32_t occluderDynamicCount = 0u;
  uint64_t occluderStaticSignature = 0u;
  uint64_t occluderDynamicSignature = 0u;

  // Game-side height-field projection injected before graph execution (filled by
  // the gameplay adapter via the shared HeightFieldAdapter). Points into an
  // Engine-owned staging buffer; count == stamps.size(). Consumed by
  // HeightShadowPass instead of reading game components.
  const lighting::GlobalHeightField::HeightStamp *heightFieldStamps = nullptr;
  uint32_t heightFieldStampCount = 0u;

  // Game-side emissive material stamp projection injected before graph
  // execution (filled by the gameplay adapter via the shared
  // EmissiveStampAdapter). Points into an Engine-owned staging buffer;
  // count == stamps.size(). Consumed by
  // RadianceCascadesPass::RunMaterialEmissive instead of reading game
  // components.
  const components::EmissiveStampInput *emissiveStamps = nullptr;
  uint32_t emissiveStampCount = 0u;

  // Game-side world semantics injected by the gameplay adapter (previously the
  // game Constants::World values read by HeightShadowPass).
  float worldWidth = 0.0f;
  float worldHeight = 0.0f;
  float tileWorldSize = 0.0f;

  // Per-frame snapshots supplied by external backing owners. RenderGraph may
  // resolve these for contract validation, but never allocates, binds, resizes,
  // or releases the referenced GL objects.
  std::vector<ImportedBackingHandle> importedBackings;

  const ImportedBackingHandle *FindImportedBacking(RenderResourceTag tag) const {
    for (const ImportedBackingHandle &backing : importedBackings) {
      if (backing.resourceTag == tag) {
        return &backing;
      }
    }
    return nullptr;
  }

  // Set by RenderGraph::Execute while a pass is running so the pass can emit
  // declared same-pass phase barriers (RenderContext::EmitPhaseBarrier) at the
  // exact execution point that pass-entry barriers cannot cover.
  RenderGraph *activeGraph = nullptr;

  // Emits the GL memory barrier declared via
  // RenderGraphBuilder::AddPhaseBarrier(sourcePhase, targetPhase, bits) for
  // the currently executing pass. Returns true when a matching declaration was
  // resolved and the barrier was issued; false when no graph is active or the
  // phase pair was never declared (contract violation).
  bool EmitPhaseBarrier(PipelineStage sourcePhase, PipelineStage targetPhase) const {
    return activeGraph != nullptr &&
           activeGraph->EmitActivePassPhaseBarrier(sourcePhase, targetPhase);
  }

  // B12 graph-driven binding execution: resolves and executes the admitted
  // BindBufferBase / BindImageUnit operations for the pass currently executing
  // (declared bindings matched against this per-frame imported backing
  // snapshot). Only the existing GPUUtils binding APIs are called; no
  // allocation, resize, release, or ownership change, and the graph never owns
  // GL handles. Returns true when every supported binding was admitted and
  // bound; false (with a recorded runtime diagnostic) when any was denied or
  // unsupported. Since B2-B4 convergence (2026-08-05) graph-driven binding is
  // the sole binding surface for these passes; the manual binds they used to
  // keep in Execute are removed.
  bool ApplyActivePassBindings() {
    return activeGraph != nullptr && activeGraph->ApplyActivePassBindings(*this);
  }

  // B2-B4 final convergence (2026-08-05): true when the graph-driven binding
  // surface of the currently executing pass was fully admitted against this
  // frame's imported-backing snapshot (or the pass declares no bindings —
  // vacuous admission). Pure query: resolves against the same snapshot
  // RenderGraph::Execute already used in ApplyActivePassBindings, issues no GL
  // calls and records no diagnostics. A pass calls it from inside its own
  // Execute to FAIL CLOSED (skip its dispatch) when the graph could not admit
  // its binding surface, instead of rendering garbage through unbound surfaces.
  // Returns false when no graph is active (no binding contract in effect);
  // callers must gate on activeGraph != nullptr and handle the standalone path
  // explicitly.
  bool AreActivePassBindingsAdmitted() const {
    return activeGraph != nullptr &&
           activeGraph->ResolveActivePassBindings(*this).allAdmitted;
  }

  bool IsValid() const {
    return registry != nullptr && resources != nullptr && camera != nullptr;
  }
};

} // namespace NoMoreDay::render::graph
