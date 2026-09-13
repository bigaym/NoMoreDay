#pragma once

#include "core/logging/Logger.hpp"
#include "doctest.h"
#include "game/systems/item/ItemFactory.hpp" // If TestSetupScope uses it

using namespace NoMoreDay;

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
