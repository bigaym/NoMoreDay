#include "engine/render/GPUSkillEffectSystem.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
  #include "engine/render/RenderConstants.hpp"
  #include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "engine/render/resources/GPUResourceRegistry.hpp"
#include "engine/render/trail/GPUTrailRenderer.hpp"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace NoMoreDay::systems {
namespace {

constexpr size_t kMaxQueuedSkillEvents = 4096u;
constexpr size_t kMaxQueuedDistortion = 32u;
constexpr int kMaxShaderIncludeDepth = 8;
struct SkillCapEntry {
  int high = 0;
  int medium = 0;
  int low = 0;
};

constexpr std::array<SkillCapEntry, 10> kSkillCaps = {{
    {0, 0, 0},         // 0: invalid
    {24, 16, 8},       // 1: Flowing Thrust
    {32, 24, 16},      // 2: Rending Wave
    {16, 12, 8},       // 3: Blade Formation
    {6, 5, 4},         // 4: Blade Ward
    {4096, 2048, 1024},// 5: Infinite Blades
    {4, 3, 2},         // 6: Sword Array
    {64, 48, 32},      // 7: Mind Blade
    {20, 14, 10},      // 8: Blade Boomerang
    {4, 3, 2},         // 9: Phantom Trance
}};

constexpr std::array<SkillCapEntry, 10> kTriggerCaps = {{
    {0, 0, 0},   // 0: invalid
    {12, 9, 6},  // 1: Flowing Thrust
    {14, 10, 7}, // 2: Rending Wave
    {8, 6, 4},   // 3: Blade Formation
    {6, 5, 4},   // 4: Blade Ward
    {14, 10, 7}, // 5: Infinite Blades
    {6, 4, 3},   // 6: Sword Array
    {18, 13, 9}, // 7: Mind Blade
    {10, 7, 5},  // 8: Blade Boomerang
    {6, 4, 3},   // 9: Phantom Trance
}};

Color ResolveSkillColor(const uint32_t skillId) {
  switch (skillId) {
  case 1:
    return SKYBLUE;
  case 2:
    return BLUE;
  case 3:
    return Color{200, 230, 255, 255};
  case 4:
    return Color{180, 220, 255, 255};
  case 5:
    return ORANGE;
  case 6:
    return Color{120, 200, 255, 255};
  case 7:
    return GOLD;
  case 8:
    return Color{90, 180, 255, 255};
  case 9:
    return Color{240, 245, 255, 255};
  default:
    return WHITE;
  }
}

bool ReadTextFile(const std::filesystem::path &path, std::string &out) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return false;
  }
  std::stringstream ss;
  ss << file.rdbuf();
  out = ss.str();
  return true;
}

std::string ResolveShaderIncludes(const std::filesystem::path &path, int depth) {
  if (depth > kMaxShaderIncludeDepth) {
    LOG_ERROR("GPUSkillEffectSystem: shader include depth exceeded at {}",
              path.string());
    return {};
  }

  std::string source;
  if (!ReadTextFile(path, source)) {
    LOG_ERROR("GPUSkillEffectSystem: failed to read shader file {}",
              path.string());
    return {};
  }

  std::stringstream input(source);
  std::ostringstream output;
  std::string line;
  while (std::getline(input, line)) {
    const std::string includeTag = "#include \"";
    const size_t start = line.find(includeTag);
    if (start == std::string::npos) {
      output << line << '\n';
      continue;
    }

    const size_t pathStart = start + includeTag.size();
    const size_t endQuote = line.find('"', pathStart);
    if (endQuote == std::string::npos) {
      output << line << '\n';
      continue;
    }

    const std::string relative = line.substr(pathStart, endQuote - pathStart);
    const std::filesystem::path includePath = path.parent_path() / relative;
    const std::string included = ResolveShaderIncludes(includePath, depth + 1);
    output << included << '\n';
  }

  return output.str();
}

Shader LoadShaderWithIncludes(const std::filesystem::path &vertexPath,
                              const std::filesystem::path &fragmentPath) {
  Shader shader = {};
  const std::string vertexSrc = ResolveShaderIncludes(vertexPath, 0);
  const std::string fragmentSrc = ResolveShaderIncludes(fragmentPath, 0);
  if (vertexSrc.empty() || fragmentSrc.empty()) {
    return shader;
  }

  unsigned int vsId = rlCompileShader(vertexSrc.c_str(), RL_VERTEX_SHADER);
  unsigned int fsId = rlCompileShader(fragmentSrc.c_str(), RL_FRAGMENT_SHADER);
  if (vsId == 0 || fsId == 0) {
    LOG_ERROR("GPUSkillEffectSystem: shader compile failed for {} / {}",
              vertexPath.string(), fragmentPath.string());
    return shader;
  }

  const unsigned int programId = rlLoadShaderProgram(vsId, fsId);
  if (programId == 0) {
    LOG_ERROR("GPUSkillEffectSystem: shader link failed for {} / {}",
              vertexPath.string(), fragmentPath.string());
    return shader;
  }

  // RenderDoc 可读性: 用 vertex shader 路径 basename 命名 program。
  const std::string programLabel =
      NoMoreDay::utils::GPUUtils::BaseNameNoExt(vertexPath.string().c_str());
  NoMoreDay::utils::GPUUtils::LabelProgram(programId, programLabel.c_str());

  shader.id = programId;
  shader.locs = static_cast<int *>(RL_CALLOC(RL_MAX_SHADER_LOCATIONS, sizeof(int)));
  for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; ++i) {
    shader.locs[i] = -1;
  }
  return shader;
}

uint32_t EncodeSkillEffectFlags(const uint8_t elementType,
                                const uint32_t skillId) {
  const uint8_t clamped =
      std::min<uint8_t>(elementType, static_cast<uint8_t>(SkillVfxElementType::Void));
  return NoMoreDay::render::skillfx::PackSkillEffectFlags(clamped, skillId);
}

float ClampIntensity(const float value) {
  return std::clamp(value, 0.25f, 3.0f);
}

Vector2 NormalizedDirection(Vector2 from, Vector2 to) {
  Vector2 direction = Vector2Subtract(to, from);
  if (Vector2LengthSqr(direction) <= 1e-5f) {
    return Vector2{1.0f, 0.0f};
  }
  return Vector2Normalize(direction);
}

struct VfxFallbackPolicy {
  uint8_t tier = static_cast<uint8_t>(render::core::QualityTier::Medium);
  float particleEmissionScale = 1.0f;
  int trailSampleStride = 1;
  bool allowTrailStroke = true;
  bool allowDistortion = false;
  bool allowSecondaryGlow = true;
  bool allowEnvironmentParticles = true;
  bool useAfterimageSpriteFallback = false;
};

VfxFallbackPolicy BuildVfxFallbackPolicy(const SkillVfxEvent &event) {
  auto &qualityManager = render::core::QualityTierManager::Get();
  const uint8_t runtimeTier =
      qualityManager.IsInitialized()
          ? static_cast<uint8_t>(qualityManager.GetTier())
          : static_cast<uint8_t>(render::core::QualityTier::Medium);
  const uint8_t eventTier = std::min<uint8_t>(event.qualityTier, 3u);
  const uint8_t tier = std::min(runtimeTier, eventTier);

  VfxFallbackPolicy policy = {};
  policy.tier = tier;
  policy.allowTrailStroke = !qualityManager.IsInitialized() ||
                            qualityManager.GetConfig().trailEnabled;
  policy.allowDistortion = qualityManager.IsInitialized() &&
                           qualityManager.GetConfig().distortionEnabled &&
                           tier >= static_cast<uint8_t>(render::core::QualityTier::High);

  if (qualityManager.IsInitialized()) {
    const int detail = std::clamp(qualityManager.GetConfig().vfxSequenceDetail, 0, 2);
    if (detail == 0) {
      policy.particleEmissionScale *= 0.5f;
      policy.trailSampleStride = 2;
      policy.allowSecondaryGlow = false;
    } else if (detail == 1) {
      policy.particleEmissionScale *= 0.75f;
    }

    const int degradeLevel =
        std::clamp(qualityManager.GetAutoDegradeLevel(), 0, 6);
    if (degradeLevel >= 1) {
      policy.particleEmissionScale *= 0.5f;
    }
    if (degradeLevel >= 2) {
      policy.allowDistortion = false;
    }
    if (degradeLevel >= 3) {
      policy.trailSampleStride = std::max(policy.trailSampleStride, 2);
    }
    if (degradeLevel >= 4) {
      policy.allowSecondaryGlow = false;
    }
    if (degradeLevel >= 5) {
      policy.allowEnvironmentParticles = false;
    }
    if (degradeLevel >= 6) {
      policy.useAfterimageSpriteFallback = true;
    }
  }

  if (tier <= static_cast<uint8_t>(render::core::QualityTier::Low)) {
    policy.particleEmissionScale = std::min(policy.particleEmissionScale, 0.5f);
    policy.trailSampleStride = std::max(policy.trailSampleStride, 2);
    policy.allowSecondaryGlow = false;
  } else if (tier <= static_cast<uint8_t>(render::core::QualityTier::Medium)) {
    policy.particleEmissionScale = std::min(policy.particleEmissionScale, 0.75f);
    policy.trailSampleStride = std::max(policy.trailSampleStride, 2);
  }

  policy.particleEmissionScale = std::clamp(policy.particleEmissionScale, 0.1f, 1.0f);
  return policy;
}

Color ResolveElementColor(const uint8_t elementType, const Color fallback) {
  switch (static_cast<SkillVfxElementType>(elementType)) {
  case SkillVfxElementType::Fire:
    return Color{255, 120, 40, 255};
  case SkillVfxElementType::Cold:
    return Color{120, 210, 255, 255};
  case SkillVfxElementType::Lightning:
    return Color{250, 245, 130, 255};
  case SkillVfxElementType::Void:
    return Color{175, 95, 255, 255};
  case SkillVfxElementType::Physical:
  default:
    return fallback;
  }
}

int ParseSelectorInt(const nlohmann::json &value, const int wildcard = -1) {
  if (value.is_null()) {
    return wildcard;
  }
  if (value.is_number_integer()) {
    return value.get<int>();
  }
  if (value.is_string()) {
    const std::string token = value.get<std::string>();
    if (token == "*") {
      return wildcard;
    }
    static const std::unordered_map<std::string, int> kEventMap = {
        {"CastStart", static_cast<int>(SkillVfxEventType::CastStart)},
        {"CastImpact", static_cast<int>(SkillVfxEventType::CastImpact)},
        {"TriggerProc", static_cast<int>(SkillVfxEventType::TriggerProc)},
        {"EmpoweredConsume", static_cast<int>(SkillVfxEventType::EmpoweredConsume)},
        {"BuffEnter", static_cast<int>(SkillVfxEventType::BuffEnter)},
        {"BuffExit", static_cast<int>(SkillVfxEventType::BuffExit)},
        {"TransmuterSwitch", static_cast<int>(SkillVfxEventType::TransmuterSwitch)},
        {"KeystoneActivate", static_cast<int>(SkillVfxEventType::KeystoneActivate)},
    };
    if (auto it = kEventMap.find(token); it != kEventMap.end()) {
      return it->second;
    }
  }
  return wildcard;
}

GPUSkillEffectSystem::RecipeActionKind ParseActionKind(
    const nlohmann::json &value) {
  if (value.is_number_integer()) {
    return static_cast<GPUSkillEffectSystem::RecipeActionKind>(
        std::clamp(value.get<int>(), 0, 4));
  }
  if (value.is_string()) {
    const std::string token = value.get<std::string>();
    if (token == "ParticleBurst") {
      return GPUSkillEffectSystem::RecipeActionKind::ParticleBurst;
    }
    if (token == "TrailStroke") {
      return GPUSkillEffectSystem::RecipeActionKind::TrailStroke;
    }
    if (token == "DistortionPulse") {
      return GPUSkillEffectSystem::RecipeActionKind::DistortionPulse;
    }
    if (token == "ResistOverlay") {
      return GPUSkillEffectSystem::RecipeActionKind::ResistOverlay;
    }
  }
  return GPUSkillEffectSystem::RecipeActionKind::Overlay;
}

// Validates the scalar element contract at the Engine boundary before recipe
// selection. Game owns the Tag -> scalar translation; here an out-of-range
// scalar is unsafe and falls back to Physical with an observable diagnostic.
uint8_t SanitizeSkillVfxElementType(const SkillVfxEvent &event) {
  const uint8_t normalized = NormalizeSkillVfxElementType(event.elementType);
  if (normalized != event.elementType) {
    LOG_LIMITED_WARN(1.0f,
                     "GPUSkillEffectSystem: invalid elementType={} for "
                     "skillId={} event={}; falling back to Physical",
                     static_cast<uint32_t>(event.elementType), event.skillId,
                     static_cast<int>(event.type));
  }
  return normalized;
}

int ResolveRoleMaskPriority(const uint32_t mask) {
  if (mask == SkillVfxNodeRoleMask::None) {
    return 0;
  }
  if ((mask & SkillVfxNodeRoleMask::Keystone) != 0u) {
    return 4;
  }
  if ((mask & SkillVfxNodeRoleMask::Trigger) != 0u) {
    return 3;
  }
  if ((mask & SkillVfxNodeRoleMask::Synergy) != 0u) {
    return 2;
  }
  return 1;
}

} // namespace

void GPUSkillEffectSystem::Init(ResourceManager &rm, int maxEffects) {
  (void)rm;
  if (m_shader.id != 0) {
    return;
  }

  m_maxEffects = maxEffects;
  LOG_INFO("Initializing GPUSkillEffectSystem with max {} effects...",
           maxEffects);

  m_hostBuffer.resize(m_maxEffects);
  m_currentCount = 0;
  m_pendingEvents.clear();
  m_pendingDistortion.clear();
  m_pendingResistOverlay.clear();
  m_skillFrameCounts.fill(0);
  m_triggerFrameCounts.fill(0);
  m_triggerCarryBlend.fill(0.0f);
  m_triggerDedupKeys.clear();
  m_recipes.clear();

  m_gpuBuffer.Create(m_maxEffects * sizeof(components::GPUSkillEffect), nullptr,
                     RL_DYNAMIC_DRAW);
  m_shader = LoadShaderWithIncludes("assets/shaders/sh_skill_effect.vs",
                                    "assets/shaders/sh_skill_effect.fs");
  if (m_shader.id == 0) {
    m_shader = NoMoreDay::utils::GPUUtils::LoadShaderLabeled(
        "assets/shaders/sh_skill_effect.vs", "assets/shaders/sh_skill_effect.fs");
  }
  m_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(m_shader, "mvp");
  m_timeLoc = GetShaderLocation(m_shader, "uTime");
  LoadSkillVfxRecipes("assets/data/vfx/blade_ascendant_v3.json");

  InitRender();
}

void GPUSkillEffectSystem::InitRender() {
  float vertices[] = {
      -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f,
      0.5f,  -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f,
  };

  m_quadVAO = rlLoadVertexArray();
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      m_quadVAO, NoMoreDay::render::graph::ResourceKind::VertexArray,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, 0u,
      "SkillEffectQuadVAO");
  rlEnableVertexArray(m_quadVAO);
  m_quadVBO = rlLoadVertexBuffer(vertices, sizeof(vertices), false);
  NoMoreDay::render::resources::GPUResourceRegistry::Get().RegisterResource(
      m_quadVBO, NoMoreDay::render::graph::ResourceKind::VertexBuffer,
      NoMoreDay::render::graph::RenderOwnerTag::Unknown, sizeof(vertices),
      "SkillEffectQuadVBO");
  rlSetVertexAttribute(0, 2, RL_FLOAT, false, 0, 0);
  rlEnableVertexAttribute(0);
  rlDisableVertexArray();
}

void GPUSkillEffectSystem::Submit(const components::GPUSkillEffect &effect) {
  if (m_currentCount < m_maxEffects) {
    m_hostBuffer[m_currentCount] = effect;
    ++m_currentCount;
  }
}

SkillVfxEvent GPUSkillEffectSystem::NormalizeSkillVfxEvent(
    const SkillVfxEvent &event) {
  SkillVfxEvent normalized = event;
  normalized.elementType = SanitizeSkillVfxElementType(event);
  return normalized;
}

void GPUSkillEffectSystem::SubmitSkillEvent(const SkillVfxEvent &event) {
  if (event.skillId == 0u) {
    return;
  }
  if (m_pendingEvents.size() >= kMaxQueuedSkillEvents) {
    return;
  }
  m_pendingEvents.push_back(NormalizeSkillVfxEvent(event));
}

std::vector<SkillVfxEvent>
GPUSkillEffectSystem::GetStagedSkillEventsForTesting() const {
  return m_pendingEvents;
}

void GPUSkillEffectSystem::DrainDistortionRequests(
    std::vector<DistortionRequest> &out) {
  if (m_pendingDistortion.empty()) {
    return;
  }
  out.insert(out.end(), m_pendingDistortion.begin(), m_pendingDistortion.end());
  m_pendingDistortion.clear();
}

void GPUSkillEffectSystem::DrainResistOverlayRequests(
    std::vector<ResistOverlayRequest> &out) {
  if (m_pendingResistOverlay.empty()) {
    return;
  }
  out.insert(out.end(), m_pendingResistOverlay.begin(),
             m_pendingResistOverlay.end());
  m_pendingResistOverlay.clear();
}

bool GPUSkillEffectSystem::TrySubmitCapped(
    const uint32_t skillId, const int cap,
    const components::GPUSkillEffect &effect) {
  if (cap <= 0 || m_currentCount >= m_maxEffects) {
    return false;
  }
  if (skillId >= m_skillFrameCounts.size()) {
    return false;
  }
  if (m_skillFrameCounts[skillId] >= cap) {
    return false;
  }
  Submit(effect);
  ++m_skillFrameCounts[skillId];
  return true;
}

int GPUSkillEffectSystem::ResolveSkillCap(const uint32_t skillId,
                                          const uint8_t tier) const {
  if (skillId >= kSkillCaps.size()) {
    return 0;
  }
  const SkillCapEntry &entry = kSkillCaps[skillId];
  if (tier <= static_cast<uint8_t>(render::core::QualityTier::Low)) {
    return entry.low;
  }
  if (tier <= static_cast<uint8_t>(render::core::QualityTier::Medium)) {
    return entry.medium;
  }
  return entry.high;
}

int GPUSkillEffectSystem::ResolveTriggerCap(const uint32_t skillId,
                                            const uint8_t tier) const {
  if (skillId >= kTriggerCaps.size()) {
    return 0;
  }
  const SkillCapEntry &entry = kTriggerCaps[skillId];
  if (tier <= static_cast<uint8_t>(render::core::QualityTier::Low)) {
    return entry.low;
  }
  if (tier <= static_cast<uint8_t>(render::core::QualityTier::Medium)) {
    return entry.medium;
  }
  return entry.high;
}

bool GPUSkillEffectSystem::ConsumeTriggerBudget(const SkillVfxEvent &event,
                                                const uint8_t tier,
                                                float &actionScale,
                                                float &intensityScale) {
  actionScale = 1.0f;
  intensityScale = 1.0f;
  if (event.type != SkillVfxEventType::TriggerProc) {
    return true;
  }
  if (event.skillId >= m_triggerFrameCounts.size() ||
      event.skillId >= m_triggerCarryBlend.size()) {
    return false;
  }

  const int triggerCap = ResolveTriggerCap(event.skillId, tier);
  if (triggerCap <= 0) {
    return false;
  }

  int &counter = m_triggerFrameCounts[event.skillId];
  ++counter;

  if (counter <= triggerCap) {
    const float carry = m_triggerCarryBlend[event.skillId];
    m_triggerCarryBlend[event.skillId] = 0.0f;
    intensityScale += carry * 0.35f;
    return true;
  }

  const int overflow = counter - triggerCap;
  const int stride = std::clamp(2 + overflow / std::max(1, triggerCap), 2, 6);
  if ((overflow % stride) != 0) {
    m_triggerCarryBlend[event.skillId] =
        std::min(m_triggerCarryBlend[event.skillId] + 0.08f, 0.6f);
    return false;
  }

  actionScale = std::max(0.25f, 1.0f / static_cast<float>(stride));
  intensityScale = std::max(0.65f, actionScale);
  const float carry = m_triggerCarryBlend[event.skillId];
  m_triggerCarryBlend[event.skillId] = 0.0f;
  intensityScale += carry * 0.35f;
  return true;
}

bool GPUSkillEffectSystem::ShouldCullDuplicateTrigger(
    const SkillVfxEvent &event) {
  if (event.type != SkillVfxEventType::TriggerProc || event.castId == 0u) {
    return false;
  }

  const auto quantize = [](const float value) -> int64_t {
    return static_cast<int64_t>(std::llround(value * 0.1f));
  };

  const int64_t qx = quantize(event.target.x);
  const int64_t qy = quantize(event.target.y);
  size_t key = 0xcbf29ce484222325ull;
  const auto hashCombine = [&key](const size_t v) {
    key ^= v + 0x9e3779b97f4a7c15ull + (key << 6) + (key >> 2);
  };
  hashCombine(static_cast<size_t>(event.skillId));
  hashCombine(static_cast<size_t>(event.castId));
  hashCombine(static_cast<size_t>(qx));
  hashCombine(static_cast<size_t>(qy));

  const auto [it, inserted] = m_triggerDedupKeys.insert(key);
  (void)it;
  return !inserted;
}

bool GPUSkillEffectSystem::QueueDistortion(const float worldX,
                                           const float worldY,
                                           const float radius,
                                           const float strength) {
  if (radius <= 1e-3f || strength <= 1e-3f ||
      m_pendingDistortion.size() >= kMaxQueuedDistortion) {
    return false;
  }
  m_pendingDistortion.push_back(
      DistortionRequest{worldX, worldY, radius, strength});
  return true;
}

void GPUSkillEffectSystem::LoadBuiltinRecipes() {
  m_recipes.clear();

  auto makeSelector = [](const int skillId, const SkillVfxEventType eventType) {
    SkillVfxRecipeSelector selector = {};
    selector.skillId = skillId;
    selector.eventType = static_cast<int>(eventType);
    selector.elementType = -1;
    selector.resistDebuffType = -1;
    selector.requiredNodeRoleMask = SkillVfxNodeRoleMask::None;
    return selector;
  };
  auto makeAction = [](const RecipeActionKind kind, const int count,
                       const float radius, const float angle,
                       const float softness, const float type,
                       const float speed, const float alpha,
                       const float spread, const float distortionStrength,
                       const float width, const float lifetime,
                       const float trailLength) {
    SkillVfxRecipeAction action = {};
    action.kind = kind;
    action.count = count;
    action.radius = radius;
    action.angle = angle;
    action.softness = softness;
    action.type = type;
    action.speed = speed;
    action.alpha = alpha;
    action.spread = spread;
    action.distortionStrength = distortionStrength;
    action.width = width;
    action.lifetime = lifetime;
    action.trailLength = trailLength;
    return action;
  };

  SkillVfxRecipe castStart = {};
  castStart.name = "skill1_cast_start";
  castStart.priority = 100;
  castStart.selector = makeSelector(1, SkillVfxEventType::CastStart);
  castStart.actions.push_back(makeAction(RecipeActionKind::TrailStroke, 3, 10.0f,
                                         360.0f, 0.35f, 2.0f, 260.0f, 0.8f, 0.0f,
                                         0.1f, 9.0f, 0.2f, 0.2f));
  castStart.actions.push_back(makeAction(RecipeActionKind::Overlay, 1, 14.0f,
                                         24.0f, 0.35f, 2.0f, 260.0f, 0.75f, 0.0f,
                                         0.1f, 8.0f, 0.2f, 0.2f));
  m_recipes.push_back(castStart);

  SkillVfxRecipe castImpact = {};
  castImpact.name = "skill1_cast_impact";
  castImpact.priority = 100;
  castImpact.selector = makeSelector(1, SkillVfxEventType::CastImpact);
  castImpact.actions.push_back(makeAction(RecipeActionKind::Overlay, 1, 22.0f,
                                          360.0f, 0.45f, 1.0f, 120.0f, 1.0f, 0.0f,
                                          0.1f, 8.0f, 0.2f, 0.2f));
  castImpact.actions.push_back(makeAction(RecipeActionKind::ParticleBurst, 12, 20.0f,
                                          360.0f, 0.35f, 1.0f, 160.0f, 0.75f,
                                          24.0f, 0.1f, 6.0f, 0.28f, 0.2f));
  m_recipes.push_back(castImpact);

  SkillVfxRecipe triggerProc = {};
  triggerProc.name = "skill1_trigger_proc";
  triggerProc.priority = 100;
  triggerProc.selector = makeSelector(1, SkillVfxEventType::TriggerProc);
  triggerProc.actions.push_back(makeAction(RecipeActionKind::Overlay, 1, 15.0f,
                                           18.0f, 0.30f, 2.0f, 320.0f, 0.85f,
                                           0.0f, 0.1f, 8.0f, 0.2f, 0.2f));
  triggerProc.actions.push_back(makeAction(RecipeActionKind::ParticleBurst, 8, 14.0f,
                                           360.0f, 0.35f, 1.0f, 220.0f, 0.65f,
                                           20.0f, 0.1f, 6.0f, 0.24f, 0.2f));
  m_recipes.push_back(triggerProc);

  SkillVfxRecipe empowered = {};
  empowered.name = "skill1_empowered_consume";
  empowered.priority = 100;
  empowered.selector = makeSelector(1, SkillVfxEventType::EmpoweredConsume);
  empowered.actions.push_back(makeAction(RecipeActionKind::Overlay, 1, 26.0f,
                                         360.0f, 0.45f, 1.0f, 80.0f, 1.0f, 0.0f,
                                         0.1f, 8.0f, 0.2f, 0.2f));
  empowered.actions.push_back(makeAction(RecipeActionKind::DistortionPulse, 1,
                                         36.0f, 360.0f, 0.35f, 1.0f, 80.0f, 1.0f,
                                         0.0f, 0.22f, 8.0f, 0.2f, 0.2f));
  m_recipes.push_back(empowered);
}

void GPUSkillEffectSystem::LoadSkillVfxRecipes(const std::string &path) {
  m_recipes.clear();

  std::ifstream input(path);
  if (!input.good()) {
    LOG_WARN("SkillVFX recipe config missing at '{}', using builtin defaults",
             path);
    LoadBuiltinRecipes();
    return;
  }

  try {
    const nlohmann::json root = nlohmann::json::parse(input);
    if (!root.contains("recipes") || !root["recipes"].is_array()) {
      LOG_WARN("SkillVFX recipe config '{}' missing 'recipes' array", path);
      LoadBuiltinRecipes();
      return;
    }

    for (const auto &entry : root["recipes"]) {
      if (!entry.is_object()) {
        continue;
      }
      SkillVfxRecipe recipe = {};
      recipe.name = entry.value("name", std::string("unnamed"));
      recipe.priority = entry.value("priority", 0);

      const nlohmann::json selector =
          entry.value("selector", nlohmann::json::object());
      recipe.selector.skillId = ParseSelectorInt(
          selector.contains("skillId") ? selector["skillId"] : nlohmann::json(), -1);
      recipe.selector.eventType = ParseSelectorInt(
          selector.contains("eventType") ? selector["eventType"] : nlohmann::json(),
          -1);
      recipe.selector.elementType = ParseSelectorInt(
          selector.contains("elementType") ? selector["elementType"]
                                           : nlohmann::json(),
          -1);
      recipe.selector.resistDebuffType = ParseSelectorInt(
          selector.contains("resistDebuffType") ? selector["resistDebuffType"]
                                                : nlohmann::json(),
          -1);
      recipe.selector.requiredNodeRoleMask = SkillVfxNodeRoleMask::None;
      if (selector.contains("requiredNodeRoleMask")) {
        const nlohmann::json &roleMaskValue = selector["requiredNodeRoleMask"];
        if (roleMaskValue.is_number_unsigned()) {
          recipe.selector.requiredNodeRoleMask = roleMaskValue.get<uint32_t>();
        } else if (roleMaskValue.is_array()) {
          uint32_t mask = SkillVfxNodeRoleMask::None;
          for (const auto &item : roleMaskValue) {
            if (!item.is_string()) {
              continue;
            }
            const std::string token = item.get<std::string>();
            if (token == "Keystone") {
              mask |= SkillVfxNodeRoleMask::Keystone;
            } else if (token == "Trigger") {
              mask |= SkillVfxNodeRoleMask::Trigger;
            } else if (token == "Synergy") {
              mask |= SkillVfxNodeRoleMask::Synergy;
            } else if (token == "Transmuter") {
              mask |= SkillVfxNodeRoleMask::Transmuter;
            }
          }
          recipe.selector.requiredNodeRoleMask = mask;
        }
      }

      if (!entry.contains("actions") || !entry["actions"].is_array()) {
        continue;
      }
      for (const auto &actionJson : entry["actions"]) {
        if (!actionJson.is_object()) {
          continue;
        }
        SkillVfxRecipeAction action = {};
        action.kind = ParseActionKind(actionJson.contains("kind")
                                          ? actionJson["kind"]
                                          : nlohmann::json("Overlay"));
        action.count = std::max(1, actionJson.value("count", 1));
        action.radius = std::max(0.1f, actionJson.value("radius", 16.0f));
        action.angle = actionJson.value("angle", 360.0f);
        action.softness = std::max(0.1f, actionJson.value("softness", 0.35f));
        action.type = actionJson.value("type", 1.0f);
        action.speed = actionJson.value("speed", 140.0f);
        action.alpha = std::clamp(actionJson.value("alpha", 1.0f), 0.05f, 1.0f);
        action.spread = std::max(0.0f, actionJson.value("spread", 0.0f));
        action.distortionStrength =
            std::max(0.0f, actionJson.value("distortionStrength", 0.1f));
        action.width = std::max(1.0f, actionJson.value("width", 8.0f));
        action.lifetime = std::max(0.03f, actionJson.value("lifetime", 0.2f));
        action.trailLength = std::max(0.03f, actionJson.value("trailLength", 0.2f));
        recipe.actions.push_back(action);
      }

      if (!recipe.actions.empty()) {
        m_recipes.push_back(std::move(recipe));
      }
    }
  } catch (const std::exception &ex) {
    LOG_WARN("SkillVFX recipe parse failed for '{}': {}", path, ex.what());
    LoadBuiltinRecipes();
    return;
  }

  if (m_recipes.empty()) {
    LOG_WARN("SkillVFX recipe config '{}' has no valid entries; using builtin",
             path);
    LoadBuiltinRecipes();
    return;
  }

  LOG_INFO("SkillVFX recipe system loaded {} recipes from '{}'", m_recipes.size(),
           path);
}

bool GPUSkillEffectSystem::EmitRecipeDrivenVisual(const SkillVfxEvent &event) {
  if (m_recipes.empty()) {
    return false;
  }

  SkillVfxEvent normalized = event;
  if (normalized.resistDebuffType >
      static_cast<uint8_t>(SkillVfxResistDebuffType::TypeE)) {
    normalized.resistDebuffType =
        static_cast<uint8_t>(SkillVfxResistDebuffType::None);
  }

  const SkillVfxRecipe *matched = nullptr;
  int bestPriority = std::numeric_limits<int>::min();
  int bestRolePriority = std::numeric_limits<int>::min();
  int bestSpecificity = std::numeric_limits<int>::min();

  for (const SkillVfxRecipe &recipe : m_recipes) {
    const SkillVfxRecipeSelector &selector = recipe.selector;
    if (selector.skillId != -1 && static_cast<uint32_t>(selector.skillId) !=
                                      normalized.skillId) {
      continue;
    }
    if (selector.eventType != -1 &&
        selector.eventType != static_cast<int>(normalized.type)) {
      continue;
    }
    if (selector.elementType != -1 &&
        selector.elementType != static_cast<int>(normalized.elementType)) {
      continue;
    }
    if (selector.resistDebuffType != -1 &&
        selector.resistDebuffType !=
            static_cast<int>(normalized.resistDebuffType)) {
      continue;
    }
    if (selector.requiredNodeRoleMask != SkillVfxNodeRoleMask::None &&
        (normalized.nodeRoleMask & selector.requiredNodeRoleMask) !=
            selector.requiredNodeRoleMask) {
      continue;
    }

    int specificity = 0;
    specificity += (selector.skillId == -1) ? 0 : 5;
    specificity += (selector.eventType == -1) ? 0 : 4;
    specificity += (selector.elementType == -1) ? 0 : 3;
    specificity += (selector.resistDebuffType == -1) ? 0 : 2;
    specificity +=
        (selector.requiredNodeRoleMask == SkillVfxNodeRoleMask::None) ? 0 : 1;
    const int rolePriority =
        ResolveRoleMaskPriority(selector.requiredNodeRoleMask);

    if (!matched || recipe.priority > bestPriority ||
        (recipe.priority == bestPriority && rolePriority > bestRolePriority) ||
        (recipe.priority == bestPriority &&
         rolePriority == bestRolePriority &&
         specificity > bestSpecificity)) {
      matched = &recipe;
      bestPriority = recipe.priority;
      bestRolePriority = rolePriority;
      bestSpecificity = specificity;
    }
  }

  if (!matched) {
    return false;
  }

  const VfxFallbackPolicy policy = BuildVfxFallbackPolicy(normalized);
  int cap = ResolveSkillCap(normalized.skillId, policy.tier);
  if (normalized.type == SkillVfxEventType::TriggerProc) {
    cap = std::min(cap, ResolveTriggerCap(normalized.skillId, policy.tier));
  }
  if (cap <= 0) {
    return true;
  }

  float triggerActionScale = 1.0f;
  float triggerIntensityScale = 1.0f;
  if (!ConsumeTriggerBudget(normalized, policy.tier, triggerActionScale,
                            triggerIntensityScale)) {
    return true;
  }
  if (normalized.type == SkillVfxEventType::TriggerProc) {
    const bool hasKeystone =
        HasSkillVfxNodeRole(normalized.nodeRoleMask, SkillVfxNodeRoleMask::Keystone);
    const bool hasTrigger =
        HasSkillVfxNodeRole(normalized.nodeRoleMask, SkillVfxNodeRoleMask::Trigger);
    const bool hasSynergy =
        HasSkillVfxNodeRole(normalized.nodeRoleMask, SkillVfxNodeRoleMask::Synergy);
    if (hasSynergy && !hasTrigger && !hasKeystone) {
      triggerActionScale *= 0.72f;
      triggerIntensityScale *= 0.86f;
    }
  }

  const Color baseColor =
      ResolveElementColor(normalized.elementType, ResolveSkillColor(normalized.skillId));
  const Vector2 direction =
      NormalizedDirection(normalized.origin, normalized.target);
  const float intensity =
      ClampIntensity(normalized.intensity * triggerIntensityScale);

  auto emitOverlay = [&](const SkillVfxRecipeAction &action, const Vector2 pos,
                         const Vector2 vel) {
    components::GPUSkillEffect effect = {};
    effect.position = pos;
    effect.velocity = vel;
    effect.radius = std::max(2.0f, action.radius * intensity);
    effect.sectorAngle = action.angle;
    effect.flags = EncodeSkillEffectFlags(normalized.elementType,
                                          normalized.skillId);
    if (normalized.skillId == 0u) {
      LOG_LIMITED_WARN(1.0f,
                       "GPUSkillEffectSystem: recipe overlay missing skillId, "
                       "using fallback routing");
    }
    effect.type = action.type;

    const float alpha = std::clamp(action.alpha * triggerActionScale, 0.05f, 1.0f);
    const float glowAlpha = policy.allowSecondaryGlow ? 0.45f * alpha : 0.0f;
    effect.coreColor = ColorNormalize(ColorAlpha(baseColor, alpha));
    effect.glowColor = ColorNormalize(ColorAlpha(WHITE, glowAlpha));
    TrySubmitCapped(normalized.skillId, cap, effect);
  };

  auto emitDistortion = [&](const SkillVfxRecipeAction &action,
                            const Vector2 center) {
    if (!policy.allowDistortion) {
      return;
    }
    QueueDistortion(center.x, center.y, action.radius * intensity,
                    action.distortionStrength);
  };

  auto emitParticles = [&](const SkillVfxRecipeAction &action) {
    if (!policy.allowEnvironmentParticles ||
        !GPUParticleSystem::Get().IsInitialized()) {
      return;
    }
    const int count = std::max(1, action.count);
    std::vector<components::GPUParticle> particles;
    particles.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      const float ratio =
          (count == 1) ? 0.5f : static_cast<float>(i) / static_cast<float>(count - 1);
      const Vector2 samplePos =
          Vector2Lerp(normalized.origin, normalized.target, ratio);
      const float spreadX =
          static_cast<float>(GetRandomValue(-100, 100)) * (action.spread / 100.0f);
      const float spreadY =
          static_cast<float>(GetRandomValue(-100, 100)) * (action.spread / 100.0f);

      components::GPUParticle particle = {};
      particle.position = {samplePos.x + spreadX, samplePos.y + spreadY};
      particle.velocity = Vector2Scale(direction, action.speed * intensity);
      particle.acceleration = {0.0f, 0.0f};
      particle.maxLifetime = action.lifetime;
      particle.lifetime = action.lifetime;
      particle.scale = std::max(2.0f, action.radius * 0.25f);
      particle.growthRate = -particle.scale / std::max(action.lifetime, 0.05f);
      particle.color =
          ColorAlpha(baseColor, std::clamp(action.alpha * triggerActionScale,
                                           0.05f, 1.0f));
      particle.blendMode = 1;
      particles.push_back(particle);
    }
    GPUParticleSystem::Get().EmitBatch(particles);
  };

  auto emitTrail = [&](const SkillVfxRecipeAction &action) {
    if (!policy.allowTrailStroke) {
      return;
    }
    auto &trailRenderer = render::GPUTrailRenderer::Get();
    if (!trailRenderer.IsInitialized()) {
      return;
    }
    components::GPUTrailHeader header = {};
    header.maxPoints = std::max(2, 8 / std::max(1, policy.trailSampleStride));
    header.maxLifetime = std::max(0.05f, action.trailLength);
    header.widthStart = action.width;
    header.widthEnd = std::max(1.0f, action.width * 0.45f);
    const float trailAlpha =
        std::clamp(action.alpha * triggerActionScale, 0.05f, 1.0f);
    header.colorStart =
        static_cast<uint32_t>(ColorToInt(ColorAlpha(baseColor, trailAlpha)));
    header.colorEnd =
        static_cast<uint32_t>(ColorToInt(ColorAlpha(baseColor, 0.0f)));

    const int trailId = trailRenderer.AllocateTrail(header);
    if (trailId < 0) {
      return;
    }

    const Vector2 step = Vector2Scale(direction, action.speed * 0.012f);
    const int pointCount = std::max(2, 4 / std::max(1, policy.trailSampleStride));
    for (int i = 0; i < pointCount; ++i) {
      const Vector2 pos = {
          normalized.origin.x + step.x * static_cast<float>(i),
          normalized.origin.y + step.y * static_cast<float>(i),
      };
      trailRenderer.AppendPoint(
          trailId, pos, direction,
          std::max(1.0f, action.width * (1.0f - 0.15f * static_cast<float>(i))),
          static_cast<uint32_t>(ColorToInt(ColorAlpha(baseColor, trailAlpha))));
    }
  };

  auto resolveScaledCount = [&](const SkillVfxRecipeAction &action) {
    const int scaledCount = std::max(
        1, static_cast<int>(std::round(static_cast<float>(action.count) *
                                       policy.particleEmissionScale *
                                       triggerActionScale)));
    return (cap > 0) ? std::min(scaledCount, cap) : scaledCount;
  };

  auto emitResistOverlay = [&](const SkillVfxRecipeAction &action,
                               const Vector2 &pos) {
    if (normalized.resistDebuffType ==
        static_cast<uint8_t>(SkillVfxResistDebuffType::None)) {
      return;
    }
    ResistOverlayRequest request = {};
    request.worldPos = pos;
    request.resistDebuffType = normalized.resistDebuffType;
    request.intensity = std::clamp(action.alpha * intensity, 0.1f, 2.0f);
    m_pendingResistOverlay.push_back(request);
  };

  for (const SkillVfxRecipeAction &action : matched->actions) {
    const int count = resolveScaledCount(action);
    if (action.kind == RecipeActionKind::ParticleBurst) {
      SkillVfxRecipeAction scaledAction = action;
      scaledAction.count = count;
      emitParticles(scaledAction);
      continue;
    }

    for (int i = 0; i < count; ++i) {
      const float t = (count == 1) ? 0.5f
                                   : static_cast<float>(i + 1) /
                                         static_cast<float>(count + 1);
      const Vector2 pos = Vector2Lerp(normalized.origin, normalized.target, t);
      const Vector2 vel = Vector2Scale(direction, action.speed);
      switch (action.kind) {
      case RecipeActionKind::Overlay:
        emitOverlay(action, pos, vel);
        break;
      case RecipeActionKind::TrailStroke:
        emitTrail(action);
        break;
      case RecipeActionKind::DistortionPulse:
        emitDistortion(action, pos);
        break;
      case RecipeActionKind::ResistOverlay:
        emitResistOverlay(action, pos);
        break;
      case RecipeActionKind::ParticleBurst:
        break;
      }
    }
  }

  return true;
}

void GPUSkillEffectSystem::EmitSkillEventVisual(const SkillVfxEvent &event) {
  if (EmitRecipeDrivenVisual(event)) {
    return;
  }
  EmitLegacySkillEventVisual(event);
}

void GPUSkillEffectSystem::EmitLegacySkillEventVisual(
    const SkillVfxEvent &event) {
  auto &qualityManager = render::core::QualityTierManager::Get();
  (void)qualityManager;
  const VfxFallbackPolicy policy = BuildVfxFallbackPolicy(event);
  const bool reduceEmission = policy.particleEmissionScale < 0.99f;
  const bool reduceTrailSampling = policy.trailSampleStride > 1;
  const bool disableSecondaryGlow = !policy.allowSecondaryGlow;
  const bool allowDistortion = policy.allowDistortion;

  int cap = ResolveSkillCap(event.skillId, policy.tier);
  if (event.type == SkillVfxEventType::TriggerProc) {
    cap = std::min(cap, ResolveTriggerCap(event.skillId, policy.tier));
  }
  if (cap <= 0) {
    return;
  }

  float triggerActionScale = 1.0f;
  float triggerIntensityScale = 1.0f;
  if (!ConsumeTriggerBudget(event, policy.tier, triggerActionScale,
                            triggerIntensityScale)) {
    return;
  }
  if (event.type == SkillVfxEventType::TriggerProc) {
    const bool hasKeystone =
        HasSkillVfxNodeRole(event.nodeRoleMask, SkillVfxNodeRoleMask::Keystone);
    const bool hasTrigger =
        HasSkillVfxNodeRole(event.nodeRoleMask, SkillVfxNodeRoleMask::Trigger);
    const bool hasSynergy =
        HasSkillVfxNodeRole(event.nodeRoleMask, SkillVfxNodeRoleMask::Synergy);
    if (hasSynergy && !hasTrigger && !hasKeystone) {
      triggerActionScale *= 0.72f;
      triggerIntensityScale *= 0.86f;
    }
  }

  const uint8_t elementType = event.elementType;
  const Color baseColor =
      ResolveElementColor(elementType, ResolveSkillColor(event.skillId));
  const Vector2 direction = NormalizedDirection(event.origin, event.target);
  const float intensity = ClampIntensity(event.intensity * triggerIntensityScale);

  auto emitEffect = [&](Vector2 pos, Vector2 vel, float radius, float angle,
                        float softness, float type, float alphaScale) {
    (void)softness;
    components::GPUSkillEffect effect = {};
    effect.position = pos;
    effect.velocity = vel;
    effect.radius = std::max(2.0f, radius);
    effect.sectorAngle = angle;
    effect.flags = EncodeSkillEffectFlags(elementType, event.skillId);
    if (event.skillId == 0u) {
      LOG_LIMITED_WARN(1.0f,
                       "GPUSkillEffectSystem: legacy effect missing skillId, "
                       "using fallback routing");
    }
    effect.type = type;

    const float coreAlpha =
        std::clamp(0.9f * alphaScale * triggerActionScale, 0.25f, 1.0f);
    const float glowAlpha =
        disableSecondaryGlow ? 0.0f : 0.5f * alphaScale * triggerActionScale;
    effect.coreColor = ColorNormalize(ColorAlpha(baseColor, coreAlpha));
    effect.glowColor = ColorNormalize(ColorAlpha(WHITE, glowAlpha));
    TrySubmitCapped(event.skillId, cap, effect);
  };

  auto emitDistortion = [&](Vector2 center, float radius, float strength) {
    if (!allowDistortion) {
      return;
    }
    QueueDistortion(center.x, center.y, radius, strength);
  };

  if (event.resistDebuffType !=
          static_cast<uint8_t>(SkillVfxResistDebuffType::None) &&
      (event.type == SkillVfxEventType::CastImpact ||
       event.type == SkillVfxEventType::TriggerProc)) {
    ResistOverlayRequest request = {};
    request.worldPos = event.target;
    request.resistDebuffType = event.resistDebuffType;
    request.intensity = std::clamp(intensity, 0.1f, 2.0f);
    m_pendingResistOverlay.push_back(request);
  }

  if (event.type == SkillVfxEventType::TransmuterSwitch) {
    emitEffect(event.origin, {0.0f, 0.0f}, 26.0f * intensity, 360.0f, 0.5f,
               1.0f, 0.75f);
    emitEffect(event.origin, Vector2Scale(direction, 180.0f), 16.0f * intensity,
               24.0f, 0.3f, 2.0f, 0.65f);
    return;
  }
  if (event.type == SkillVfxEventType::KeystoneActivate) {
    emitEffect(event.origin, {0.0f, 0.0f}, 32.0f * intensity, 360.0f, 0.55f,
               1.0f, 0.95f);
    emitDistortion(event.origin, 28.0f * intensity, 0.18f);
    return;
  }

  switch (event.skillId) {
  case 1: {
    if (event.type == SkillVfxEventType::CastStart && !reduceTrailSampling) {
      constexpr int trailCount = 3;
      for (int i = 0; i < trailCount; ++i) {
        const float t = static_cast<float>(i + 1) /
                        static_cast<float>(trailCount + 1);
        const Vector2 pos = Vector2Lerp(event.origin, event.target, t);
        emitEffect(pos, Vector2Scale(direction, 260.0f), 12.0f * intensity, 24.0f,
                   0.35f, 2.0f, 0.75f);
      }
    }
    if (event.type == SkillVfxEventType::CastImpact) {
      emitEffect(event.target, Vector2Scale(direction, 120.0f), 20.0f * intensity,
                 360.0f, 0.45f, 1.0f, 1.0f);
    }
    if (event.type == SkillVfxEventType::TriggerProc) {
      emitEffect(event.target, Vector2Scale(direction, 320.0f), 14.0f * intensity,
                 18.0f, 0.3f, 2.0f, 0.8f);
    }
    return;
  }
  case 2: {
    if (event.type == SkillVfxEventType::CastImpact) {
      const int bladeSamples = reduceTrailSampling ? 1 : 2;
      for (int i = 0; i < bladeSamples; ++i) {
        const float t = (bladeSamples == 1)
                            ? 0.82f
                            : (0.68f + 0.16f * static_cast<float>(i));
        const Vector2 samplePos = Vector2Lerp(event.origin, event.target, t);
        const float sampleRadius =
            (reduceTrailSampling ? 8.0f : (7.0f + 0.8f * static_cast<float>(i))) *
            intensity;
        emitEffect(samplePos, Vector2Scale(direction, 220.0f + 20.0f * i),
                   sampleRadius, 34.0f, 0.28f, 2.0f, 0.5f);
      }

      emitEffect(event.target, Vector2Scale(direction, 180.0f), 12.0f * intensity,
                 40.0f, 0.28f, 3.0f, 0.52f);
      emitDistortion(event.target, 18.0f * intensity, 0.10f);
    } else if (event.type == SkillVfxEventType::TriggerProc) {
      emitEffect(event.target, Vector2Scale(direction, 170.0f), 10.0f * intensity,
                 36.0f, 0.28f, 3.0f, 0.42f);
      const Vector2 trailPos = Vector2Lerp(event.origin, event.target, 0.76f);
      emitEffect(trailPos, Vector2Scale(direction, 190.0f), 6.0f * intensity,
                 30.0f, 0.24f, 2.0f, 0.36f);
    }
    return;
  }
  case 3: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::BuffEnter) {
      const int orbitCount = reduceEmission ? 8 : 12;
      for (int i = 0; i < orbitCount; ++i) {
        const float angle = (2.0f * PI * static_cast<float>(i)) /
                            static_cast<float>(orbitCount);
        const float radius = 34.0f + ((i % 2 == 0) ? 6.0f : 0.0f);
        Vector2 pos = {event.origin.x + std::cos(angle) * radius,
                       event.origin.y + std::sin(angle) * radius};
        Vector2 vel = {std::cos(angle) * 100.0f, std::sin(angle) * 100.0f};
        emitEffect(pos, vel, 10.0f, 24.0f, 0.35f, 2.0f, 0.65f);
      }
    }
    if (event.type == SkillVfxEventType::TriggerProc ||
        event.type == SkillVfxEventType::CastImpact) {
      emitEffect(event.target, Vector2Scale(direction, 80.0f), 20.0f * intensity,
                 360.0f, 0.5f, 1.0f, 0.8f);
    }
    return;
  }
  case 4: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::BuffEnter) {
      emitEffect(event.origin, {0.0f, 0.0f}, 30.0f * intensity, 360.0f, 0.45f,
                 1.0f, 0.85f);
    }
    if (event.type == SkillVfxEventType::TriggerProc ||
        event.type == SkillVfxEventType::CastImpact) {
      emitEffect(event.target, Vector2Scale(direction, 200.0f), 12.0f * intensity,
                 20.0f, 0.3f, 2.0f, 0.9f);
    }
    return;
  }
  case 5: {
    if (event.type == SkillVfxEventType::CastStart) {
      emitEffect(event.target, {0.0f, 0.0f}, 82.0f * intensity, 360.0f, 0.6f,
                 1.0f, 0.55f);
    }
    if (event.type == SkillVfxEventType::CastImpact) {
      const int rainCount = reduceEmission ? 64 : 192;
      for (int i = 0; i < rainCount; ++i) {
        const float degrees = static_cast<float>(GetRandomValue(0, 359));
        const float radians = degrees * DEG2RAD;
        const float distance = static_cast<float>(GetRandomValue(10, 120));
        Vector2 pos = {event.target.x + std::cos(radians) * distance,
                       event.target.y + std::sin(radians) * distance};
        emitEffect(pos, {0.0f, 280.0f}, 10.0f, 18.0f, 0.32f, 2.0f, 0.6f);
      }
    }
    if (event.type == SkillVfxEventType::EmpoweredConsume) {
      emitEffect(event.origin, {0.0f, 0.0f}, 36.0f * intensity, 360.0f, 0.4f,
                 1.0f, 1.0f);
      emitDistortion(event.origin, 44.0f * intensity, 0.4f);
    }
    return;
  }
  case 6: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::BuffEnter) {
      emitEffect(event.origin, {0.0f, 0.0f}, 64.0f * intensity, 360.0f, 0.55f,
                 1.0f, 0.75f);
      emitDistortion(event.origin, 68.0f * intensity, 0.24f);
    }
    if (event.type == SkillVfxEventType::TriggerProc && !reduceEmission) {
      Vector2 flashPos = {event.target.x + static_cast<float>(GetRandomValue(-40, 40)),
                          event.target.y + static_cast<float>(GetRandomValue(-40, 40))};
      emitEffect(flashPos, Vector2Scale(direction, 180.0f), 13.0f, 22.0f, 0.3f,
                 2.0f, 0.65f);
    }
    return;
  }
  case 7: {
    if (event.type == SkillVfxEventType::CastImpact ||
        event.type == SkillVfxEventType::TriggerProc) {
      const int beamSamples = reduceTrailSampling ? 1 : 3;
      for (int i = 0; i < beamSamples; ++i) {
        const float t = (beamSamples == 1) ? 0.5f
                                            : static_cast<float>(i) /
                                                  static_cast<float>(beamSamples - 1);
        const Vector2 pos = Vector2Lerp(event.origin, event.target, t);
        emitEffect(pos, Vector2Scale(direction, 520.0f),
                   reduceTrailSampling ? 14.0f : 11.0f, 10.0f,
                   reduceTrailSampling ? 0.5f : 0.28f, 2.0f, 1.0f);
      }
    }
    return;
  }
  case 8: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::CastImpact) {
      emitEffect(event.origin, Vector2Scale(direction, 240.0f), 18.0f * intensity,
                 360.0f, 0.45f, 1.0f, 0.95f);

      const int trailSamples = reduceTrailSampling ? 1 : 4;
      for (int i = 0; i < trailSamples; ++i) {
        const float t = static_cast<float>(i + 1) /
                        static_cast<float>(trailSamples + 1);
        const Vector2 pos = Vector2Lerp(event.origin, event.target, t);
        emitEffect(pos, Vector2Scale(direction, 240.0f), 10.0f, 22.0f, 0.45f,
                   2.0f, 0.55f);
      }
    }
    if (event.type == SkillVfxEventType::TriggerProc ||
        event.type == SkillVfxEventType::BuffExit) {
      emitEffect(event.target, {0.0f, 0.0f}, 24.0f * intensity, 360.0f, 0.55f,
                 1.0f, 0.8f);
    }
    return;
  }
  case 9: {
    if (event.type == SkillVfxEventType::CastStart ||
        event.type == SkillVfxEventType::BuffEnter) {
      emitEffect(event.origin, {0.0f, 0.0f}, 28.0f * intensity, 360.0f, 0.5f,
                 1.0f, 0.75f);
    }
    if (event.type == SkillVfxEventType::TriggerProc) {
      emitEffect(event.target, Vector2Scale(direction, 260.0f), 16.0f * intensity,
                 20.0f, 0.34f, 2.0f, 0.9f);
    }
    if (event.type == SkillVfxEventType::CastImpact ||
        event.type == SkillVfxEventType::BuffExit) {
      emitEffect(event.origin, {0.0f, 0.0f}, 34.0f * intensity, 360.0f, 0.6f,
                 1.0f, 1.0f);
    }
    return;
  }
  default:
    return;
  }
}

void GPUSkillEffectSystem::StageSkillEvents() {
  if (m_pendingEvents.empty()) {
    return;
  }
  for (const SkillVfxEvent &event : m_pendingEvents) {
    if (ShouldCullDuplicateTrigger(event)) {
      continue;
    }
    EmitSkillEventVisual(event);
  }
  m_pendingEvents.clear();
}

void GPUSkillEffectSystem::Render(const Camera2D &camera) {
  (void)camera;
  m_skillFrameCounts.fill(0);
  m_triggerFrameCounts.fill(0);
  m_triggerCarryBlend.fill(0.0f);
  m_triggerDedupKeys.clear();
  StageSkillEvents();
  if (m_currentCount == 0 || m_shader.id == 0) {
    m_currentCount = 0;
    return;
  }

  m_gpuBuffer.Update(m_hostBuffer.data(),
                     m_currentCount * sizeof(components::GPUSkillEffect));

  rlDrawRenderBatchActive();
  Matrix mvp = rlGetMatrixModelview();
  Matrix projection = rlGetMatrixProjection();
  Matrix finalMvp = MatrixMultiply(mvp, projection);

  BeginBlendMode(BLEND_ALPHA);
  rlDisableDepthTest();
  rlDisableBackfaceCulling();

  rlEnableShader(m_shader.id);
  rlSetUniformMatrix(m_shader.locs[SHADER_LOC_MATRIX_MVP], finalMvp);
  if (m_timeLoc >= 0) {
    const float timeSeconds = static_cast<float>(GetTime());
    rlSetUniform(m_timeLoc, &timeSeconds, SHADER_UNIFORM_FLOAT, 1);
  }

  using NoMoreDay::RenderConstants::Binding;
  m_gpuBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_SKILL_EFFECTS));

  rlEnableVertexArray(m_quadVAO);
  rlDrawVertexArrayInstanced(0, 6, m_currentCount);
  rlDisableVertexArray();

  rlDisableShader();
  EndBlendMode();
  m_currentCount = 0;
}

void GPUSkillEffectSystem::Shutdown() {
  LOG_INFO("Shutting down GPUSkillEffectSystem...");
  m_gpuBuffer.Release();
  UnloadShader(m_shader);
  if (m_quadVAO != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_quadVAO, NoMoreDay::render::graph::ResourceKind::VertexArray);
    rlUnloadVertexArray(m_quadVAO);
  }
  if (m_quadVBO != 0) {
    NoMoreDay::render::resources::GPUResourceRegistry::Get().UnregisterResource(
        m_quadVBO, NoMoreDay::render::graph::ResourceKind::VertexBuffer);
    rlUnloadVertexBuffer(m_quadVBO);
  }

  m_shader.id = 0;
  m_timeLoc = -1;
  m_quadVAO = 0;
  m_quadVBO = 0;
  m_maxEffects = 0;
  m_currentCount = 0;
  m_hostBuffer.clear();
  m_pendingEvents.clear();
  m_pendingDistortion.clear();
  m_pendingResistOverlay.clear();
  m_skillFrameCounts.fill(0);
  m_triggerFrameCounts.fill(0);
  m_triggerCarryBlend.fill(0.0f);
  m_triggerDedupKeys.clear();
  m_recipes.clear();
}

} // namespace NoMoreDay::systems
