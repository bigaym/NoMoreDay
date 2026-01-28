#pragma once

// STL
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <queue>
#include <stack>
#include <bitset>
#include <iostream>
#include <fstream>
#include <sstream>
#include <array>
#include <functional>
#include <chrono>
#include <filesystem>
#include <utility>
#include <optional>
#include <variant>
#include <span>
#include <concepts>
#include <numbers>
#include <cassert>
#include <type_traits>
#include <string_view>
#include <numeric>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <thread>
#include <typeindex>

// Third Party - Heavy Headers
#include <raylib.h> 
#include <raymath.h>
#include <rlgl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <xsimd/xsimd.hpp>

#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>

// Project Utilities - Core (Stable & Ubiquitous)
#include "core/logging/Logger.hpp"              // 日志
#include "core/utils/ScopedTimer.hpp"             // 性能计时器
#include "core/utils/HashUtils.hpp"             // 哈希工具
// #include "core/math/PhysicsUtils.hpp"           // 物理 (经常改动，暂不放 PCH)

#include "game/data/TagRegistry.hpp"            // 标签注册 (生成文件，变动少，使用广)

// Game Logic Components - Keep OUT of PCH to avoid cascade rebuilds
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Combat.hpp"
#include "game/components/SkillDefs.hpp"
#include "engine/resource/EquipmentAssetRegistry.hpp"
#include "engine/resource/RuneAssetRegistry.hpp"

// Engine - REMOVED to avoid frequent PCH rebuilds
// #include "engine/render/GPUData.hpp"
// #include "engine/render/GPUEntitySystem.hpp"
