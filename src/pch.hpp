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
#include "core/logging/Logger.hpp"
#include "core/utils/HashUtils.hpp"
#include "core/math/PhysicsUtils.hpp"
#include "game/data/TagRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Combat.hpp"
#include "game/components/SkillDefs.hpp"

// Engine
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUEntitySystem.hpp"
