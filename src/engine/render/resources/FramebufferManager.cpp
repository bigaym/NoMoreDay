#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/render/resources/GPUTexturePool.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"

#include <atomic>

namespace NoMoreDay::render::resources {
namespace {

constexpr uint32_t kGLFramebuffer = 0x8D40;
constexpr uint32_t kGLRenderbuffer = 0x8D41;
constexpr uint32_t kGLTexture2D = 0x0DE1;
constexpr uint32_t kGLColorAttachment0 = 0x8CE0;
constexpr uint32_t kGLDepthAttachment = 0x8D00;
constexpr uint32_t kGLFramebufferComplete = 0x8CD5;
constexpr uint32_t kGLDepthComponent24 = 0x81A6;
constexpr uint32_t kGLRgba = 0x1908;
constexpr uint32_t kGLRed = 0x1903;
constexpr uint32_t kGLRg = 0x8227;
constexpr uint32_t kGLRgInteger = 0x8228;
constexpr uint32_t kGLHalfFloat = 0x140B;
constexpr uint32_t kGLUnsignedByte = 0x1401;
constexpr uint32_t kGLUnsignedShort = 0x1403;
constexpr uint32_t kGLTextureMinFilter = 0x2801;
constexpr uint32_t kGLTextureMagFilter = 0x2800;
constexpr uint32_t kGLTextureWrapS = 0x2802;
constexpr uint32_t kGLTextureWrapT = 0x2803;
constexpr uint32_t kGLLinear = 0x2601;
constexpr uint32_t kGLNearest = 0x2600;
constexpr uint32_t kGLClampToEdge = 0x812F;
constexpr uint32_t kGLR8 = 0x8229;
constexpr uint32_t kGLR16f = 0x822D;
constexpr uint32_t kGLRgba16f = 0x881A;
constexpr uint32_t kGLRg16f = 0x822F;
constexpr uint32_t kGLRg16ui = 0x823A;
constexpr uint64_t kBytesPerPixelRgba8 = 4u;
constexpr uint64_t kBytesPerPixelR8 = 1u;
constexpr uint64_t kBytesPerPixelR16f = 2u;
constexpr uint64_t kBytesPerPixelRgba16f = 8u;
constexpr uint64_t kBytesPerPixelRg16f = 4u;
constexpr uint64_t kBytesPerPixelRg16ui = 4u;
constexpr uint64_t kBytesPerDepthRbo = 4u;
std::atomic<uint64_t> s_trackedBytes = 0u;

uint64_t BytesPerPixel(uint32_t internalFormat) {
  if (internalFormat == kGLR8) {
    return kBytesPerPixelR8;
  }
  if (internalFormat == kGLR16f) {
    return kBytesPerPixelR16f;
  }
  if (internalFormat == kGLRgba16f) {
    return kBytesPerPixelRgba16f;
  }
  if (internalFormat == kGLRg16f) {
    return kBytesPerPixelRg16f;
  }
  if (internalFormat == kGLRg16ui) {
    return kBytesPerPixelRg16ui;
  }
  return kBytesPerPixelRgba8;
}

} // namespace

FramebufferHandle FramebufferManager::Create(int width, int height,
                                             uint32_t internalFormat,
                                             bool withDepth) {
  FramebufferHandle handle = {};
  if (width <= 0 || height <= 0) {
    LOG_ERROR("FramebufferManager::Create invalid size {}x{}", width, height);
    return handle;
  }

  NoMoreDay::utils::GPUUtils::GenFramebuffers(1, &handle.fbo);
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, handle.fbo);

  NoMoreDay::utils::GPUUtils::GenTextures(1, &handle.colorTexture);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, handle.colorTexture);
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureWrapS,
                                            kGLClampToEdge);
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureWrapT,
                                            kGLClampToEdge);

  uint32_t uploadFormat = kGLRgba;
  uint32_t textureType = kGLUnsignedByte;
  uint32_t minFilter = kGLLinear;
  uint32_t magFilter = kGLLinear;
  if (internalFormat == kGLR8) {
    uploadFormat = kGLRed;
    textureType = kGLUnsignedByte;
    minFilter = kGLNearest;
    magFilter = kGLNearest;
  } else if (internalFormat == kGLR16f) {
    uploadFormat = kGLRed;
    textureType = kGLHalfFloat;
    minFilter = kGLNearest;
    magFilter = kGLNearest;
  } else if (internalFormat == kGLRgba16f) {
    uploadFormat = kGLRgba;
    textureType = kGLHalfFloat;
  } else if (internalFormat == kGLRg16f) {
    uploadFormat = kGLRg;
    textureType = kGLHalfFloat;
  } else if (internalFormat == kGLRg16ui) {
    uploadFormat = kGLRgInteger;
    textureType = kGLUnsignedShort;
    minFilter = kGLNearest;
    magFilter = kGLNearest;
  }
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureMinFilter,
                                            static_cast<int>(minFilter));
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureMagFilter,
                                            static_cast<int>(magFilter));
  NoMoreDay::utils::GPUUtils::TexImage2D(
      kGLTexture2D, 0, static_cast<int>(internalFormat), width, height, 0,
      uploadFormat, textureType, nullptr);
  NoMoreDay::utils::GPUUtils::FramebufferTexture2D(
      kGLFramebuffer, kGLColorAttachment0, kGLTexture2D, handle.colorTexture,
      0);

  if (withDepth) {
    NoMoreDay::utils::GPUUtils::GenRenderbuffers(1, &handle.depthRbo);
    NoMoreDay::utils::GPUUtils::BindRenderbuffer(kGLRenderbuffer, handle.depthRbo);
    NoMoreDay::utils::GPUUtils::RenderbufferStorage(kGLRenderbuffer,
                                                    kGLDepthComponent24, width,
                                                    height);
    NoMoreDay::utils::GPUUtils::FramebufferRenderbuffer(
        kGLFramebuffer, kGLDepthAttachment, kGLRenderbuffer, handle.depthRbo);
  }

  const uint32_t status =
      NoMoreDay::utils::GPUUtils::CheckFramebufferStatus(kGLFramebuffer);
  if (status != kGLFramebufferComplete) {
    LOG_ERROR("FramebufferManager::Create incomplete FBO status=0x{:X}", status);
    NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
    Destroy(handle);
    return {};
  }

  handle.width = width;
  handle.height = height;
  handle.internalFormat = internalFormat;
  const uint64_t colorBytes =
      static_cast<uint64_t>(width) * static_cast<uint64_t>(height) *
      BytesPerPixel(internalFormat);
  const uint64_t depthBytes =
      withDepth ? (static_cast<uint64_t>(width) * static_cast<uint64_t>(height) *
                   kBytesPerDepthRbo)
                : 0u;
  handle.trackedBytes = colorBytes + depthBytes;
  s_trackedBytes.fetch_add(handle.trackedBytes, std::memory_order_relaxed);

  if (handle.fbo > 0) {
    GPUResourceRegistry::Get().RegisterResource(
        handle.fbo, graph::ResourceKind::Framebuffer, graph::RenderOwnerTag::Scene,
        handle.trackedBytes, "Framebuffer");
  }
  if (handle.colorTexture > 0) {
    GPUResourceRegistry::Get().RegisterResource(
        handle.colorTexture, graph::ResourceKind::Texture2D, graph::RenderOwnerTag::Scene,
        colorBytes, "FBOColorTexture");
  }

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, 0);
  if (withDepth) {
    NoMoreDay::utils::GPUUtils::BindRenderbuffer(kGLRenderbuffer, 0);
  }

  return handle;
}

void FramebufferManager::Destroy(FramebufferHandle &handle) {
  if (handle.colorTexture != 0) {
    GPUResourceRegistry::Get().UnregisterResource(
        handle.colorTexture, graph::ResourceKind::Texture2D);
  }
  if (handle.fbo != 0) {
    GPUResourceRegistry::Get().UnregisterResource(
        handle.fbo, graph::ResourceKind::Framebuffer);
  }

  if (handle.trackedBytes > 0u) {
    const uint64_t tracked = s_trackedBytes.load(std::memory_order_relaxed);
    if (tracked >= handle.trackedBytes) {
      s_trackedBytes.fetch_sub(handle.trackedBytes, std::memory_order_relaxed);
    } else {
      s_trackedBytes.store(0u, std::memory_order_relaxed);
    }
    handle.trackedBytes = 0u;
  }

  if (handle.depthRbo != 0) {
    NoMoreDay::utils::GPUUtils::DeleteRenderbuffers(1, &handle.depthRbo);
    handle.depthRbo = 0;
  }
  if (handle.colorTexture != 0) {
    NoMoreDay::utils::GPUUtils::DeleteTextures(1, &handle.colorTexture);
    handle.colorTexture = 0;
  }
  if (handle.fbo != 0) {
    NoMoreDay::utils::GPUUtils::DeleteFramebuffers(1, &handle.fbo);
    handle.fbo = 0;
  }
  handle.width = 0;
  handle.height = 0;
  handle.internalFormat = 0;
}

void FramebufferManager::Resize(FramebufferHandle &handle, int newWidth,
                                int newHeight) {
  ResizeSafe(handle, newWidth, newHeight, nullptr);
}

void FramebufferManager::ResizeSafe(FramebufferHandle &handle, int newWidth,
                                    int newHeight, void *retireFence) {
  if (!handle.IsValid() || (handle.width == newWidth && handle.height == newHeight) ||
      newWidth <= 0 || newHeight <= 0) {
    return;
  }

  const uint32_t format = handle.internalFormat;
  const bool withDepth = handle.depthRbo != 0;
  if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
    Destroy(handle);
    handle = Create(newWidth, newHeight, format, withDepth);
    return;
  }
  // Acquire the new resource first: on failure (e.g. GPU allocation failure)
  // the old resource is kept intact instead of being retired and leaving the
  // caller with an invalid handle.
  FramebufferHandle next =
      GPUTexturePool::Get().Acquire(newWidth, newHeight, format, withDepth);
  if (!next.IsValid()) {
    LOG_WARN("FramebufferManager::ResizeSafe: acquire failed for {}x{} format=0x{:X} "
             "withDepth={}; keeping old framebuffer {}x{}",
             newWidth, newHeight, format, withDepth, handle.width, handle.height);
    return;
  }
  GPUTexturePool::Get().RetireOldResource(handle, retireFence);
  handle = next;
}

uint64_t FramebufferManager::GetTrackedBytes() {
  return s_trackedBytes.load(std::memory_order_relaxed);
}

void FramebufferManager::ResetTrackedBytesForTesting() {
  s_trackedBytes.store(0u, std::memory_order_relaxed);
}

} // namespace NoMoreDay::render::resources
