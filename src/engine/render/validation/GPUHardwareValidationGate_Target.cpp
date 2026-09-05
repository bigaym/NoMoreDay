#include "engine/render/validation/GPUHardwareValidationGate.hpp"
#include "engine/render/validation/GPUHardwareValidationGateInternal.hpp"

#include "GLFW/glfw3.h"

namespace NoMoreDay::render::validation {

// M0-B: external target contract - captures the REAL state of the
// harness-owned composite target. Uses legal GL 4.3 pnames only
// (glGetFramebufferAttachmentParameteriv: OBJECT_TYPE/OBJECT_NAME/COLOR_ENCODING/
// COMPONENT_TYPE/*_SIZE, plus texture-level / renderbuffer parameter queries for
// extent and internal format). The pseudo-pnames that commit 5c257e22 removed
// (0x8D24/0x8D25/0x825D) were never valid core GL constants and are NOT reused.
// Fail-closed: a missing entry point -> "unavailable"; fbo == 0, an absent
// attachment, or a contract mismatch (extent/internalFormat) -> "failed"; only
// a fully verified capture yields "passed". Nothing is default-filled.
TargetAttachmentState
GPUHardwareValidationGate::CaptureTargetState(uint32_t framebuffer,
                                              int expectedWidth,
                                              int expectedHeight,
                                              uint32_t expectedInternalFormat) {
  constexpr uint32_t kGlFramebuffer = 0x8D40;
  constexpr uint32_t kGlColorAttachment0 = 0x8CE0;
  constexpr uint32_t kGlFramebufferBinding = 0x8CA6;
  constexpr uint32_t kGlViewport = 0x0BA2;
  constexpr uint32_t kGlScissorTest = 0x0C11;
  constexpr uint32_t kGlScissorBox = 0x0C10;
  constexpr uint32_t kGlFramebufferAttachmentObjectType = 0x8CD0;
  constexpr uint32_t kGlFramebufferAttachmentObjectName = 0x8CD1;
  constexpr uint32_t kGlFramebufferAttachmentColorEncoding = 0x8210;
  constexpr uint32_t kGlFramebufferAttachmentComponentType = 0x8211;
  constexpr uint32_t kGlFramebufferAttachmentRedSize = 0x8212;
  constexpr uint32_t kGlFramebufferAttachmentGreenSize = 0x8213;
  constexpr uint32_t kGlFramebufferAttachmentBlueSize = 0x8214;
  constexpr uint32_t kGlFramebufferAttachmentAlphaSize = 0x8215;
  constexpr uint32_t kGlFramebufferAttachmentDepthSize = 0x8216;
  constexpr uint32_t kGlFramebufferAttachmentStencilSize = 0x8217;
  constexpr uint32_t kGlTexture = 0x1702;
  constexpr uint32_t kGlRenderbuffer = 0x8D41;
  constexpr uint32_t kGlTexture2D = 0x0DE1;
  constexpr uint32_t kGlTextureBinding2D = 0x8069;
  constexpr uint32_t kGlTextureWidth = 0x1000;
  constexpr uint32_t kGlTextureHeight = 0x1001;
  constexpr uint32_t kGlTextureInternalFormat = 0x1003;
  constexpr uint32_t kGlRenderbufferBinding = 0x8CA7;
  constexpr uint32_t kGlRenderbufferWidth = 0x8D42;
  constexpr uint32_t kGlRenderbufferHeight = 0x8D43;
  constexpr uint32_t kGlRenderbufferInternalFormat = 0x8D81;

  TargetAttachmentState out;
  out.expectedInternalFormat = expectedInternalFormat;
  if (framebuffer == 0u) {
    out.status = "failed";
    out.reason =
        "no composite target to capture (fbo == 0); external target contract "
        "cannot be verified";
    return out;
  }

  using GlGetIntegervFn = void(APIENTRY *)(uint32_t, int *);
  using GlGetFramebufferAttachmentParameterivFn =
      void(APIENTRY *)(uint32_t, uint32_t, uint32_t, int *);
  using GlGetTexLevelParameterivFn =
      void(APIENTRY *)(uint32_t, int, uint32_t, int *);
  using GlGetRenderbufferParameterivFn =
      void(APIENTRY *)(uint32_t, uint32_t, int *);
  using GlIsEnabledFn = uint8_t(APIENTRY *)(uint32_t);

  auto glGetIntegerv = reinterpret_cast<GlGetIntegervFn>(
      glfwGetProcAddress("glGetIntegerv"));
  auto glGetFramebufferAttachmentParameteriv =
      reinterpret_cast<GlGetFramebufferAttachmentParameterivFn>(
          glfwGetProcAddress("glGetFramebufferAttachmentParameteriv"));
  auto glGetTexLevelParameteriv = reinterpret_cast<GlGetTexLevelParameterivFn>(
      glfwGetProcAddress("glGetTexLevelParameteriv"));
  auto glGetRenderbufferParameteriv =
      reinterpret_cast<GlGetRenderbufferParameterivFn>(
          glfwGetProcAddress("glGetRenderbufferParameteriv"));
  auto glIsEnabled =
      reinterpret_cast<GlIsEnabledFn>(glfwGetProcAddress("glIsEnabled"));

  if (glGetIntegerv == nullptr ||
      glGetFramebufferAttachmentParameteriv == nullptr) {
    out.status = "unavailable";
    out.reason =
        "glGetIntegerv/glGetFramebufferAttachmentParameteriv unavailable; "
        "external target contract cannot be verified (fail-closed)";
    return out;
  }

  // bind/viewport/scissor snapshot - global state, queried before binding the
  // target so the recorded values match the harness-side state.
  {
    int previousBinding = 0;
    glGetIntegerv(kGlFramebufferBinding, &previousBinding);
    out.framebufferBinding = static_cast<uint32_t>(previousBinding);
  }
  {
    int viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(kGlViewport, viewport);
    out.viewportX = viewport[0];
    out.viewportY = viewport[1];
    out.viewportWidth = viewport[2];
    out.viewportHeight = viewport[3];
  }
  out.scissorTestEnabled =
      (glIsEnabled != nullptr) && (glIsEnabled(kGlScissorTest) != 0);
  {
    int scissor[4] = {0, 0, 0, 0};
    glGetIntegerv(kGlScissorBox, scissor);
    out.scissorX = scissor[0];
    out.scissorY = scissor[1];
    out.scissorWidth = scissor[2];
    out.scissorHeight = scissor[3];
  }

  // Bind the external target and query the COLOR_ATTACHMENT0 identity.
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGlFramebuffer, framebuffer);
  int objectType = 0;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentObjectType,
                                        &objectType);
  out.attachmentObjectType = static_cast<uint32_t>(objectType);

  if (objectType == 0) { // GL_NONE
    NoMoreDay::utils::GPUUtils::BindFramebuffer(kGlFramebuffer, 0);
    out.status = "failed";
    out.reason =
        "COLOR_ATTACHMENT0 has no attachment (OBJECT_TYPE == GL_NONE); "
        "external target contract cannot be verified";
    return out;
  }

  int objectName = 0;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentObjectName,
                                        &objectName);
  out.attachmentObjectName = static_cast<uint32_t>(objectName);

  // Attachment format parameters (valid for a color-renderable attachment on
  // GL 4.3). A driver that rejects any of these surfaces a GL error which the
  // gate's debug collector records - fail-closed, never silently ignored.
  int value = 0;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentColorEncoding,
                                        &value);
  out.colorEncoding = static_cast<uint32_t>(value);
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentComponentType,
                                        &value);
  out.componentType = static_cast<uint32_t>(value);
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentRedSize, &value);
  out.redSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentGreenSize,
                                        &value);
  out.greenSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentBlueSize, &value);
  out.blueSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentAlphaSize, &value);
  out.alphaSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentDepthSize, &value);
  out.depthSize = value;
  glGetFramebufferAttachmentParameteriv(kGlFramebuffer, kGlColorAttachment0,
                                        kGlFramebufferAttachmentStencilSize,
                                        &value);
  out.stencilSize = value;

  // Extent + internal format via the attachment object's own query (texture
  // level parameters or renderbuffer parameters - the legal GL 4.3 way to read
  // them; the 0x8D24/0x8D25 pseudo-pnames were never valid constants).
  bool extentQueried = false;
  if (objectType == kGlTexture) {
    if (glGetTexLevelParameteriv != nullptr) {
      int previousBinding = 0;
      glGetIntegerv(kGlTextureBinding2D, &previousBinding);
      NoMoreDay::utils::GPUUtils::BindTexture(kGlTexture2D,
                                              static_cast<uint32_t>(objectName));
      glGetTexLevelParameteriv(kGlTexture2D, 0, kGlTextureWidth,
                               &out.attachmentWidth);
      glGetTexLevelParameteriv(kGlTexture2D, 0, kGlTextureHeight,
                               &out.attachmentHeight);
      glGetTexLevelParameteriv(kGlTexture2D, 0, kGlTextureInternalFormat,
                               &value);
      out.attachmentInternalFormat = static_cast<uint32_t>(value);
      NoMoreDay::utils::GPUUtils::BindTexture(
          kGlTexture2D, static_cast<uint32_t>(previousBinding));
      extentQueried = true;
    }
  } else if (objectType == kGlRenderbuffer) {
    if (glGetRenderbufferParameteriv != nullptr) {
      int previousBinding = 0;
      glGetIntegerv(kGlRenderbufferBinding, &previousBinding);
      NoMoreDay::utils::GPUUtils::BindRenderbuffer(
          kGlRenderbuffer, static_cast<uint32_t>(objectName));
      glGetRenderbufferParameteriv(kGlRenderbuffer, kGlRenderbufferWidth,
                                   &out.attachmentWidth);
      glGetRenderbufferParameteriv(kGlRenderbuffer, kGlRenderbufferHeight,
                                   &out.attachmentHeight);
      glGetRenderbufferParameteriv(kGlRenderbuffer,
                                   kGlRenderbufferInternalFormat, &value);
      out.attachmentInternalFormat = static_cast<uint32_t>(value);
      NoMoreDay::utils::GPUUtils::BindRenderbuffer(
          kGlRenderbuffer, static_cast<uint32_t>(previousBinding));
      extentQueried = true;
    }
  }

  // Restore the previous framebuffer binding.
  NoMoreDay::utils::GPUUtils::BindFramebuffer(kGlFramebuffer, 0);

  if (objectType != kGlTexture && objectType != kGlRenderbuffer) {
    out.status = "failed";
    out.reason =
        "COLOR_ATTACHMENT0 object type " + std::to_string(objectType) +
        " is neither GL_TEXTURE nor GL_RENDERBUFFER; external target contract "
        "cannot be verified";
    return out;
  }
  if (!extentQueried) {
    out.status = "unavailable";
    out.reason =
        "texture-level/renderbuffer parameter query entry point unavailable; "
        "extent/format of the external target cannot be verified (fail-closed)";
    return out;
  }

  // Contract verification: extent and internal format must match the expected
  // external target contract exactly. No default-fill is ever synthesized.
  if (out.attachmentWidth != expectedWidth ||
      out.attachmentHeight != expectedHeight) {
    out.status = "failed";
    char extentBuf[192] = {0};
    std::snprintf(extentBuf, sizeof(extentBuf),
                  "external target extent %dx%d does not match contract %dx%d",
                  out.attachmentWidth, out.attachmentHeight, expectedWidth,
                  expectedHeight);
    out.reason = extentBuf;
    return out;
  }
  if (expectedInternalFormat != 0u &&
      out.attachmentInternalFormat != expectedInternalFormat) {
    out.status = "failed";
    char formatBuf[192] = {0};
    std::snprintf(formatBuf, sizeof(formatBuf),
                  "external target internal format 0x%04X does not match "
                  "contract 0x%04X",
                  out.attachmentInternalFormat, expectedInternalFormat);
    out.reason = formatBuf;
    return out;
  }

  out.status = "passed";
  out.captured = true;
  return out;
}

} // namespace NoMoreDay::render::validation
