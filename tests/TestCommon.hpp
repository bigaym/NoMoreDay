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
  }
  ~TestSetupScope() {
    ProcBudgetManager::Get().ResetForTests();
    CombatTelemetry::Get().ResetForTests();
    CombatTelemetry::Get().SetRuntimeEnabled(false);
    StatsSystem::Reset(); // Clean up
                          // tools::Logger::Shutdown();
  }
};
