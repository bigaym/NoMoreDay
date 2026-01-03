#include "AssetLoadingSystem.hpp"
#include "../tools/Logger.hpp"
#include "EquipmentAssetRegistry.hpp"
#include "UIAssetRegistry.hpp"

namespace NoMoreDay {

ResourceManager* AssetLoadingSystem::m_resourceManager = nullptr;
std::vector<Font> AssetLoadingSystem::m_loadedFonts;

void AssetLoadingSystem::Initialize(ResourceManager& resourceManager) {
    m_resourceManager = &resourceManager;
    RegisterUITextures(); // Automatically register UI base assets
    LOG_INFO("AssetLoadingSystem 已初始化。");
}

void AssetLoadingSystem::RegisterUITextures() {
    if (!m_resourceManager) return;

    using namespace assets::ui::textures;
    m_resourceManager->registerTexture(Inventory_Slot.id, std::string(Inventory_Slot.path));
    m_resourceManager->registerTexture(Equipment_Slot.id, std::string(Equipment_Slot.path));
    m_resourceManager->registerTexture(Panel_Background.id, std::string(Panel_Background.path));
    m_resourceManager->registerTexture(Context_Menu_BG.id, std::string(Context_Menu_BG.path));
    
    // Skill Icons
    m_resourceManager->registerTexture(Skill_Icon_1.id, std::string(Skill_Icon_1.path));
    m_resourceManager->registerTexture(Skill_Icon_2.id, std::string(Skill_Icon_2.path));
    
    LOG_INFO("AssetLoadingSystem: Registered core UI textures.");
}

void AssetLoadingSystem::LoadAllEquipment() {
    if (!m_resourceManager) {
        LOG_ERROR("AssetLoadingSystem: Not initialized!");
        return;
    }

    LOG_INFO("Registering equipment assets (Lazy Load)...");
    int count = 0;

    auto load = [&](const auto& assets) {
        for (const auto& asset : assets) {
            m_resourceManager->registerTexture(asset.id, std::string(asset.path));
            count++;
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

Font AssetLoadingSystem::LoadUIFont(const std::string& path, int fontSize) {
    if (!m_resourceManager) {
        LOG_ERROR("AssetLoadingSystem: 未初始化！");
        return GetFontDefault();
    }
    
    // 使用路径哈希作为通过此系统管理的UI字体的ID
    entt::id_type id = entt::hashed_string(path.c_str());
    return m_resourceManager->loadFont(id, path, fontSize);
}

Texture2D AssetLoadingSystem::LoadUITexture(entt::id_type id, const std::string& path) {
    if (!m_resourceManager) {
        LOG_ERROR("AssetLoadingSystem: 未初始化！");
        return { 0 };
    }
    return m_resourceManager->loadTexture(id, path); // 加载UI纹理
}

Texture2D AssetLoadingSystem::GetTexture(entt::id_type id) {
    if (!m_resourceManager) {
        LOG_ERROR("AssetLoadingSystem: Not initialized!");
        return { 0 };
    }
    return m_resourceManager->getTexture(id);
}

void AssetLoadingSystem::Shutdown() {
    m_resourceManager = nullptr;
    m_loadedFonts.clear();
    LOG_INFO("AssetLoadingSystem 已关闭。");
}

} // namespace NoMoreDay
