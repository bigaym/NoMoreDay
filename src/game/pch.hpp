#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

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
#include "core/utils/FmtBuffer.hpp"            // fmt 缓冲区格式化

// ============================================================================
// Dual PCH Architecture Rationale:
// After Phase 3 PCH sanitization, all first-party game business headers
// (Common.hpp, Stats.hpp, Combat.hpp, SkillDefs.hpp, TagRegistry.hpp, etc.)
// were removed to eliminate cascading rebuilds across Game targets.
//
// This file is intentionally retained separately from src/pch.hpp because:
// 1. Layer Boundary Governance: scripts/check_module_boundaries.py enforces that
//    src/pch.hpp is an EngineOwnedPch strictly forbidden from including any game/
//    or app/ headers.
// 2. Future Game Isolation: Retaining src/game/pch.hpp provides a safe boundary
//    for future ubiquitous Game-layer stable dependencies without polluting the
//    lower Engine/Core layers.
// 3. Parallel Build Scalability: Peer game targets remain topologically independent
//    without artificial serialization dependencies (REUSE_FROM across peer libraries
//    would force a single-queue bottleneck on the parallel compile graph).
// ============================================================================

