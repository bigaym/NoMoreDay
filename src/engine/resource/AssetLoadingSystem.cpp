#include "engine/resource/AssetLoadingSystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/resource/EquipmentAssetRegistry.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "engine/resource/RuneAssetRegistry.hpp"
#include "engine/resource/BuffAssetRegistry.hpp"

namespace NoMoreDay {

ResourceManager *AssetLoadingSystem::m_resourceManager = nullptr;
std::vector<Font> AssetLoadingSystem::m_loadedFonts;

void AssetLoadingSystem::Initialize(ResourceManager &resourceManager) {
  m_resourceManager = &resourceManager;
  RegisterUITextures(); // Automatically register UI base assets
  RegisterShaders();    // Load shaders
  RegisterRunes();      // Register Rune assets
  RegisterBuffs();      // Register Buff assets
  LOG_INFO("AssetLoadingSystem 已初始化。");
}

void AssetLoadingSystem::RegisterShaders() {
  if (!m_resourceManager)
    return;

  // Load AOE Array Shader
  m_resourceManager->loadShader(entt::hashed_string("sh_aoe_array"), "",
                                "assets/shaders/aoe_array.frag");

  // VFX Shaders
  m_resourceManager->loadShader(entt::hashed_string("sh_sword_trail"),
                                "assets/shaders/vfx/sword_trail.vs",
                                "assets/shaders/vfx/sword_trail.fs");

  m_resourceManager->loadShader(entt::hashed_string("sh_holo_blade"),
                                "assets/shaders/vfx/holo_blade.vs",
                                "assets/shaders/vfx/holo_blade.fs");

  m_resourceManager->loadShader(entt::hashed_string("sh_distortion"), "",
                                "assets/shaders/vfx/distortion.fs");

  // Astrolabe Shaders
  m_resourceManager->loadShader(assets::shaders::Void_Nebula.id,
                                std::string(assets::shaders::Void_Nebula.vs_path),
                                std::string(assets::shaders::Void_Nebula.fs_path));

  m_resourceManager->loadShader(assets::shaders::Galaxy_Procedural.id,
                                std::string(assets::shaders::Galaxy_Procedural.vs_path),
                                std::string(assets::shaders::Galaxy_Procedural.fs_path));

  // Register essential VFX textures
  m_resourceManager->registerTexture(entt::hashed_string("vfx_spirit_sword"), "assets/textures/vfx/spirit_sword.png");
  m_resourceManager->registerTexture(entt::hashed_string("vfx_noise"), "assets/textures/vfx/energy_noise.png");
  m_resourceManager->registerTexture(entt::hashed_string("vfx_trail"), "assets/textures/vfx/trail_mask.png");
  m_resourceManager->registerTexture(entt::hashed_string("vfx_dist_norm"), "assets/textures/vfx/distortion_normal.png");

  LOG_INFO("AssetLoadingSystem: Registered VFX shaders and textures.");
}

void AssetLoadingSystem::RegisterUITextures() {
  if (!m_resourceManager)
    return;

  using namespace assets::ui::textures;
  
  // Register all UI textures defined in the registry
  int count = 0;
  for (const auto* asset : All) {
      if (asset) {
          m_resourceManager->registerTexture(asset->id, std::string(asset->path));
          count++;
      }
  }
  
  // Register Fast Font Texture (separate namespace)
  using namespace assets::ui::fonts;
  m_resourceManager->registerTexture(Fast_Font_Img.id,
                                     std::string(Fast_Font_Img.path));

  LOG_INFO("AssetLoadingSystem: Registered {} core UI textures.", count + 1);
}

void AssetLoadingSystem::LoadAllUI() {
  if (!m_resourceManager)
    return;

  using namespace assets::ui::textures;
  for (const auto *asset : All) {
    if (asset) {
      m_resourceManager->loadTexture(asset->id, std::string(asset->path));
    }
  }
}

void AssetLoadingSystem::LoadAllEquipment() {
  if (!m_resourceManager) {
    LOG_LIMITED_ERROR(1.0f, "AssetLoadingSystem: Not initialized!");
    return;
  }

  LOG_INFO("Registering equipment assets (Lazy Load)...");
  int count = 0;

  auto load = [&](const auto &assets) {
    for (const auto &asset : assets) {
      if (asset) {
        m_resourceManager->registerTexture(asset->id, std::string(asset->path));
        count++;
      }
    }
  };

  // 加载所有装备资产
  using namespace assets::equipment;
  load(amulet::All);
  load(axe::All);
  load(boots::All);
  load(chest::All);
  load(dagger::All);
  load(gauntlets::All);
  load(greatsword::All);
  load(grimoire::All);
  load(hammer::All);
  load(helmet::All);
  load(leggings::All);
  load(orb::All);
  load(pauldrons::All);
  load(ring::All);
  load(shield::All);
  load(staff::All);
  load(sword::All);
  load(wand::All);

  LOG_INFO("Registered {} equipment assets.", count);
}

void AssetLoadingSystem::RegisterRunes() {
  if (!m_resourceManager) return;

  using namespace assets::runes::general;
  int count = 0;
  for (const auto *asset : All) {
      if (asset) {
          m_resourceManager->registerTexture(asset->id, std::string(asset->path));
          count++;
      }
  }
  LOG_INFO("Registered {} rune assets.", count);
}

void AssetLoadingSystem::RegisterBuffs() {
  if (!m_resourceManager) return;

  using namespace assets::buffs::general;
  int count = 0;
  for (const auto *asset : All) {
      if (asset) {
          m_resourceManager->registerTexture(asset->id, std::string(asset->path));
          count++;
      }
  }
  LOG_INFO("Registered {} buff assets.", count);
}

Font AssetLoadingSystem::LoadUIFont(const std::string &path, int fontSize) {
  if (!m_resourceManager) {
    LOG_LIMITED_ERROR(1.0f, "AssetLoadingSystem: 未初始化！");
    return GetFontDefault();
  }

  // 使用路径哈希作为通过此系统管理的UI字体的ID
  entt::id_type id = entt::hashed_string(path.c_str());
  return m_resourceManager->loadFont(id, path, fontSize);
}

Texture2D AssetLoadingSystem::LoadUITexture(entt::id_type id,
                                            const std::string &path) {
  if (!m_resourceManager) {
    LOG_LIMITED_ERROR(1.0f, "AssetLoadingSystem: 未初始化！");
    return {0};
  }
  return m_resourceManager->loadTexture(id, path); // 加载UI纹理
}

Texture2D AssetLoadingSystem::GetTexture(entt::id_type id) {
  if (!m_resourceManager) {
    LOG_LIMITED_ERROR(1.0f, "AssetLoadingSystem: Not initialized!");
    return {0};
  }
  return m_resourceManager->getTexture(id);
}

Shader AssetLoadingSystem::GetShader(entt::id_type id) {
  if (!m_resourceManager) {
    LOG_LIMITED_ERROR(1.0f, "AssetLoadingSystem: Not initialized!");
    return {0};
  }
  return m_resourceManager->getShader(id);
}

void AssetLoadingSystem::Shutdown() {
  m_resourceManager = nullptr;
  m_loadedFonts.clear();
  LOG_INFO("AssetLoadingSystem 已关闭。");
}

} // namespace NoMoreDay