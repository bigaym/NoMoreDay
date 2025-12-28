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
        constexpr TextureAsset Weapon_Sword = { "weapon_sword"_hs, "assets/textures/weapons/weapon_sword_fantasy_01.png" };
        
        // 角色
        constexpr TextureAsset Player_Warrior = { "player_warrior"_hs, "assets/textures/characters/player_warrior.png" };
        constexpr TextureAsset Skeleton = { "skeleton"_hs, "assets/textures/characters/skeleton.png" };
        constexpr TextureAsset Cultist = { "cultist"_hs, "assets/textures/characters/cultist.png" };
        constexpr TextureAsset Demon = { "demon"_hs, "assets/textures/characters/demon.png" };
        constexpr TextureAsset Corrupted_Beast = { "corrupted_beast"_hs, "assets/textures/characters/corrupted_beast.png" };
    }
}
