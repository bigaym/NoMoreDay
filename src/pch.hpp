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

// Project Utilities
#include "core/logging/Logger.hpp"              // 日志
#include "core/utils/ScopedTimer.hpp"             // 性能计时器
#include "core/utils/HashUtils.hpp"             // 哈希工具
#include "core/math/PhysicsUtils.hpp"           // 物理工具
#include "game/data/TagRegistry.hpp"            // 标签注册
#include "game/components/Common.hpp"             // 通用组件
#include "game/components/Stats.hpp"              // 属性
#include "game/components/Combat.hpp"             // 战斗
#include "game/components/SkillDefs.hpp"          // 技能定义
#include "engine/resource/EquipmentAssetRegistry.hpp" // 装备资源注册
#include "engine/resource/RuneAssetRegistry.hpp"    // 符文资源注册

// Engine
#include "engine/render/GPUData.hpp"  // GPU数据
#include "engine/render/GPUEntitySystem.hpp"    // GPU实体系统
