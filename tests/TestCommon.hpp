#pragma once

#include "core/logging/Logger.hpp"
#include "doctest.h"
#include "game/systems/item/ItemFactory.hpp" // If TestSetupScope uses it
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <system_error>
#include <vector>

using namespace NoMoreDay;

// 从构建产物强制重载 Modifier Runtime V2 registry。
// 该 registry 是进程级单例，可能被其它用例注入的合成数据覆盖；
// 依赖真实生成数据的用例（地图/怪物适配器、AttributePipeline、怪物词缀行为）
// 应在用例开始时显式重载，避免受执行顺序影响。
//
// 失败原因区分“资产缺失”与“解析失败”并各自 WARN 输出，便于在未生成 .bin
// 的机器上定位；资产缺失时提示先运行生成脚本。
inline bool ReloadModifierRuntimeFromAsset() {
  constexpr const char *kAssetPath = "assets/generated/modifier_runtime_v2.bin";
  NoMoreDay::ModifierRuntimeRegistry &registry =
      NoMoreDay::ModifierRuntimeRegistry::Get();

  std::error_code ec;
  if (!std::filesystem::exists(kAssetPath, ec)) {
    DOCTEST_WARN_MESSAGE(
        false, "modifier runtime asset missing: " << kAssetPath
               << " (run: python scripts/gen_modifier_runtime_v2.py --build)");
    return false;
  }

  if (!registry.Reload(kAssetPath)) {
    DOCTEST_WARN_MESSAGE(
        false, "modifier runtime asset failed to parse: " << kAssetPath);
    return false;
  }

  return true;
}

// RAII Helper for Logger
struct LoggerScope {
  LoggerScope() { tools::Logger::Init(); }
  ~LoggerScope() { /* tools::Logger::Shutdown(); */ }
};

#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/CombatTelemetry.hpp"
#include "game/contracts/impl/ProcBudgetManager.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/application/persistence/SaveManager.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"

// RAII Helper for Logger and ItemFactory
struct TestSetupScope {
  TestSetupScope() {
    tools::Logger::Init();
    ItemFactory::initialize();
    ProcBudgetManager::Get().ResetForTests();
    CombatEventDispatcher::Init();
    CombatTelemetry::Get().ResetForTests();
    CombatTelemetry::Get().SetRuntimeEnabled(false);
    StatsSystem::Reset(); // Clear static cache from previous tests
    ResetSkillRegistries();
  }
  ~TestSetupScope() {
    ProcBudgetManager::Get().ResetForTests();
    CombatTelemetry::Get().ResetForTests();
    CombatTelemetry::Get().SetRuntimeEnabled(false);
    StatsSystem::Reset(); // Clean up
    ResetSkillRegistries();
                          // tools::Logger::Shutdown();
  }

private:
  // 技能相关单例是进程级状态：用例注册的临时技能/机制值必须显式复位，
  // 否则会跨用例泄漏（如 BladeWard470 的偶发失败）。
  static void ResetSkillRegistries() {
    // 机制数值表提供专用测试复位接口；技能行为表仅由静态初始化注册，
    // 无按用例可变数据，故无需（也不可）在此清空。
    data::SkillMechanicsRegistry::Get().ResetForTests();
    // 技能数据表没有独立清空接口，用权威数据重载以清除用例注册的临时技能与技能树。
    // LoadFromJson 无返回值：加载失败会静默清空数据表，故显式断言非空防止空跑。
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    CHECK_FALSE(SkillRegistry::Get().GetAllSkills().empty());
  }
};
