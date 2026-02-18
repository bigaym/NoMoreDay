#include "engine/render/MaterialManager.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

namespace NoMoreDay::render {
namespace {

using json = nlohmann::json;

constexpr std::array<const char *, 14> kSchemaV2RequiredFields = {
    "name",          "baseColor", "emissive",     "emissiveIntensity",
    "distortion",    "blendMode", "shader",       "textureSlots",
    "normalMapSlot", "roughness", "specular",     "ao",
    "heightBias",    "detailNormalScale"};

constexpr std::array<const char *, 14> kSchemaV2AllowedFields = {
    "name",          "baseColor", "emissive",     "emissiveIntensity",
    "distortion",    "blendMode", "shader",       "textureSlots",
    "normalMapSlot", "roughness", "specular",     "ao",
    "heightBias",    "detailNormalScale"};

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool ContainsField(const json &item, const char *field) {
  return item.contains(field);
}

bool IsSchemaV2AllowedField(const std::string &field) {
  return std::find_if(
             kSchemaV2AllowedFields.begin(), kSchemaV2AllowedFields.end(),
             [&](const char *value) { return field == value; }) !=
         kSchemaV2AllowedFields.end();
}

bool ParseBlendMode(const json &item, BlendMode &out, bool strict,
                    std::string &error) {
  if (!ContainsField(item, "blendMode")) {
    if (strict) {
      error = "missing required field 'blendMode'";
      return false;
    }
    out = BlendMode::Alpha;
    return true;
  }
  if (!item["blendMode"].is_string()) {
    error = "field 'blendMode' must be string";
    return false;
  }

  const std::string blend = ToLower(item["blendMode"].get<std::string>());
  if (blend == "additive") {
    out = BlendMode::Additive;
    return true;
  }
  if (blend == "multiply") {
    out = BlendMode::Multiply;
    return true;
  }
  if (blend == "alpha") {
    out = BlendMode::Alpha;
    return true;
  }
  if (strict) {
    error = "field 'blendMode' has unsupported value '" + blend + "'";
    return false;
  }

  out = BlendMode::Alpha;
  return true;
}

bool ParseShaderVariant(const json &item, ShaderVariant &out, bool strict,
                        std::string &error) {
  if (!ContainsField(item, "shader")) {
    if (strict) {
      error = "missing required field 'shader'";
      return false;
    }
    out = ShaderVariant::Default;
    return true;
  }
  if (!item["shader"].is_string()) {
    error = "field 'shader' must be string";
    return false;
  }

  const std::string shader = ToLower(item["shader"].get<std::string>());
  if (shader == "ink") {
    out = ShaderVariant::Ink;
    return true;
  }
  if (shader == "hologram") {
    out = ShaderVariant::Hologram;
    return true;
  }
  if (shader == "fire") {
    out = ShaderVariant::Fire;
    return true;
  }
  if (shader == "ice") {
    out = ShaderVariant::Ice;
    return true;
  }
  if (shader == "lightning") {
    out = ShaderVariant::Lightning;
    return true;
  }
  if (shader == "dissolve") {
    out = ShaderVariant::Dissolve;
    return true;
  }
  if (shader == "default") {
    out = ShaderVariant::Default;
    return true;
  }
  if (strict) {
    error = "field 'shader' has unsupported value '" + shader + "'";
    return false;
  }

  out = ShaderVariant::Default;
  return true;
}

bool ParseFloat(const json &item, const char *field, float &out, bool strict,
                std::string &error) {
  if (!ContainsField(item, field)) {
    if (strict) {
      error = std::string("missing required field '") + field + "'";
      return false;
    }
    return true;
  }
  const json &node = item[field];
  if (!node.is_number()) {
    error = std::string("field '") + field + "' must be number";
    return false;
  }
  out = node.get<float>();
  return true;
}

bool ParseInt16(const json &item, const char *field, int16_t &out, bool strict,
                std::string &error) {
  if (!ContainsField(item, field)) {
    if (strict) {
      error = std::string("missing required field '") + field + "'";
      return false;
    }
    return true;
  }
  const json &node = item[field];
  if (!node.is_number_integer()) {
    error = std::string("field '") + field + "' must be integer";
    return false;
  }
  out = static_cast<int16_t>(node.get<int>());
  return true;
}

bool ParseBaseColor(const json &item, MaterialInstance &material, bool strict,
                    std::string &error) {
  if (!ContainsField(item, "baseColor")) {
    if (strict) {
      error = "missing required field 'baseColor'";
      return false;
    }
    return true;
  }
  const json &arr = item["baseColor"];
  if (!arr.is_array() || arr.size() != 4) {
    error = "field 'baseColor' must be float[4]";
    return false;
  }
  for (size_t i = 0; i < arr.size(); ++i) {
    if (!arr[i].is_number()) {
      error = "field 'baseColor' must contain numbers";
      return false;
    }
  }

  material.baseColorR = arr[0].get<float>();
  material.baseColorG = arr[1].get<float>();
  material.baseColorB = arr[2].get<float>();
  material.baseColorA = arr[3].get<float>();
  return true;
}

bool ParseEmissive(const json &item, MaterialInstance &material, bool strict,
                   std::string &error) {
  if (!ContainsField(item, "emissive")) {
    if (strict) {
      error = "missing required field 'emissive'";
      return false;
    }
  } else {
    const json &arr = item["emissive"];
    if (!arr.is_array() || arr.size() != 3) {
      error = "field 'emissive' must be float[3]";
      return false;
    }
    for (size_t i = 0; i < arr.size(); ++i) {
      if (!arr[i].is_number()) {
        error = "field 'emissive' must contain numbers";
        return false;
      }
    }
    material.emissiveR = arr[0].get<float>();
    material.emissiveG = arr[1].get<float>();
    material.emissiveB = arr[2].get<float>();
  }

  return ParseFloat(item, "emissiveIntensity", material.emissiveIntensity, strict,
                    error);
}

bool ParseTextureSlots(const json &item, MaterialInstance &material, bool strict,
                       std::string &error) {
  if (!ContainsField(item, "textureSlots")) {
    if (strict) {
      error = "missing required field 'textureSlots'";
      return false;
    }
    return true;
  }
  const json &arr = item["textureSlots"];
  if (!arr.is_array() || arr.size() != 4) {
    error = "field 'textureSlots' must be int[4]";
    return false;
  }
  for (size_t i = 0; i < 4; ++i) {
    if (!arr[i].is_number_integer()) {
      error = "field 'textureSlots' must contain integers";
      return false;
    }
    material.textureSlots[i] = static_cast<int16_t>(arr[i].get<int>());
  }
  return true;
}

bool ValidateV2MaterialObject(const json &item, std::string &error) {
  if (!item.is_object()) {
    error = "material entry must be object";
    return false;
  }

  for (auto it = item.begin(); it != item.end(); ++it) {
    if (!IsSchemaV2AllowedField(it.key())) {
      error = "unsupported field '" + it.key() + "'";
      return false;
    }
  }

  for (const char *field : kSchemaV2RequiredFields) {
    if (!item.contains(field)) {
      error = std::string("missing required field '") + field + "'";
      return false;
    }
  }

  return true;
}

bool ParseMaterialEntry(const json &item, int schemaVersion, MaterialInstance &material,
                        std::string &name, std::string &error) {
  const bool strict = schemaVersion >= MaterialManager::MATERIAL_SCHEMA_VERSION;
  material = MaterialPresets::Default();

  if (strict && !ValidateV2MaterialObject(item, error)) {
    return false;
  }

  if (!item.is_object()) {
    error = "material entry must be object";
    return false;
  }
  if (!ContainsField(item, "name") || !item["name"].is_string()) {
    error = "field 'name' is required and must be string";
    return false;
  }
  name = item["name"].get<std::string>();
  if (name.empty()) {
    error = "field 'name' cannot be empty";
    return false;
  }

  if (!ParseBlendMode(item, material.blendMode, strict, error)) {
    return false;
  }
  if (!ParseShaderVariant(item, material.shader, strict, error)) {
    return false;
  }
  if (!ParseFloat(item, "distortion", material.distortion, strict, error)) {
    return false;
  }
  if (!ParseBaseColor(item, material, strict, error)) {
    return false;
  }
  if (!ParseEmissive(item, material, strict, error)) {
    return false;
  }
  if (!ParseTextureSlots(item, material, strict, error)) {
    return false;
  }
  if (!ParseInt16(item, "normalMapSlot", material.normalMapSlot, strict, error)) {
    return false;
  }
  if (!ParseFloat(item, "roughness", material.roughness, strict, error)) {
    return false;
  }
  if (!ParseFloat(item, "specular", material.specular, strict, error)) {
    return false;
  }
  if (!ParseFloat(item, "ao", material.ao, strict, error)) {
    return false;
  }
  if (!ParseFloat(item, "heightBias", material.heightBias, strict, error)) {
    return false;
  }
  if (!ParseFloat(item, "detailNormalScale", material.detailNormalScale, strict,
                  error)) {
    return false;
  }

  return true;
}

} // namespace

MaterialManager &MaterialManager::Get() {
  static MaterialManager manager;
  return manager;
}

void MaterialManager::Initialize() {
  if (m_initialized) {
    return;
  }

  m_nameToId.clear();
  m_registered.fill(false);
  m_materialCount = 0;
  m_nextDynamicId = PRESET_RESERVE;
  m_gpuUploadCount = 0;
  m_dirty = true;
  m_jsonPath.clear();
  m_lastModified = {};
  m_v1WarnedAssets.clear();
  m_runtimePhaseShiftActive = false;
  m_runtimeRoughnessScale = 1.0f;
  m_runtimeSpecularScale = 1.0f;
  m_runtimeEmissiveScale = 1.0f;

  const MaterialInstance defaultMaterial = MaterialPresets::Default();
  const components::GPUMaterialDataV2 defaultGpu = ToGpuData(defaultMaterial);
  m_materials.fill(defaultMaterial);
  m_gpuMaterials.fill(defaultGpu);

  RegisterPresets();

  if (utils::GPUUtils::IsInitialized()) {
    m_ssbo.Create(static_cast<size_t>(MAX_MATERIALS) *
                      sizeof(components::GPUMaterialDataV2),
                  nullptr, RL_DYNAMIC_DRAW);
  }

  m_initialized = true;
  SyncToGPU();
}

void MaterialManager::Shutdown() {
  m_ssbo.Release();
  m_nameToId.clear();
  m_registered.fill(false);
  m_materialCount = 0;
  m_nextDynamicId = PRESET_RESERVE;
  m_gpuUploadCount = 0;
  m_dirty = true;
  m_jsonPath.clear();
  m_lastModified = {};
  m_v1WarnedAssets.clear();
  m_runtimePhaseShiftActive = false;
  m_runtimeRoughnessScale = 1.0f;
  m_runtimeSpecularScale = 1.0f;
  m_runtimeEmissiveScale = 1.0f;
  m_initialized = false;
}

int MaterialManager::RegisterPresetMaterial(int id, const MaterialInstance &mat,
                                            const std::string &name) {
  if (id < 0 || id >= PRESET_RESERVE || id >= MAX_MATERIALS) {
    LOG_ERROR("MaterialManager: preset id {} out of range", id);
    return -1;
  }

  MarkSlot(id, mat, name);
  return id;
}

void MaterialManager::RegisterPresets() {
  RegisterPresetMaterial(0, MaterialPresets::Default(), "Default");
  RegisterPresetMaterial(1, MaterialPresets::InkSplash(), "InkSplash");
  RegisterPresetMaterial(2, MaterialPresets::FireGlow(), "FireGlow");
  RegisterPresetMaterial(3, MaterialPresets::IceCrystal(), "IceCrystal");
  RegisterPresetMaterial(4, MaterialPresets::LightningArc(), "LightningArc");
  RegisterPresetMaterial(5, MaterialPresets::HoloBlade(), "HoloBlade");
  RegisterPresetMaterial(6, MaterialPresets::ShadowVoid(), "ShadowVoid");
  RegisterPresetMaterial(7, MaterialPresets::DistortionShockwave(),
                         "DistortionShockwave");
}

void MaterialManager::MarkSlot(int id, const MaterialInstance &material,
                               const std::string &name) {
  if (id < 0 || id >= MAX_MATERIALS) {
    return;
  }

  if (!m_registered[id]) {
    m_registered[id] = true;
    ++m_materialCount;
  }

  m_materials[id] = material;
  m_gpuMaterials[id] = ToGpuData(material);
  m_gpuUploadCount = std::max(m_gpuUploadCount, id + 1);
  if (!name.empty()) {
    m_nameToId[name] = id;
  }
  m_dirty = true;
}

int MaterialManager::RegisterMaterial(const MaterialInstance &mat,
                                      const std::string &name) {
  if (!m_initialized) {
    Initialize();
  }

  if (!name.empty()) {
    const auto it = m_nameToId.find(name);
    if (it != m_nameToId.end()) {
      MarkSlot(it->second, mat, name);
      return it->second;
    }
  }

  if (m_nextDynamicId >= MAX_MATERIALS) {
    LOG_WARN("MaterialManager: max material capacity {} reached", MAX_MATERIALS);
    return -1;
  }

  const int id = m_nextDynamicId++;
  MarkSlot(id, mat, name);
  return id;
}

int MaterialManager::LoadFromJson(const std::string &path) {
  if (path.empty()) {
    return 0;
  }
  if (!m_initialized) {
    Initialize();
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("MaterialManager: failed to open {}", path);
    return 0;
  }

  json document;
  try {
    file >> document;
  } catch (const std::exception &e) {
    LOG_ERROR("MaterialManager: json parse failed {} ({})", path, e.what());
    return 0;
  }

  if (!document.contains("material_schema_version") ||
      !document["material_schema_version"].is_number_integer()) {
    LOG_ERROR(
        "MaterialManager: schema_validation_failed path={} reason=missing_or_invalid_material_schema_version",
        path);
    return 0;
  }

  const int schemaVersion = document["material_schema_version"].get<int>();
  if (schemaVersion < MATERIAL_SCHEMA_MIN_VERSION ||
      schemaVersion > MATERIAL_SCHEMA_VERSION) {
    LOG_ERROR(
        "MaterialManager: schema_validation_failed path={} reason=unsupported_schema version={} supported=[{},{}]",
        path, schemaVersion, MATERIAL_SCHEMA_MIN_VERSION, MATERIAL_SCHEMA_VERSION);
    return 0;
  }
  if (schemaVersion == MATERIAL_SCHEMA_MIN_VERSION &&
      m_v1WarnedAssets.insert(path).second) {
    LOG_WARN(
        "MaterialManager: schema_compatibility path={} schema={} action=apply_defaults target_schema={}",
        path, schemaVersion, MATERIAL_SCHEMA_VERSION);
  }

  if (!document.contains("materials") || !document["materials"].is_array()) {
    LOG_WARN("MaterialManager: schema_validation_failed path={} reason=no_materials_array",
             path);
    return 0;
  }

  auto stagedMaterials = std::make_unique<decltype(m_materials)>(m_materials);
  auto stagedGpuMaterials =
      std::make_unique<decltype(m_gpuMaterials)>(m_gpuMaterials);
  auto stagedRegistered =
      std::make_unique<decltype(m_registered)>(m_registered);
  auto stagedNameToId = m_nameToId;

  const MaterialInstance defaultMaterial = MaterialPresets::Default();
  const components::GPUMaterialDataV2 defaultGpu = ToGpuData(defaultMaterial);
  for (int id = PRESET_RESERVE; id < MAX_MATERIALS; ++id) {
    (*stagedMaterials)[id] = defaultMaterial;
    (*stagedGpuMaterials)[id] = defaultGpu;
    (*stagedRegistered)[id] = false;
  }

  for (auto it = stagedNameToId.begin(); it != stagedNameToId.end();) {
    if (it->second >= PRESET_RESERVE) {
      it = stagedNameToId.erase(it);
    } else {
      ++it;
    }
  }

  int stagedMaterialCount = 0;
  int stagedGpuUploadCount = 0;
  for (int id = 0; id < MAX_MATERIALS; ++id) {
    if ((*stagedRegistered)[id]) {
      ++stagedMaterialCount;
      stagedGpuUploadCount = std::max(stagedGpuUploadCount, id + 1);
    }
  }

  int stagedNextDynamicId = PRESET_RESERVE;
  while (stagedNextDynamicId < MAX_MATERIALS &&
         (*stagedRegistered)[stagedNextDynamicId]) {
    ++stagedNextDynamicId;
  }

  const auto stageMarkSlot = [&](int id, const MaterialInstance &material,
                                 const std::string &name) {
    if (id < 0 || id >= MAX_MATERIALS) {
      return;
    }

    if (!(*stagedRegistered)[id]) {
      (*stagedRegistered)[id] = true;
      ++stagedMaterialCount;
    }

    (*stagedMaterials)[id] = material;
    (*stagedGpuMaterials)[id] = ToGpuData(material);
    stagedGpuUploadCount = std::max(stagedGpuUploadCount, id + 1);
    if (!name.empty()) {
      stagedNameToId[name] = id;
    }
  };

  const auto stageRegisterMaterial =
      [&](const MaterialInstance &material, const std::string &name) -> int {
    if (!name.empty()) {
      const auto existing = stagedNameToId.find(name);
      if (existing != stagedNameToId.end()) {
        stageMarkSlot(existing->second, material, name);
        return existing->second;
      }
    }

    while (stagedNextDynamicId < MAX_MATERIALS &&
           (*stagedRegistered)[stagedNextDynamicId]) {
      ++stagedNextDynamicId;
    }
    if (stagedNextDynamicId >= MAX_MATERIALS) {
      LOG_WARN("MaterialManager: max material capacity {} reached",
               MAX_MATERIALS);
      return -1;
    }

    const int id = stagedNextDynamicId++;
    stageMarkSlot(id, material, name);
    return id;
  };

  const bool strictSchema = schemaVersion >= MATERIAL_SCHEMA_VERSION;
  int loaded = 0;
  for (size_t index = 0; index < document["materials"].size(); ++index) {
    const auto &item = document["materials"][index];
    MaterialInstance material = MaterialPresets::Default();
    std::string name;
    std::string error;
    if (!ParseMaterialEntry(item, schemaVersion, material, name, error)) {
      if (strictSchema) {
        LOG_ERROR(
            "MaterialManager: schema_validation_failed path={} entry={} reason={}",
            path, index, error);
        return 0;
      }

      LOG_WARN(
          "MaterialManager: schema_compatibility_skipped path={} entry={} reason={}",
          path, index, error);
      continue;
    }

    if (stageRegisterMaterial(material, name) >= 0) {
      ++loaded;
    }
  }

  std::error_code ec;
  m_lastModified = std::filesystem::last_write_time(path, ec);
  if (ec) {
    m_lastModified = {};
  }
  m_jsonPath = path;

  m_materials = std::move(*stagedMaterials);
  m_gpuMaterials = std::move(*stagedGpuMaterials);
  m_registered = std::move(*stagedRegistered);
  m_nameToId = std::move(stagedNameToId);
  m_materialCount = stagedMaterialCount;
  m_nextDynamicId = stagedNextDynamicId;
  m_gpuUploadCount = stagedGpuUploadCount;
  m_dirty = true;

  LOG_INFO("MaterialManager: loaded {} materials from {} (schema={})", loaded,
           path, schemaVersion);
  return loaded;
}

void MaterialManager::TryHotReload() {
  if (m_jsonPath.empty() || !m_initialized) {
    return;
  }
  if (!core::QualityTierManager::Get().IsInitialized()) {
    return;
  }
  if (!core::QualityTierManager::Get().GetConfig().hotReloadEnabled) {
    return;
  }

  std::error_code ec;
  const auto modified = std::filesystem::last_write_time(m_jsonPath, ec);
  if (ec || modified <= m_lastModified) {
    return;
  }

  const int loaded = LoadFromJson(m_jsonPath);
  if (loaded > 0) {
    LOG_INFO("MaterialManager: hot reloaded {}", m_jsonPath);
  }
}

const MaterialInstance &MaterialManager::GetMaterial(int materialId) const {
  static const MaterialInstance kFallback = MaterialPresets::Default();
  if (materialId >= 0 && materialId < MAX_MATERIALS && m_registered[materialId]) {
    return m_materials[materialId];
  }
  if (m_registered[0]) {
    return m_materials[0];
  }
  return kFallback;
}

int MaterialManager::GetMaterialId(const std::string &name) const {
  const auto it = m_nameToId.find(name);
  if (it == m_nameToId.end()) {
    return -1;
  }
  return it->second;
}

void MaterialManager::SetRuntimePhaseShift(float roughnessScale,
                                           float specularScale,
                                           float emissiveScale) {
  const float clampedRoughness = std::max(0.0f, roughnessScale);
  const float clampedSpecular = std::max(0.0f, specularScale);
  const float clampedEmissive = std::max(0.0f, emissiveScale);
  const bool changed = (!m_runtimePhaseShiftActive) ||
                       (m_runtimeRoughnessScale != clampedRoughness) ||
                       (m_runtimeSpecularScale != clampedSpecular) ||
                       (m_runtimeEmissiveScale != clampedEmissive);
  m_runtimePhaseShiftActive = true;
  m_runtimeRoughnessScale = clampedRoughness;
  m_runtimeSpecularScale = clampedSpecular;
  m_runtimeEmissiveScale = clampedEmissive;
  if (changed) {
    m_dirty = true;
  }
}

void MaterialManager::ResetRuntimePhaseShift() {
  if (!m_runtimePhaseShiftActive) {
    return;
  }
  m_runtimePhaseShiftActive = false;
  m_runtimeRoughnessScale = 1.0f;
  m_runtimeSpecularScale = 1.0f;
  m_runtimeEmissiveScale = 1.0f;
  m_dirty = true;
}

void MaterialManager::SyncToGPU() {
  if (!m_initialized || !m_dirty) {
    return;
  }
  if (!utils::GPUUtils::IsInitialized()) {
    return;
  }
  if (m_ssbo.GetId() == 0) {
    m_ssbo.Create(static_cast<size_t>(MAX_MATERIALS) *
                      sizeof(components::GPUMaterialDataV2),
                  nullptr, RL_DYNAMIC_DRAW);
  }

  const int uploadCount = std::max(1, m_gpuUploadCount);
  const size_t bytes =
      static_cast<size_t>(uploadCount) * sizeof(components::GPUMaterialDataV2);
  m_ssbo.OrphanAndUpload(m_gpuMaterials.data(), bytes, RL_DYNAMIC_DRAW);
  m_dirty = false;
}

void MaterialManager::BindSSBO(NoMoreDay::RenderConstants::Binding binding) const {
  if (m_ssbo.GetId() == 0) {
    return;
  }
  m_ssbo.BindBase(static_cast<unsigned int>(binding));
}

components::GPUMaterialDataV2
MaterialManager::ToGpuData(const MaterialInstance &material) const {
  components::GPUMaterialDataV2 gpu = {};
  gpu.baseColor = {material.baseColorR, material.baseColorG, material.baseColorB,
                   material.baseColorA};
  const float roughnessScale = m_runtimePhaseShiftActive ? m_runtimeRoughnessScale : 1.0f;
  const float specularScale = m_runtimePhaseShiftActive ? m_runtimeSpecularScale : 1.0f;
  const float emissiveScale = m_runtimePhaseShiftActive ? m_runtimeEmissiveScale : 1.0f;

  gpu.emissiveAndIntensity = {
      material.emissiveR,
      material.emissiveG,
      material.emissiveB,
      std::max(0.0f, material.emissiveIntensity * emissiveScale)};
  gpu.pbrLite = {std::max(0.0f, material.roughness * roughnessScale),
                 std::max(0.0f, material.specular * specularScale), material.ao,
                 material.heightBias};
  gpu.textureSlots = {static_cast<float>(material.textureSlots[0]),
                      static_cast<float>(material.normalMapSlot),
                      static_cast<float>(material.textureSlots[2]),
                      static_cast<float>(material.textureSlots[3])};
  gpu.detailParams = {material.detailNormalScale,
                      static_cast<float>(material.blendMode),
                      static_cast<float>(material.shader), material.distortion};
  return gpu;
}

} // namespace NoMoreDay::render
