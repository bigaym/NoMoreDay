#pragma once

#include "engine/render/GPUUtils.hpp"
#include "engine/render/RenderConstants.hpp"
#include "rlgl.h"

namespace NoMoreDay::render::core {

// Contract template (§13): enter/leave custom GL stages with an explicit rlgl flush.
inline void ApplyRlglFlushTemplate() {
  rlDrawRenderBatchActive();
}

// Contract template (§13): synchronize compute writes before fragment reads.
inline void ApplyComputeToFragmentBarrierTemplate() {
  constexpr uint32_t kComputeToFragmentMask =
      static_cast<uint32_t>(NoMoreDay::RenderConstants::Barrier::SSBO) |
      static_cast<uint32_t>(NoMoreDay::RenderConstants::Barrier::Buffer);
  NoMoreDay::utils::GPUUtils::MemoryBarrier(kComputeToFragmentMask);
}

} // namespace NoMoreDay::render::core
