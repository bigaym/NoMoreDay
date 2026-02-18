#include "engine/vfx/VFXSequenceManager.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/MaterialManager.hpp"
#include "engine/render/core/QualityTierManager.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

#include <nlohmann/json.hpp>

namespace NoMoreDay::vfx {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

render::core::QualityTier
ParseQualityTierString(const std::string &tierName,
                       render::core::QualityTier fallback) {
  const std::string lower = ToLower(tierName);
  if (lower == "low") {
    return render::core::QualityTier::Low;
  }
  if (lower == "medium") {
    return render::core::QualityTier::Medium;
  }
  if (lower == "high") {
    return render::core::QualityTier::High;
  }
  if (lower == "ultra") {
    return render::core::QualityTier::Ultra;
  }
  return fallback;
}

render::core::QualityTier
ParseQualityTier(const json &node, const char *key,
                 render::core::QualityTier fallback) {
  if (!node.contains(key) || !node[key].is_string()) {
    return fallback;
  }
  return ParseQualityTierString(node[key].get<std::string>(), fallback);
}

bool ParseTierPolicy(const json &eventNode, const fs::path &filePath,
                     TierPolicy &outPolicy) {
  if (!eventNode.contains("tierPolicy")) {
    outPolicy = TierPolicy::Skip;
    return true;
  }
  if (!eventNode["tierPolicy"].is_string()) {
    LOG_ERROR("VFXSequenceManager: invalid tierPolicy type in {}",
              filePath.string());
    return false;
  }

  const std::string value = ToLower(eventNode["tierPolicy"].get<std::string>());
  if (value == "strict") {
    outPolicy = TierPolicy::Strict;
    return true;
  }
  if (value == "degrade") {
    outPolicy = TierPolicy::Degrade;
    return true;
  }
  if (value == "skip") {
    outPolicy = TierPolicy::Skip;
    return true;
  }

  LOG_ERROR("VFXSequenceManager: unsupported tierPolicy '{}' in {}", value,
            filePath.string());
  return false;
}

AnchorType ParseAnchorType(const json &eventNode) {
  if (!eventNode.contains("anchor") || !eventNode["anchor"].is_string()) {
    return AnchorType::Caster;
  }

  const std::string anchor = ToLower(eventNode["anchor"].get<std::string>());
  if (anchor == "target") {
    return AnchorType::Target;
  }
  if (anchor == "world") {
    return AnchorType::World;
  }
  if (anchor == "impact") {
    return AnchorType::Impact;
  }
  return AnchorType::Caster;
}

bool ParseEventType(const std::string &typeName, EventType &outType) {
  const std::string type = ToLower(typeName);
  if (type == "particle") {
    outType = EventType::Particle;
    return true;
  }
  if (type == "trail") {
    outType = EventType::Trail;
    return true;
  }
  if (type == "light") {
    outType = EventType::Light;
    return true;
  }
  if (type == "shake") {
    outType = EventType::Shake;
    return true;
  }
  if (type == "distortion") {
    outType = EventType::Distortion;
    return true;
  }
  if (type == "sound") {
    outType = EventType::Sound;
    return true;
  }
  if (type == "materialswap" || type == "material_swap") {
    outType = EventType::MaterialSwap;
    return true;
  }
  if (type == "shadowpulse" || type == "shadow_pulse") {
    outType = EventType::ShadowPulse;
    return true;
  }
  if (type == "lightprofileblend" || type == "light_profile_blend") {
    outType = EventType::LightProfileBlend;
    return true;
  }
  if (type == "materialphaseshift" || type == "material_phase_shift") {
    outType = EventType::MaterialPhaseShift;
    return true;
  }
  return false;
}

int ParseMaterialIdField(const json &paramsNode, const char *key) {
  if (!paramsNode.contains(key)) {
    return 0;
  }

  const json &value = paramsNode[key];
  if (value.is_number_integer()) {
    return value.get<int>();
  }
  if (value.is_string()) {
    const std::string materialName = value.get<std::string>();
    const int materialId =
        render::MaterialManager::Get().GetMaterialId(materialName);
    if (materialId >= 0) {
      return materialId;
    }
    LOG_WARN("VFXSequenceManager: unknown material '{}', fallback to 0",
             materialName);
  }
  return 0;
}

uint32_t ParseColorValue(const json &paramsNode, const char *key,
                         uint32_t fallback) {
  if (!paramsNode.contains(key)) {
    return fallback;
  }

  const json &value = paramsNode[key];
  if (value.is_number_unsigned()) {
    return value.get<uint32_t>();
  }
  if (value.is_number_integer()) {
    const int signedValue = value.get<int>();
    return signedValue < 0 ? fallback : static_cast<uint32_t>(signedValue);
  }
  if (value.is_string()) {
    try {
      return static_cast<uint32_t>(std::stoul(value.get<std::string>(), nullptr, 0));
    } catch (const std::exception &) {
      return fallback;
    }
  }
  return fallback;
}

uint32_t ParseUIntField(const json &paramsNode, const char *key, uint32_t fallback) {
  if (!paramsNode.contains(key)) {
    return fallback;
  }

  const json &value = paramsNode[key];
  if (value.is_number_unsigned()) {
    return value.get<uint32_t>();
  }
  if (value.is_number_integer()) {
    const int signedValue = value.get<int>();
    return signedValue < 0 ? fallback : static_cast<uint32_t>(signedValue);
  }
  return fallback;
}

EventParams ParseEventParams(EventType type, const json &paramsNode) {
  switch (type) {
  case EventType::Particle: {
    ParticleEventParams params;
    params.materialId = ParseMaterialIdField(paramsNode, "materialId");
    params.count = paramsNode.value("count", params.count);
    params.speed = paramsNode.value("speed", params.speed);
    params.speedVariance = paramsNode.value("speedVariance", params.speedVariance);
    params.lifetime = paramsNode.value("lifetime", params.lifetime);
    params.scale = paramsNode.value("scale", params.scale);
    params.spreadAngle = paramsNode.value("spreadAngle", params.spreadAngle);
    params.textureIndex = static_cast<int16_t>(
        paramsNode.value("textureIndex", static_cast<int>(params.textureIndex)));
    params.blendMode = static_cast<uint8_t>(
        paramsNode.value("blendMode", static_cast<int>(params.blendMode)));
    params.offsetX = paramsNode.value("offsetX", params.offsetX);
    params.offsetY = paramsNode.value("offsetY", params.offsetY);
    return params;
  }
  case EventType::Trail: {
    TrailEventParams params;
    params.materialId = ParseMaterialIdField(paramsNode, "materialId");
    params.duration = paramsNode.value("duration", params.duration);
    params.widthStart = paramsNode.value("widthStart", params.widthStart);
    params.widthEnd = paramsNode.value("widthEnd", params.widthEnd);
    params.colorStart = ParseColorValue(paramsNode, "colorStart", params.colorStart);
    params.colorEnd = ParseColorValue(paramsNode, "colorEnd", params.colorEnd);
    return params;
  }
  case EventType::Light: {
    LightEventParams params;
    params.radius = paramsNode.value("radius", params.radius);
    params.intensity = paramsNode.value("intensity", params.intensity);
    params.duration = paramsNode.value("duration", params.duration);
    params.fadeInRatio = paramsNode.value("fadeInRatio", params.fadeInRatio);
    params.fadeOutRatio = paramsNode.value("fadeOutRatio", params.fadeOutRatio);
    if (paramsNode.contains("color") && paramsNode["color"].is_array() &&
        paramsNode["color"].size() >= 3) {
      params.colorR = paramsNode["color"][0].get<float>();
      params.colorG = paramsNode["color"][1].get<float>();
      params.colorB = paramsNode["color"][2].get<float>();
    } else {
      params.colorR = paramsNode.value("colorR", params.colorR);
      params.colorG = paramsNode.value("colorG", params.colorG);
      params.colorB = paramsNode.value("colorB", params.colorB);
    }
    return params;
  }
  case EventType::Shake: {
    ShakeEventParams params;
    params.intensity = paramsNode.value("intensity", params.intensity);
    return params;
  }
  case EventType::Distortion: {
    DistortionEventParams params;
    params.radius = paramsNode.value("radius", params.radius);
    params.strength = paramsNode.value("strength", params.strength);
    params.duration = paramsNode.value("duration", params.duration);
    params.speed = paramsNode.value("speed", params.speed);
    return params;
  }
  case EventType::Sound: {
    SoundEventParams params;
    params.soundId = paramsNode.value("soundId", params.soundId);
    params.volume = paramsNode.value("volume", params.volume);
    params.pitch = paramsNode.value("pitch", params.pitch);
    return params;
  }
  case EventType::MaterialSwap: {
    MaterialSwapParams params;
    params.materialId = ParseMaterialIdField(paramsNode, "materialId");
    params.duration = paramsNode.value("duration", params.duration);
    return params;
  }
  case EventType::ShadowPulse: {
    ShadowPulseParams params;
    params.softnessScale = paramsNode.value("softnessScale", params.softnessScale);
    params.intensityScale = paramsNode.value("intensityScale", params.intensityScale);
    params.duration = paramsNode.value("duration", params.duration);
    return params;
  }
  case EventType::LightProfileBlend: {
    LightProfileBlendParams params;
    params.profileA = ParseUIntField(paramsNode, "profileA", params.profileA);
    params.profileB = ParseUIntField(paramsNode, "profileB", params.profileB);
    params.blendTime = paramsNode.value("blendTime", params.blendTime);
    return params;
  }
  case EventType::MaterialPhaseShift: {
    MaterialPhaseShiftParams params;
    params.roughnessScale = paramsNode.value("roughnessScale", params.roughnessScale);
    params.specularScale = paramsNode.value("specularScale", params.specularScale);
    params.emissiveScale = paramsNode.value("emissiveScale", params.emissiveScale);
    params.duration = paramsNode.value("duration", params.duration);
    return params;
  }
  case EventType::Count:
    break;
  }

  return ParticleEventParams{};
}

bool ValidateEventParams(const EventType type, const EventParams &params,
                         const fs::path &filePath) {
  switch (type) {
  case EventType::ShadowPulse: {
    const auto *parsed = std::get_if<ShadowPulseParams>(&params);
    if (parsed == nullptr) {
      LOG_ERROR("VFXSequenceManager: ShadowPulse payload decode failed in {}",
                filePath.string());
      return false;
    }
    if (parsed->duration <= 0.0f || parsed->softnessScale < 0.0f ||
        parsed->intensityScale < 0.0f) {
      LOG_ERROR(
          "VFXSequenceManager: ShadowPulse payload invalid in {} "
          "(duration={}, softnessScale={}, intensityScale={})",
          filePath.string(), parsed->duration, parsed->softnessScale,
          parsed->intensityScale);
      return false;
    }
    return true;
  }
  case EventType::LightProfileBlend: {
    const auto *parsed = std::get_if<LightProfileBlendParams>(&params);
    if (parsed == nullptr) {
      LOG_ERROR(
          "VFXSequenceManager: LightProfileBlend payload decode failed in {}",
          filePath.string());
      return false;
    }
    if (parsed->blendTime <= 0.0f) {
      LOG_ERROR(
          "VFXSequenceManager: LightProfileBlend payload invalid in {} "
          "(blendTime={})",
          filePath.string(), parsed->blendTime);
      return false;
    }
    return true;
  }
  case EventType::MaterialPhaseShift: {
    const auto *parsed = std::get_if<MaterialPhaseShiftParams>(&params);
    if (parsed == nullptr) {
      LOG_ERROR(
          "VFXSequenceManager: MaterialPhaseShift payload decode failed in {}",
          filePath.string());
      return false;
    }
    if (parsed->duration <= 0.0f || parsed->roughnessScale < 0.0f ||
        parsed->specularScale < 0.0f || parsed->emissiveScale < 0.0f) {
      LOG_ERROR(
          "VFXSequenceManager: MaterialPhaseShift payload invalid in {} "
          "(duration={}, roughnessScale={}, specularScale={}, emissiveScale={})",
          filePath.string(), parsed->duration, parsed->roughnessScale,
          parsed->specularScale, parsed->emissiveScale);
      return false;
    }
    return true;
  }
  default:
    return true;
  }
}

bool ParseSequenceFile(const fs::path &filePath, VFXSequenceAsset &outSequence) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    LOG_ERROR("VFXSequenceManager: failed to open {}", filePath.string());
    return false;
  }

  json document;
  try {
    file >> document;
  } catch (const std::exception &e) {
    LOG_ERROR("VFXSequenceManager: json parse failed {} ({})",
              filePath.string(), e.what());
    return false;
  }

  if (!document.contains("vfx_schema_version") ||
      !document["vfx_schema_version"].is_number_integer()) {
    LOG_ERROR(
        "VFXSequenceManager: missing required integer vfx_schema_version in {}",
        filePath.string());
    return false;
  }

  const int schemaVersion = document["vfx_schema_version"].get<int>();
  if (schemaVersion < VFXSequenceManager::VFX_SCHEMA_MIN_COMPAT_VERSION ||
      schemaVersion > VFXSequenceManager::VFX_SCHEMA_VERSION) {
    LOG_ERROR(
        "VFXSequenceManager: unsupported schema version {} in {} "
        "(supported range={}..{})",
        schemaVersion, filePath.string(),
        VFXSequenceManager::VFX_SCHEMA_MIN_COMPAT_VERSION,
        VFXSequenceManager::VFX_SCHEMA_VERSION);
    return false;
  }
  if (schemaVersion < VFXSequenceManager::VFX_SCHEMA_VERSION) {
    LOG_INFO("VFXSequenceManager: loading compatibility schema version {} from {}",
             schemaVersion, filePath.string());
  }

  const std::string name =
      document.value("name", filePath.stem().string());
  if (name.empty()) {
    LOG_WARN("VFXSequenceManager: sequence name missing in {}", filePath.string());
    return false;
  }

  VFXSequenceAsset sequence;
  sequence.name = name;
  sequence.duration = document.value("duration", 1.0f);
  sequence.version = schemaVersion;
  sequence.minTier =
      ParseQualityTier(document, "minTier", render::core::QualityTier::Low);

  if (document.contains("events") && document["events"].is_array()) {
    for (const auto &eventNode : document["events"]) {
      if (!eventNode.is_object()) {
        continue;
      }
      if (!eventNode.contains("type") || !eventNode["type"].is_string()) {
        LOG_WARN("VFXSequenceManager: skipped event without valid type in {}",
                 filePath.string());
        continue;
      }

      EventType eventType = EventType::Particle;
      if (!ParseEventType(eventNode["type"].get<std::string>(), eventType)) {
        LOG_WARN("VFXSequenceManager: unknown event type '{}' in {}",
                 eventNode["type"].get<std::string>(), filePath.string());
        continue;
      }

      const json emptyParams = json::object();
      const json &paramsNode =
          (eventNode.contains("params") && eventNode["params"].is_object())
              ? eventNode["params"]
              : emptyParams;

      VFXEvent event;
      event.time = eventNode.value("time", 0.0f);
      event.type = eventType;
      event.anchor = ParseAnchorType(eventNode);
      event.minTier = ParseQualityTier(eventNode, "minTier", sequence.minTier);
      if (!ParseTierPolicy(eventNode, filePath, event.tierPolicy)) {
        return false;
      }
      event.params = ParseEventParams(eventType, paramsNode);
      if (!ValidateEventParams(eventType, event.params, filePath)) {
        return false;
      }

      if (event.type == EventType::MaterialSwap) {
        const auto *materialSwap = std::get_if<MaterialSwapParams>(&event.params);
        if (materialSwap != nullptr && materialSwap->materialId <= 0) {
          LOG_WARN(
              "VFXSequenceManager: MaterialSwap in {} has invalid materialId, "
              "runtime will fallback",
              filePath.string());
        }
      }
      sequence.events.push_back(std::move(event));
    }
  }

  std::sort(sequence.events.begin(), sequence.events.end(),
            [](const VFXEvent &lhs, const VFXEvent &rhs) {
              return lhs.time < rhs.time;
            });

  outSequence = std::move(sequence);
  return true;
}

} // namespace

VFXSequenceManager &VFXSequenceManager::Get() {
  static VFXSequenceManager manager;
  return manager;
}

void VFXSequenceManager::Initialize() {
  if (m_initialized) {
    return;
  }

  m_sequences.clear();
  m_nameToId.clear();
  m_assetDir.clear();
  m_fileTimestamps.clear();
  m_initialized = true;
}

void VFXSequenceManager::Shutdown() {
  m_sequences.clear();
  m_nameToId.clear();
  m_assetDir.clear();
  m_fileTimestamps.clear();
  m_initialized = false;
}

int VFXSequenceManager::LoadFromJson(const std::string &path) {
  if (path.empty()) {
    return 0;
  }
  if (!m_initialized) {
    Initialize();
  }

  // Sequence assets resolve material names to runtime IDs during parse.
  // Ensure preset registry is available even when caller hasn't initialized render systems yet.
  render::MaterialManager::Get().Initialize();

  std::error_code ec;
  const fs::path input(path);
  std::vector<fs::path> files;
  if (fs::is_directory(input, ec) && !ec) {
    for (const auto &entry : fs::directory_iterator(input, ec)) {
      if (ec) {
        break;
      }
      if (!entry.is_regular_file()) {
        continue;
      }
      const fs::path filePath = entry.path();
      if (ToLower(filePath.extension().string()) == ".json") {
        files.push_back(filePath);
      }
    }
  } else if (fs::is_regular_file(input, ec) && !ec) {
    files.push_back(input);
  } else {
    LOG_ERROR("VFXSequenceManager: invalid path {}", path);
    return 0;
  }

  std::sort(files.begin(), files.end(),
            [](const fs::path &lhs, const fs::path &rhs) {
              return lhs.generic_string() < rhs.generic_string();
            });

  std::vector<VFXSequenceAsset> stagedSequences;
  stagedSequences.reserve(std::min(static_cast<int>(files.size()), MAX_SEQUENCES));
  std::unordered_map<std::string, int> stagedNameToId;
  std::unordered_map<std::string, fs::file_time_type> stagedTimestamps;

  int loaded = 0;
  for (const fs::path &filePath : files) {
    if (loaded >= MAX_SEQUENCES) {
      LOG_WARN("VFXSequenceManager: reached sequence cap {}", MAX_SEQUENCES);
      break;
    }

    VFXSequenceAsset sequence;
    if (!ParseSequenceFile(filePath, sequence)) {
      continue;
    }

    auto existing = stagedNameToId.find(sequence.name);
    if (existing != stagedNameToId.end()) {
      stagedSequences[existing->second] = std::move(sequence);
      LOG_WARN("VFXSequenceManager: duplicate sequence name '{}', replaced",
               stagedSequences[existing->second].name);
    } else {
      const int id = static_cast<int>(stagedSequences.size());
      stagedNameToId.emplace(sequence.name, id);
      stagedSequences.push_back(std::move(sequence));
      ++loaded;
    }

    std::error_code tsError;
    const auto ts = fs::last_write_time(filePath, tsError);
    if (!tsError) {
      stagedTimestamps[filePath.generic_string()] = ts;
    }
  }

  m_sequences = std::move(stagedSequences);
  m_nameToId = std::move(stagedNameToId);
  m_fileTimestamps = std::move(stagedTimestamps);
  m_assetDir = fs::is_directory(input) ? input.string() : input.parent_path().string();

  LOG_INFO("VFXSequenceManager: loaded {} sequence assets from {}", loaded, path);
  return loaded;
}

void VFXSequenceManager::TryHotReload() {
  if (!m_initialized || m_assetDir.empty()) {
    return;
  }
  const auto &qualityManager = render::core::QualityTierManager::Get();
  if (qualityManager.IsInitialized() &&
      !qualityManager.GetConfig().hotReloadEnabled) {
    return;
  }

  std::error_code ec;
  const fs::path dirPath(m_assetDir);
  if (!fs::is_directory(dirPath, ec) || ec) {
    return;
  }

  bool changed = false;
  std::unordered_map<std::string, fs::file_time_type> currentTimestamps;

  for (const auto &entry : fs::directory_iterator(dirPath, ec)) {
    if (ec || !entry.is_regular_file()) {
      continue;
    }
    const fs::path filePath = entry.path();
    if (ToLower(filePath.extension().string()) != ".json") {
      continue;
    }

    std::error_code tsError;
    const auto ts = fs::last_write_time(filePath, tsError);
    if (tsError) {
      continue;
    }

    const std::string key = filePath.generic_string();
    currentTimestamps[key] = ts;

    const auto it = m_fileTimestamps.find(key);
    if (it == m_fileTimestamps.end() || ts > it->second) {
      changed = true;
    }
  }

  if (!changed) {
    for (const auto &[file, _] : m_fileTimestamps) {
      if (!currentTimestamps.contains(file)) {
        changed = true;
        break;
      }
    }
  }

  if (!changed) {
    return;
  }

  const int loaded = LoadFromJson(m_assetDir);
  LOG_INFO("VFXSequenceManager: hot reloaded {} assets from {}", loaded,
           m_assetDir);
}

const VFXSequenceAsset *VFXSequenceManager::GetSequence(int id) const {
  if (id < 0 || id >= static_cast<int>(m_sequences.size())) {
    return nullptr;
  }
  return &m_sequences[id];
}

const VFXSequenceAsset *
VFXSequenceManager::GetSequence(const std::string &name) const {
  const int id = GetSequenceId(name);
  return GetSequence(id);
}

int VFXSequenceManager::GetSequenceId(const std::string &name) const {
  const auto it = m_nameToId.find(name);
  if (it == m_nameToId.end()) {
    return -1;
  }
  return it->second;
}

void VFXSequenceManager::Play(entt::registry &registry, entt::entity entity,
                              const std::string &sequenceName,
                              entt::entity target, bool loop, float targetWorldX,
                              float targetWorldY, bool hasTargetWorld) {
  if (!registry.valid(entity)) {
    return;
  }

  const int sequenceId = GetSequenceId(sequenceName);
  if (sequenceId < 0) {
    LOG_WARN("VFXSequenceManager: sequence '{}' not found", sequenceName);
    return;
  }

  VFXPlayerComponent player;
  player.sequenceId = sequenceId;
  player.elapsed = 0.0f;
  player.nextEventIdx = 0;
  player.target = target;
  player.targetWorldX = targetWorldX;
  player.targetWorldY = targetWorldY;
  player.hasTargetWorld = hasTargetWorld;
  player.loop = loop;
  player.active = true;

  registry.emplace_or_replace<VFXPlayerComponent>(entity, player);
}

void VFXSequenceManager::Stop(entt::registry &registry, entt::entity entity) {
  if (!registry.valid(entity)) {
    return;
  }
  if (registry.all_of<VFXPlayerComponent>(entity)) {
    registry.remove<VFXPlayerComponent>(entity);
  }
}

} // namespace NoMoreDay::vfx
