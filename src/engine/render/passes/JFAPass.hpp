#pragma once

#include "engine/render/gi/JFADistanceFieldEvaluator.hpp"
#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"

#include "raylib.h"
#include <cstdint>
#include <string>

class ResourceManager;

namespace NoMoreDay::render::passes {

class OccluderExtractPass;

struct JFAFrameReport {
  gi::JFAUpdateMode mode = gi::JFAUpdateMode::Full;
  gi::JFARect dirtyRect = {};
  gi::JFARect expandedRect = {};
  uint32_t dispatchTexelCount = 0;
  uint32_t occluderVersion = 0;
  uint32_t sdfVersion = 0;
  std::string fullReason;
  bool verificationAttempted = false;
  bool verificationPassed = false;
  bool verificationFallback = false;
  std::string verificationResult;
  std::string verificationArtifact;
};

class JFAPass final : public graph::RenderPass {
public:
  JFAPass();
  ~JFAPass() override;

  void Setup(graph::RenderGraphBuilder &builder) override;
  void Execute(graph::RenderContext &context) override;
  const char *GetName() const override { return "JFAPass"; }
  graph::RenderPassType Type() const override {
    return graph::RenderPassType::JFA;
  }

  bool Initialize(ResourceManager &resources);
  void Shutdown();
  void OnResize(int width, int height);

  void SetOccluderExtractPass(const OccluderExtractPass *pass) noexcept {
    m_occluderExtractPass = pass;
  }

  [[nodiscard]] bool HasDistanceField() const noexcept {
    return m_distanceFieldFull.IsValid();
  }
  [[nodiscard]] uint32_t GetDistanceFieldTexture() const noexcept {
    return m_distanceFieldFull.colorTexture;
  }
  [[nodiscard]] int GetDistanceFieldWidth() const noexcept {
    return m_distanceFieldFull.width;
  }
  [[nodiscard]] int GetDistanceFieldHeight() const noexcept {
    return m_distanceFieldFull.height;
  }
  [[nodiscard]] bool UsedFallbackPlus2ThisFrame() const noexcept {
    return m_usedFallbackPlus2ThisFrame;
  }
  [[nodiscard]] uint32_t GetLastOverflowCount() const noexcept {
    return m_lastOverflowCount;
  }
  [[nodiscard]] const std::string &GetLastFailureReason() const noexcept {
    return m_lastFailureReason;
  }
  [[nodiscard]] const JFAFrameReport &GetLastReport() const noexcept {
    return m_lastReport;
  }
  [[nodiscard]] uint32_t GetSdfVersion() const noexcept {
    return m_sdfVersion;
  }
  void SetForceFallbackPlus2ForTesting(bool enabled) noexcept {
    m_forceFallbackPlus2ForTesting = enabled;
  }
  void SetIncrementalExperimentEnabledForTesting(bool enabled) noexcept {
    m_incrementalExperimentEnabled = enabled;
  }
  void SetDynamicOccluderBoundsForTesting(gi::JFARect previous, gi::JFARect current) noexcept {
    m_testPreviousBounds = previous;
    m_testCurrentBounds = current;
    m_testBoundsOverridden = true;
  }
  void SetVerificationReadbackEnabledForTesting(bool enabled) noexcept {
    m_verificationReadbackEnabled = enabled;
  }
  [[nodiscard]] static gi::JFAUpdateDecision ApplyProductionUpdatePolicy(
      gi::JFAUpdateDecision decision, bool incrementalEnabled) noexcept;


private:
  bool EnsureResources(int fullWidth, int fullHeight, bool halfResolution);
  bool RunSeedInit(const graph::RenderContext &context, uint32_t occluderMaskTexture,
                   int fullWidth, int fullHeight, const gi::JFARect *rect = nullptr);
  bool RunJumpFloodStep(const graph::RenderContext &context, int stepSize,
                        int fullWidth, int fullHeight, uint32_t inputSeedTexture,
                        uint32_t outputSeedTexture, const gi::JFARect *rect = nullptr);
  bool RunDistanceResolve(const graph::RenderContext &context,
                          uint32_t occluderMaskTexture, int fullWidth, int fullHeight,
                          uint32_t inputSeedTexture, uint32_t outputDistanceTexture,
                          const gi::JFARect *rect = nullptr);
  bool RunUpsample(int fullWidth, int fullHeight, const gi::JFARect *rect = nullptr);
  bool ClearOverflowCounter();
  uint32_t ReadOverflowCounter() const;
  void ReportFailure(const char *reason);
  void MarkSuccess();
  void LogBarrierAuditOnce();

  const OccluderExtractPass *m_occluderExtractPass = nullptr;

  Shader m_seedInitShader = {};
  Shader m_jumpFloodShader = {};
  Shader m_distanceResolveShader = {};
  Shader m_upsampleShader = {};

  resources::FramebufferHandle m_seedPing = {};
  resources::FramebufferHandle m_seedPong = {};
  resources::FramebufferHandle m_distanceFieldWork = {};
  resources::FramebufferHandle m_distanceFieldFull = {};

  int m_seedInitWorkResolutionLoc = -1;
  int m_seedInitFullResolutionLoc = -1;
  int m_seedInitMaskTextureLoc = -1;
  int m_seedInitRectMinLoc = -1;

  int m_jumpStepSizeLoc = -1;
  int m_jumpWorkResolutionLoc = -1;
  int m_jumpFullResolutionLoc = -1;
  int m_jumpRectMinLoc = -1;

  int m_distanceWorkResolutionLoc = -1;
  int m_distanceFullResolutionLoc = -1;
  int m_distanceMaskTextureLoc = -1;
  int m_distanceRectMinLoc = -1;

  int m_upsampleHalfResolutionLoc = -1;
  int m_upsampleFullResolutionLoc = -1;
  int m_upsampleRectMinLoc = -1;

  uint32_t m_overflowCounterBuffer = 0u;

  int m_fullWidth = 0;
  int m_fullHeight = 0;
  int m_workWidth = 0;
  int m_workHeight = 0;
  uint32_t m_frameIndex = 0;
  uint32_t m_lastOverflowCount = 0;
  uint32_t m_sdfVersion = 0;

  gi::JFAViewKey m_previousViewKey = {};
  gi::JFARect m_previousOccluderBounds = {};
  uint32_t m_previousOccluderCount = 0;
  gi::JFARect m_testPreviousBounds = {};

  gi::JFARect m_testCurrentBounds = {};
  bool m_testBoundsOverridden = false;

  bool m_initialized = false;
  bool m_halfResolutionMode = false;
  bool m_usedFallbackPlus2ThisFrame = false;
  bool m_forceFallbackPlus2ForTesting = false;
  bool m_incrementalExperimentEnabled = false;
  bool m_verificationReadbackEnabled = false;
  bool m_lastExecuteFailure = false;

  bool m_lastExecuteSuccess = false;
  bool m_barrierAuditLogged = false;
  std::string m_lastFailureReason;
  JFAFrameReport m_lastReport = {};
};

} // namespace NoMoreDay::render::passes

