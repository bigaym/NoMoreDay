#pragma once

#include "engine/render/graph/RenderGraph.hpp"
#include <cstdint>
#include <string>
#include <string_view>

namespace NoMoreDay::render::graph {

enum class ResourceKind : uint8_t {
  Texture2D = 0,
  Texture2DArray,
  Framebuffer,
  StorageBuffer,
  VertexBuffer,
  IndexBuffer,
  UniformBuffer,
  QueryRing,
  PersistentMapping,
  ExternalTarget,
};

enum class ResourceFormat : uint8_t {
  Unknown = 0,
  RGBA8,
  RGBA16F,
  RGBA32F,
  R32F,
  R16F,
  R8,
  RG16F,
  RG32F,
  Depth24Stencil8,
  Depth32F,
};

enum class ExtentMode : uint8_t {
  Fixed = 0,
  MatchScreen,
  RelativeScreen,
  Custom,
};

struct ExtentPolicy {
  ExtentMode mode = ExtentMode::MatchScreen;
  uint32_t width = 0;
  uint32_t height = 0;
  float scale = 1.0f;
};

namespace ResourceUsage {
  constexpr uint32_t None             = 0;
  constexpr uint32_t ColorAttachment  = 1 << 0;
  constexpr uint32_t DepthAttachment  = 1 << 1;
  constexpr uint32_t ShaderRead       = 1 << 2;
  constexpr uint32_t StorageRead      = 1 << 3;
  constexpr uint32_t StorageWrite     = 1 << 4;
  constexpr uint32_t TransferSrc      = 1 << 5;
  constexpr uint32_t TransferDst      = 1 << 6;
  constexpr uint32_t UniformBuffer    = 1 << 7;
  constexpr uint32_t StorageBuffer    = 1 << 8;
}

enum class ResourceLifetime : uint8_t {
  Transient = 0,
  Persistent,
  External,
};

enum class HistoryRelation : uint8_t {
  None = 0,
  PingPong,
  HistoryRead,
  HistoryWrite,
};

enum class PipelineStage : uint8_t {
  Vertex = 0,
  Fragment,
  Compute,
  Transfer,
  FramebufferAttachment,
  Host,
};

enum class PassAccessMode : uint8_t {
  Read = 0,
  Write,
  ReadWrite,
};

struct TypedResourceDescriptor {
  std::string name;
  RenderResourceTag tag = RenderResourceTag::Custom;
  ResourceKind kind = ResourceKind::Texture2D;
  ResourceFormat format = ResourceFormat::RGBA8;
  ExtentPolicy extentPolicy;
  uint32_t mipLevels = 1;
  uint32_t arrayLayers = 1;
  uint32_t sampleCount = 1;
  uint32_t usageFlags = ResourceUsage::ColorAttachment | ResourceUsage::ShaderRead;
  ResourceLifetime lifetime = ResourceLifetime::Transient;
  HistoryRelation historyRelation = HistoryRelation::None;
  RenderOwnerTag ownerTag = RenderOwnerTag::Unknown;
  size_t estimatedSizeBytes = 0;
  uint64_t stableResourceId = 0;
};

struct TypedPassAccess {
  std::string resourceName;
  RenderResourceTag resourceTag = RenderResourceTag::Custom;
  PassAccessMode mode = PassAccessMode::Read;
  PipelineStage stage = PipelineStage::Fragment;
  uint32_t usageFlags = ResourceUsage::ShaderRead;
  uint32_t bindingOrAttachmentIndex = 0;
  RenderOwnerTag ownerTag = RenderOwnerTag::Unknown;
  uint64_t stableResourceId = 0;
};

constexpr const char *ToResourceKindName(ResourceKind kind) {
  switch (kind) {
  case ResourceKind::Texture2D: return "Texture2D";
  case ResourceKind::Texture2DArray: return "Texture2DArray";
  case ResourceKind::Framebuffer: return "Framebuffer";
  case ResourceKind::StorageBuffer: return "StorageBuffer";
  case ResourceKind::VertexBuffer: return "VertexBuffer";
  case ResourceKind::IndexBuffer: return "IndexBuffer";
  case ResourceKind::UniformBuffer: return "UniformBuffer";
  case ResourceKind::QueryRing: return "QueryRing";
  case ResourceKind::PersistentMapping: return "PersistentMapping";
  case ResourceKind::ExternalTarget: return "ExternalTarget";
  default: return "Unknown";
  }
}

constexpr const char *ToResourceFormatName(ResourceFormat format) {
  switch (format) {
  case ResourceFormat::RGBA8: return "RGBA8";
  case ResourceFormat::RGBA16F: return "RGBA16F";
  case ResourceFormat::RGBA32F: return "RGBA32F";
  case ResourceFormat::R32F: return "R32F";
  case ResourceFormat::R16F: return "R16F";
  case ResourceFormat::R8: return "R8";
  case ResourceFormat::RG16F: return "RG16F";
  case ResourceFormat::RG32F: return "RG32F";
  case ResourceFormat::Depth24Stencil8: return "Depth24Stencil8";
  case ResourceFormat::Depth32F: return "Depth32F";
  case ResourceFormat::Unknown:
  default: return "Unknown";
  }
}

constexpr const char *ToPipelineStageName(PipelineStage stage) {
  switch (stage) {
  case PipelineStage::Vertex: return "Vertex";
  case PipelineStage::Fragment: return "Fragment";
  case PipelineStage::Compute: return "Compute";
  case PipelineStage::Transfer: return "Transfer";
  case PipelineStage::FramebufferAttachment: return "FramebufferAttachment";
  case PipelineStage::Host: return "Host";
  default: return "Unknown";
  }
}

constexpr const char *ToPassAccessModeName(PassAccessMode mode) {
  switch (mode) {
  case PassAccessMode::Read: return "Read";
  case PassAccessMode::Write: return "Write";
  case PassAccessMode::ReadWrite: return "ReadWrite";
  default: return "Unknown";
  }
}

constexpr const char *ToResourceLifetimeName(ResourceLifetime lifetime) {
  switch (lifetime) {
  case ResourceLifetime::Transient: return "Transient";
  case ResourceLifetime::Persistent: return "Persistent";
  case ResourceLifetime::External: return "External";
  default: return "Unknown";
  }
}

constexpr uint32_t kInvalidBarrierBits = 0xFFFFFFFFu;

constexpr uint64_t StableResourceId(std::string_view name) {
  uint64_t hash = 1469598103934665603ull;
  for (const char character : name) {
    hash ^= static_cast<uint8_t>(character);
    hash *= 1099511628211ull;
  }
  return hash == 0 ? 1 : hash;
}

constexpr uint64_t ResolveStableResourceId(uint64_t explicitId,
                                           std::string_view name) {
  return explicitId != 0 ? explicitId : StableResourceId(name);
}

inline uint32_t MapGlBarrierBits(PipelineStage prevStage, PassAccessMode prevMode,
                                  PipelineStage nextStage, PassAccessMode nextMode,
                                  ResourceKind kind) {
  const bool previousWrites = prevMode == PassAccessMode::Write ||
                              prevMode == PassAccessMode::ReadWrite;
  const bool nextReads = nextMode == PassAccessMode::Read ||
                         nextMode == PassAccessMode::ReadWrite;
  if (!previousWrites) {
    return 0u;
  }

  uint32_t barrierBits = 0u;

  if (prevStage == PipelineStage::Compute) {
    if (kind == ResourceKind::StorageBuffer) {
      barrierBits |= 0x00002000u; // GL_SHADER_STORAGE_BARRIER_BIT
    } else if (kind == ResourceKind::Texture2D || kind == ResourceKind::Texture2DArray ||
               kind == ResourceKind::Framebuffer) {
      barrierBits |= 0x00000020u; // GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
      if (nextReads && (nextStage == PipelineStage::Fragment ||
                        nextStage == PipelineStage::FramebufferAttachment)) {
        barrierBits |= 0x00000008u; // GL_TEXTURE_FETCH_BARRIER_BIT
      } else if (nextStage == PipelineStage::Compute && !nextReads) {
        barrierBits |= 0x00000020u; // subsequent image access
      }
    } else if (kind == ResourceKind::UniformBuffer) {
      barrierBits |= 0x00000004u; // GL_UNIFORM_BARRIER_BIT
    } else {
      barrierBits |= 0x00000200u; // GL_BUFFER_UPDATE_BARRIER_BIT
    }
  }

  if (prevStage == PipelineStage::FramebufferAttachment || prevStage == PipelineStage::Fragment) {
    if (nextReads && (nextStage == PipelineStage::Compute ||
                      nextStage == PipelineStage::Fragment)) {
      if (kind == ResourceKind::Framebuffer || kind == ResourceKind::Texture2D || kind == ResourceKind::Texture2DArray) {
        barrierBits |= 0x00000400u | 0x00000008u; // GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT
        if (nextStage == PipelineStage::Compute && !nextReads) {
          barrierBits |= 0x00000020u; // GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
        }
      }
    }
  }

  if (prevStage == PipelineStage::Host || prevStage == PipelineStage::Transfer) {
    barrierBits |= 0x00000200u; // GL_BUFFER_UPDATE_BARRIER_BIT
  }

  return barrierBits;
}

inline uint32_t MapGlBarrierBits(PipelineStage stage, PassAccessMode mode) {
  return MapGlBarrierBits(stage, mode, stage, mode, ResourceKind::Texture2D);
}

} // namespace NoMoreDay::render::graph
