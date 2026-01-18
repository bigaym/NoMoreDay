#pragma once
#include <entt/entt.hpp>
#include <string_view>

using namespace entt::literals;

namespace assets {

    struct TextureAsset {
        entt::id_type id;
        std::string_view path;
    };

    namespace textures {
        // 武器
        constexpr TextureAsset Weapon_Sword = { "weapon_sword"_hs, "assets/textures/equipment/sword/sword_0.png" };
        
        // 角色
        constexpr TextureAsset Player_Warrior = { "player_warrior"_hs, "assets/textures/characters/player_warrior.png" };
        constexpr TextureAsset Skeleton = { "skeleton"_hs, "assets/textures/monster/skeleton_0.png" };
        constexpr TextureAsset Cultist = { "cultist"_hs, "assets/textures/monster/cultist_0.png" };
        constexpr TextureAsset Demon = { "demon"_hs, "assets/textures/monster/demon_0.png" };
        constexpr TextureAsset Corrupted_Beast = { "corrupted_beast"_hs, "assets/textures/monster/warcraft_0.png" };

        // 装备槽位
        constexpr TextureAsset  EquipSlot_Amulet = { "equipslot_amulet"_hs, "assets/textures/equipslot/item_amulet_mirror.png" };  // 项链
        constexpr TextureAsset  EquipSlot_Armor = { "equipslot_armor"_hs, "assets/textures/equipslot/item_armor_chest.png" };  // 盔甲
        constexpr TextureAsset  EquipSlot_Boots = { "equipslot_boots"_hs, "assets/textures/equipslot/item_boots_flying.png" };  // 鞋子
        constexpr TextureAsset  EquipSlot_Gauntlets = { "equipslot_gauntlets"_hs, "assets/textures/equipslot/item_gauntlets_leather.png" };  // 手套
        constexpr TextureAsset  EquipSlot_Helmet = { "equipslot_helmet"_hs, "assets/textures/equipslot/item_helmet_jade.png" };  // 头盔
        constexpr TextureAsset  EquipSlot_Leggings = { "equipslot_leggings"_hs, "assets/textures/equipslot/item_leggings_silk.png" };  // 长裤
        constexpr TextureAsset  EquipSlot_Pauldrons = { "equipslot_pauldrons"_hs, "assets/textures/equipslot/item_pauldrons_iron.png" };  // 肩甲
        constexpr TextureAsset  EquipSlot_Ring1 = { "equipslot_ring1"_hs, "assets/textures/equipslot/item_ring_dragon.png" };  // 戒指1
        constexpr TextureAsset  EquipSlot_Ring2 = { "equipslot_ring2"_hs, "assets/textures/equipslot/item_ring_phoenix.png" };  // 戒指2
        constexpr TextureAsset  EquipSlot_Weapon_Major = { "equipslot_weapon_maj"_hs, "assets/textures/equipslot/item_weapon_main_sword.png" };  // 主手武器
        constexpr TextureAsset  EquipSlot_Weapon_Minor = { "equipslot_weapon_min"_hs, "assets/textures/equipslot/item_weapon_off_shield.png" };  // 副手武器
    }
}
