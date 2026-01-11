#pragma once
#include "AssetRegistry.hpp"

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
        constexpr assets::TextureAsset Equipment_Slot = {"ui_equip_slot"_hs, "assets/textures/ui/equip_slot_background.png"};
        constexpr assets::TextureAsset Panel_Background = {"ui_panel_bg"_hs, "assets/textures/ui/panel_background.png"};

        // 上下文菜单
        constexpr assets::TextureAsset Context_Menu_BG = {"ui_context_bg"_hs, "assets/textures/ui/context_menu_bg.png"};

        // 技能图标
        // constexpr assets::TextureAsset Skill_Icon_1 = { "ui_skill_icon_1"_hs, "assets/textures/ui/icons/skill_icon_1.png" };
        // constexpr assets::TextureAsset Skill_Icon_2 = { "ui_skill_icon_2"_hs, "assets/textures/ui/icons/skill_icon_2.png" };

        // 剑修技能图标
        constexpr assets::TextureAsset Skill_LiuYunCi = {"ui_skill_liuyunci"_hs, "assets/textures/ui/icons/skills/skill_liuyunci.png"};                      // 流云刺
        constexpr assets::TextureAsset Skill_LieKongZhan = {"ui_skill_liekongzhan"_hs, "assets/textures/ui/icons/skills/skill_liekongzhan.png"};             // 裂空斩
        constexpr assets::TextureAsset Skill_WanJianJue = {"ui_skill_wanjianjue"_hs, "assets/textures/ui/icons/skills/skill_wanjianjue.png"};                // 灵剑决
        constexpr assets::TextureAsset Skill_JianQiHuTi = {"ui_skill_jianqihuti"_hs, "assets/textures/ui/icons/skills/skill_jianqihuti.png"};                // 剑气护体
        constexpr assets::TextureAsset Skill_WanJianGuiZong = {"ui_skill_wanjianguizong"_hs, "assets/textures/ui/icons/skills/skill_wanjianguizong.png"};    // 万剑归宗
        constexpr assets::TextureAsset Skill_ZhuXianJianZhen = {"ui_skill_zhuxianjianzhen"_hs, "assets/textures/ui/icons/skills/skill_zhuxianjianzhen.png"}; // 剑阵·诛仙
        constexpr assets::TextureAsset Skill_XinJianWuYing = {"ui_skill_xinjianwuying"_hs, "assets/textures/ui/icons/skills/skill_xinjianwuying.png"};       // 心剑·无影
        constexpr assets::TextureAsset Skill_YuJianHuiXuan = {"ui_skill_yujianhuixuan"_hs, "assets/textures/ui/icons/skills/skill_yujianhuixuan.png"};       // 御剑·回旋
        constexpr assets::TextureAsset Skill_JueYingShan = {"ui_skill_jueyingshan"_hs, "assets/textures/ui/icons/skills/skill_jueyingshan.png"};             // 绝影闪
    }

    namespace fonts
    {
        constexpr UIFontAsset Main_Chinese = {"ui_font_main"_hs, "assets/fonts/simsun.ttc", 24};
        constexpr UIFontAsset Title_Chinese = {"ui_font_title"_hs, "assets/fonts/simsun.ttc", 32};
    }
}
