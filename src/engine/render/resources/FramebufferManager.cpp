#include "engine/render/resources/FramebufferManager.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"

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
constexpr uint32_t kGLHalfFloat = 0x140B;
constexpr uint32_t kGLUnsignedByte = 0x1401;
constexpr uint32_t kGLTextureMinFilter = 0x2801;
constexpr uint32_t kGLTextureMagFilter = 0x2800;
constexpr uint32_t kGLTextureWrapS = 0x2802;
constexpr uint32_t kGLTextureWrapT = 0x2803;
constexpr uint32_t kGLLinear = 0x2601;
constexpr uint32_t kGLClampToEdge = 0x812F;
constexpr uint32_t kGLRgba16f = 0x881A;

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
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureMinFilter,
                                            kGLLinear);
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureMagFilter,
                                            kGLLinear);
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureWrapS,
                                            kGLClampToEdge);
  NoMoreDay::utils::GPUUtils::TexParameteri(kGLTexture2D, kGLTextureWrapT,
                                            kGLClampToEdge);

  const uint32_t textureType =
      (internalFormat == kGLRgba16f) ? kGLHalfFloat : kGLUnsignedByte;
  NoMoreDay::utils::GPUUtils::TexImage2D(
      kGLTexture2D, 0, static_cast<int>(internalFormat), width, height, 0,
      kGLRgba, textureType, nullptr);
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

  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGLFramebuffer, 0);
  NoMoreDay::utils::GPUUtils::BindTexture(kGLTexture2D, 0);
  if (withDepth) {
    NoMoreDay::utils::GPUUtils::BindRenderbuffer(kGLRenderbuffer, 0);
  }

  return handle;
}

void FramebufferManager::Destroy(FramebufferHandle &handle) {
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
  if (!handle.IsValid() || (handle.width == newWidth && handle.height == newHeight) ||
      newWidth <= 0 || newHeight <= 0) {
    return;
  }

  const uint32_t format = handle.internalFormat;
  const bool withDepth = handle.depthRbo != 0;
  Destroy(handle);
  handle = Create(newWidth, newHeight, format, withDepth);
}

} // namespace NoMoreDay::render::resources
