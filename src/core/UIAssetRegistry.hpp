#pragma once
#include "AssetRegistry.hpp"

namespace assets::ui {

    struct UIFontAsset {
        entt::id_type id;
        std::string_view path;
        int defaultSize;
    };

    namespace textures {
        // UI Icons and Backgrounds
        constexpr assets::TextureAsset Inventory_Slot = { "ui_inv_slot"_hs, "assets/textures/ui/slot_background.png" };
        constexpr assets::TextureAsset Equipment_Slot = { "ui_equip_slot"_hs, "assets/textures/ui/equip_slot_background.png" };
        constexpr assets::TextureAsset Panel_Background = { "ui_panel_bg"_hs, "assets/textures/ui/panel_background.png" };
        
        // Context Menu
        constexpr assets::TextureAsset Context_Menu_BG = { "ui_context_bg"_hs, "assets/textures/ui/context_menu_bg.png" };
    }

    namespace fonts {
        constexpr UIFontAsset Main_Chinese = { "ui_font_main"_hs, "assets/fonts/simsun.ttc", 24 };
        constexpr UIFontAsset Title_Chinese = { "ui_font_title"_hs, "assets/fonts/simsun.ttc", 32 };
    }
}
