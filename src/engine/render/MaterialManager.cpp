#include "engine/render/MaterialManager.hpp"

#include "core/logging/Logger.hpp"
#include "engine/render/GPUUtils.hpp"
#include "engine/render/core/QualityTierManager.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>

namespace NoMoreDay::render {
namespace {

using json = nlohmann::json;

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

BlendMode ParseBlendMode(const json &item) {
  if (!item.contains("blendMode") || !item["blendMode"].is_string()) {
    return BlendMode::Alpha;
  }

  const std::string blend = ToLower(item["blendMode"].get<std::string>());
  if (blend == "additive") {
    return BlendMode::Additive;
  }
  if (blend == "multiply") {
    return BlendMode::Multiply;
  }
  return BlendMode::Alpha;
}

ShaderVariant ParseShaderVariant(const json &item) {
  if (!item.contains("shader") || !item["shader"].is_string()) {
    return ShaderVariant::Default;
  }

  const std::string shader = ToLower(item["shader"].get<std::string>());
  if (shader == "ink") {
    return ShaderVariant::Ink;
  }
  if (shader == "hologram") {
    return ShaderVariant::Hologram;
  }
  if (shader == "fire") {
    return ShaderVariant::Fire;
  }
  if (shader == "ice") {
    return ShaderVariant::Ice;
  }
  if (shader == "lightning") {
    return ShaderVariant::Lightning;
  }
  if (shader == "dissolve") {
    return ShaderVariant::Dissolve;
  }
  return ShaderVariant::Default;
}

void ParseBaseColor(const json &item, MaterialInstance &material) {
  if (!item.contains("baseColor") || !item["baseColor"].is_array()) {
    return;
  }

  const json &arr = item["baseColor"];
  if (arr.size() != 4) {
    return;
  }
  material.baseColorR = arr[0].get<float>();
  material.baseColorG = arr[1].get<float>();
  material.baseColorB = arr[2].get<float>();
  material.baseColorA = arr[3].get<float>();
}

void ParseEmissive(const json &item, MaterialInstance &material) {
  if (item.contains("emissive") && item["emissive"].is_array() &&
      item["emissive"].size() == 3) {
    const json &arr = item["emissive"];
    material.emissiveR = arr[0].get<float>();
    material.emissiveG = arr[1].get<float>();
    material.emissiveB = arr[2].get<float>();
  }
  if (item.contains("emissiveIntensity")) {
    material.emissiveIntensity = item.value("emissiveIntensity", 0.0f);
  }
}

void ParseTextureSlots(const json &item, MaterialInstance &material) {
  if (!item.contains("textureSlots") || !item["textureSlots"].is_array()) {
    return;
  }

  const json &arr = item["textureSlots"];
  if (arr.size() != 4) {
    return;
  }
  for (size_t i = 0; i < 4; ++i) {
    material.textureSlots[i] = static_cast<int16_t>(arr[i].get<int>());
  }
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

  const MaterialInstance defaultMaterial = MaterialPresets::Default();
  const components::GPUMaterialData defaultGpu = ToGpuData(defaultMaterial);
  m_materials.fill(defaultMaterial);
  m_gpuMaterials.fill(defaultGpu);

  RegisterPresets();

  if (utils::GPUUtils::IsInitialized()) {
    m_ssbo.Create(static_cast<size_t>(MAX_MATERIALS) *
                      sizeof(components::GPUMaterialData),
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

  const int schemaVersion = document.value("material_schema_version", 0);
  if (schemaVersion != MaterialManager::MATERIAL_SCHEMA_VERSION) {
    LOG_ERROR("MaterialManager: unsupported schema {} in {} (expected {})",
              schemaVersion, path, MaterialManager::MATERIAL_SCHEMA_VERSION);
    return 0;
  }

  if (!document.contains("materials") || !document["materials"].is_array()) {
    LOG_WARN("MaterialManager: no 'materials' array in {}", path);
    return 0;
  }

  auto stagedMaterials = m_materials;
  auto stagedGpuMaterials = m_gpuMaterials;
  auto stagedRegistered = m_registered;
  auto stagedNameToId = m_nameToId;

  const MaterialInstance defaultMaterial = MaterialPresets::Default();
  const components::GPUMaterialData defaultGpu = ToGpuData(defaultMaterial);
  for (int id = PRESET_RESERVE; id < MAX_MATERIALS; ++id) {
    stagedMaterials[id] = defaultMaterial;
    stagedGpuMaterials[id] = defaultGpu;
    stagedRegistered[id] = false;
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
    if (stagedRegistered[id]) {
      ++stagedMaterialCount;
      stagedGpuUploadCount = std::max(stagedGpuUploadCount, id + 1);
    }
  }

  int stagedNextDynamicId = PRESET_RESERVE;
  while (stagedNextDynamicId < MAX_MATERIALS &&
         stagedRegistered[stagedNextDynamicId]) {
    ++stagedNextDynamicId;
  }

  const auto stageMarkSlot = [&](int id, const MaterialInstance &material,
                                 const std::string &name) {
    if (id < 0 || id >= MAX_MATERIALS) {
      return;
    }

    if (!stagedRegistered[id]) {
      stagedRegistered[id] = true;
      ++stagedMaterialCount;
    }

    stagedMaterials[id] = material;
    stagedGpuMaterials[id] = ToGpuData(material);
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
           stagedRegistered[stagedNextDynamicId]) {
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

  int loaded = 0;
  for (const auto &item : document["materials"]) {
    if (!item.is_object()) {
      continue;
    }
    if (!item.contains("name") || !item["name"].is_string()) {
      LOG_WARN("MaterialManager: skipped material without valid name");
      continue;
    }

    try {
      MaterialInstance material = MaterialPresets::Default();
      material.blendMode = ParseBlendMode(item);
      material.shader = ParseShaderVariant(item);
      material.distortion = item.value("distortion", 0.0f);

      ParseBaseColor(item, material);
      ParseEmissive(item, material);
      ParseTextureSlots(item, material);

      const std::string name = item["name"].get<std::string>();
      if (stageRegisterMaterial(material, name) >= 0) {
        ++loaded;
      }
    } catch (const std::exception &e) {
      LOG_WARN("MaterialManager: failed to parse one material entry ({})",
               e.what());
    }
  }

  std::error_code ec;
  m_lastModified = std::filesystem::last_write_time(path, ec);
  if (ec) {
    m_lastModified = {};
  }
  m_jsonPath = path;

  m_materials = std::move(stagedMaterials);
  m_gpuMaterials = std::move(stagedGpuMaterials);
  m_registered = std::move(stagedRegistered);
  m_nameToId = std::move(stagedNameToId);
  m_materialCount = stagedMaterialCount;
  m_nextDynamicId = stagedNextDynamicId;
  m_gpuUploadCount = stagedGpuUploadCount;
  m_dirty = true;

  LOG_INFO("MaterialManager: loaded {} materials from {}", loaded, path);
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

void MaterialManager::SyncToGPU() {
  if (!m_initialized || !m_dirty) {
    return;
  }
  if (!utils::GPUUtils::IsInitialized()) {
    return;
  }
  if (m_ssbo.GetId() == 0) {
    m_ssbo.Create(static_cast<size_t>(MAX_MATERIALS) *
                      sizeof(components::GPUMaterialData),
                  nullptr, RL_DYNAMIC_DRAW);
  }

  const int uploadCount = std::max(1, m_gpuUploadCount);
  const size_t bytes =
      static_cast<size_t>(uploadCount) * sizeof(components::GPUMaterialData);
  m_ssbo.OrphanAndUpload(m_gpuMaterials.data(), bytes, RL_DYNAMIC_DRAW);
  m_dirty = false;
}

void MaterialManager::BindSSBO(NoMoreDay::RenderConstants::Binding binding) const {
  if (m_ssbo.GetId() == 0) {
    return;
  }
  m_ssbo.BindBase(static_cast<unsigned int>(binding));
}

components::GPUMaterialData
MaterialManager::ToGpuData(const MaterialInstance &material) const {
  components::GPUMaterialData gpu = {};
  gpu.baseColorR = material.baseColorR;
  gpu.baseColorG = material.baseColorG;
  gpu.baseColorB = material.baseColorB;
  gpu.baseColorA = material.baseColorA;
  gpu.emissiveR = material.emissiveR;
  gpu.emissiveG = material.emissiveG;
  gpu.emissiveB = material.emissiveB;
  gpu.emissiveIntensity = material.emissiveIntensity;
  gpu.distortion = material.distortion;
  gpu.blendMode = static_cast<uint32_t>(material.blendMode);
  gpu.shaderVariant = static_cast<uint32_t>(material.shader);
  gpu.textureSlot0 = material.textureSlots[0];
  gpu.textureSlot1 = material.textureSlots[1];
  gpu.textureSlot2 = material.textureSlots[2];
  gpu.textureSlot3 = material.textureSlots[3];
  return gpu;
}

} // namespace NoMoreDay::render
