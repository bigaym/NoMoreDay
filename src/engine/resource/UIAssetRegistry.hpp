#pragma once
#include "engine/resource/AssetRegistry.hpp"
#include <array>

namespace assets::ui
{

    struct UIFontAsset
    {
        entt::id_type id;
        std::string_view path;
        int defaultSize;
    };

    namespace textures
    {
        // UI 图标和背景
        constexpr assets::TextureAsset Inventory_Slot = {"ui_inv_slot"_hs, "assets/textures/ui/slot_background.png"};
        constexpr assets::TextureAsset Panel_Background = {"ui_panel_bg"_hs, "assets/textures/ui/panel_background.png"};
        constexpr assets::TextureAsset Home_Page = {"ui_home_page"_hs, "assets/textures/ui/ui_home.png"};  // 主页面
        constexpr assets::TextureAsset Button_Menu = {"ui_menu_btn"_hs, "assets/textures/ui/button_primary.png"};  // 按钮：384*126，约3:1,主要用于主界面菜单和esc菜单
        constexpr assets::TextureAsset Button_Frost_Square = {"ui_frost_square_btn"_hs, "assets/textures/ui/btn_frost_square.png"};  // 按钮：94*94，1:1,冰蓝色风格
        constexpr assets::TextureAsset Button_Frost_Rect = {"ui_frost_rect_btn"_hs, "assets/textures/ui/btn_frost_wide_2_1.png"};  // 按钮：106*56，2:1,冰蓝色风格
        constexpr assets::TextureAsset Button_Frost_Square2 = {"ui_frost_square_btn2"_hs, "assets/textures/ui/btn_frost_squ2.png"};  // 按钮：102*102，1:1,冰蓝色风格，颜色较浅

        // 上下文菜单
        constexpr assets::TextureAsset Context_Menu_BG = {"ui_context_bg"_hs, "assets/textures/ui/context_menu_bg.png"};

        // 技能图标
        // constexpr assets::TextureAsset Skill_Icon_1 = { "ui_skill_icon_1"_hs, "assets/textures/ui/icons/skill_icon_1.png" };
        // constexpr assets::TextureAsset Skill_Icon_2 = { "ui_skill_icon_2"_hs, "assets/textures/ui/icons/skill_icon_2.png" };

        // 剑修技能图标
        constexpr assets::TextureAsset Skill_LiuYunCi = {"ui_skill_liuyunci"_hs, "assets/textures/icons/skills/skill_liuyunci.png"};                      // 流云刺
        constexpr assets::TextureAsset Skill_LieKongZhan = {"ui_skill_liekongzhan"_hs, "assets/textures/icons/skills/skill_liekongzhan.png"};             // 裂空斩
        constexpr assets::TextureAsset Skill_WanJianJue = {"ui_skill_wanjianjue"_hs, "assets/textures/icons/skills/skill_wanjianjue.png"};                // 灵剑决
        constexpr assets::TextureAsset Skill_JianQiHuTi = {"ui_skill_jianqihuti"_hs, "assets/textures/icons/skills/skill_jianqihuti.png"};                // 剑气护体
        constexpr assets::TextureAsset Skill_WanJianGuiZong = {"ui_skill_wanjianguizong"_hs, "assets/textures/icons/skills/skill_wanjianguizong.png"};    // 万剑归宗
        constexpr assets::TextureAsset Skill_ZhuXianJianZhen = {"ui_skill_zhuxianjianzhen"_hs, "assets/textures/icons/skills/skill_zhuxianjianzhen.png"}; // 剑阵·诛仙
        constexpr assets::TextureAsset Skill_XinJianWuYing = {"ui_skill_xinjianwuying"_hs, "assets/textures/icons/skills/skill_xinjianwuying.png"};       // 心剑·无影
        constexpr assets::TextureAsset Skill_YuJianHuiXuan = {"ui_skill_yujianhuixuan"_hs, "assets/textures/icons/skills/skill_yujianhuixuan.png"};       // 御剑·回旋
        constexpr assets::TextureAsset Skill_JueYingShan = {"ui_skill_jueyingshan"_hs, "assets/textures/icons/skills/skill_jueyingshan.png"};             // 绝影闪
        constexpr assets::TextureAsset Skill_QiXingZhan = {"ui_skill_qixingzhan"_hs, "assets/textures/icons/skills/skill_qixingzhan.png"};                // 七星斩
        constexpr assets::TextureAsset Skill_TianJianJiangLin = {"ui_skill_tianjianjianglin"_hs, "assets/textures/icons/skills/skill_tianjianjianglin.png"}; // 天剑降临
        constexpr assets::TextureAsset Skill_XueHai = {"ui_skill_xuehai"_hs, "assets/textures/icons/skills/skill_xuehai.png"};                            // 血海

        // 装备槽位
        constexpr assets::TextureAsset Slot_Amulet_Mirror = {"slot_amulet_mirror"_hs, "assets/textures/equipslot/item_amulet_mirror.png"}; // 护符槽位
        constexpr assets::TextureAsset Slot_Armor_Chest = {"slot_armor"_hs, "assets/textures/equipslot/item_armor_chest.png"}; // 胸甲槽位
        constexpr assets::TextureAsset Slot_Boots = {"slot_boot"_hs, "assets/textures/equipslot/item_boots_flying.png"}; // 靴子槽位
        constexpr assets::TextureAsset Slot_Gauntlets = {"slot_gauntlets"_hs, "assets/textures/equipslot/item_gauntlets_leather.png"}; // 护手槽位
        constexpr assets::TextureAsset Slot_Helmet = {"slot_helmet"_hs, "assets/textures/equipslot/item_helmet_jade.png"}; // 头盔槽位
        constexpr assets::TextureAsset Slot_Leggings = {"slot_leggings"_hs, "assets/textures/equipslot/item_leggings_silk.png"}; // 护腿槽位
        constexpr assets::TextureAsset Slot_Pauldrons = {"slot_pauldrons"_hs, "assets/textures/equipslot/item_pauldrons_iron.png"}; // 肩甲槽位
        constexpr assets::TextureAsset Slot_Ring_1 = {"slot_ring_1"_hs, "assets/textures/equipslot/item_ring_dragon.png"}; // 戒指槽位 1
        constexpr assets::TextureAsset Slot_Ring_2 = {"slot_ring_2"_hs, "assets/textures/equipslot/item_ring_phoenix.png"}; // 戒指槽位 2
        constexpr assets::TextureAsset Slot_Weapon_Main = {"slot_weapon_main"_hs, "assets/textures/equipslot/item_weapon_main_sword.png"}; // 主手武器槽位
        constexpr assets::TextureAsset Slot_Weapon_Off = {"slot_weapon_off"_hs, "assets/textures/equipslot/item_weapon_off_shield.png"}; // 副手武器槽位


        // 环境
        constexpr assets::TextureAsset Env_Bamboo_Misty = {"env_bamboo_misty"_hs, "assets/textures/env/env_bamboo_misty.png"};
        constexpr assets::TextureAsset Env_Incense_Burner = {"env_incense_burner"_hs, "assets/textures/env/env_incense_burner.png"};
        constexpr assets::TextureAsset Env_Portal_Abyssal = {"env_portal_abyssal"_hs, "assets/textures/env/env_portal_abyssal.png"};
        constexpr assets::TextureAsset Env_Portal_Arcane = {"env_portal_arcane"_hs, "assets/textures/env/env_portal_arcane.png"};   // 普通传送门，用于城镇选择地图
        constexpr assets::TextureAsset Env_Portal_Divine = {"env_portal_divine"_hs, "assets/textures/env/env_portal_divine.png"};
        constexpr assets::TextureAsset Env_Portal_Ghostly = {"env_portal_ghostly"_hs, "assets/textures/env/env_portal_ghostly.png"};
        constexpr assets::TextureAsset Env_Portal_Infernal = {"env_portal_infernal"_hs, "assets/textures/env/env_portal_infernal.png"};
        constexpr assets::TextureAsset Env_Rock_Cluster = {"env_rock_cluster"_hs, "assets/textures/env/env_rock_cluster.png"};
        constexpr assets::TextureAsset Env_Statue_Broken = {"env_statue_broken"_hs, "assets/textures/env/env_statue_broken.png"};
        constexpr assets::TextureAsset Env_Tree_Dead = {"env_tree_dead"_hs, "assets/textures/env/env_tree_dead.png"};

        
        constexpr std::array<const assets::TextureAsset*, 41> All = {
            &Inventory_Slot,
            &Panel_Background,
            &Home_Page,
            &Button_Menu,
            &Button_Frost_Square,
            &Button_Frost_Rect,
            &Button_Frost_Square2,
            &Context_Menu_BG,
            &Skill_LiuYunCi,
            &Skill_LieKongZhan,
            &Skill_WanJianJue,
            &Skill_JianQiHuTi,
            &Skill_WanJianGuiZong,
            &Skill_ZhuXianJianZhen,
            &Skill_XinJianWuYing,
            &Skill_YuJianHuiXuan,
            &Skill_JueYingShan,
            &Skill_QiXingZhan,
            &Skill_TianJianJiangLin,
            &Skill_XueHai,
            &Slot_Amulet_Mirror,
            &Slot_Armor_Chest,
            &Slot_Boots,
            &Slot_Gauntlets,
            &Slot_Helmet,
            &Slot_Leggings,
            &Slot_Pauldrons,
            &Slot_Ring_1,
            &Slot_Ring_2,
            &Slot_Weapon_Main,
            &Slot_Weapon_Off,
            &Env_Bamboo_Misty,
            &Env_Incense_Burner,
            &Env_Portal_Abyssal,
            &Env_Portal_Arcane,
            &Env_Portal_Divine,
            &Env_Portal_Ghostly,
            &Env_Portal_Infernal,
            &Env_Rock_Cluster,
            &Env_Statue_Broken,
            &Env_Tree_Dead,
        };
    }

    namespace fonts
    {
        constexpr UIFontAsset Main_Chinese = {"ui_font_main"_hs, "assets/fonts/simsun.ttc", 24};
        constexpr UIFontAsset Title_Chinese = {"ui_font_title"_hs, "assets/fonts/simsun.ttc", 32};
        constexpr assets::TextureAsset Fast_Font_Img = {"fast_font_img"_hs, "assets/textures/icons/fastfont/popup_glyphs.png"}; // 快速字形
    }
}
