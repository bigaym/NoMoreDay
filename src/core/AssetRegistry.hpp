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
        // Weapons
        constexpr TextureAsset Weapon_Sword = { "weapon_sword"_hs, "assets/textures/weapons/weapon_sword_fantasy_01.png" };
        
        // Characters
        constexpr TextureAsset Player_Warrior = { "player_warrior"_hs, "assets/textures/characters/player_warrior_front_NoMoreDay_Asset_00007_.png" };
    }
}
