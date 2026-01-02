#pragma once
#include <string>
#include <unordered_map>
#include "raylib.h"
#include "../components/Buff.hpp"

namespace NoMoreDay {

struct BuffVisualData {
    std::string icon_text;
    Color border_color;
    std::string name;
    std::string description;
    bool is_debuff = false;
};

class BuffRegistry {
public:
    static void Initialize();
    static void Shutdown();
    static const BuffVisualData& GetVisualData(BuffType type);

private:
    static std::unordered_map<BuffType, BuffVisualData> registry;
    static BuffVisualData default_data;
};

} // namespace NoMoreDay