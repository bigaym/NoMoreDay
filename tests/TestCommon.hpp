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

#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/ProcBudgetManager.hpp"
#include "game/systems/combat/StatsSystem.hpp"

// RAII Helper for Logger and ItemFactory
struct TestSetupScope {
  TestSetupScope() {
    tools::Logger::Init();
    ItemFactory::initialize();
    ProcBudgetManager::Get().ResetForTests();
    CombatEventDispatcher::Init();
    StatsSystem::Reset(); // Clear static cache from previous tests
  }
  ~TestSetupScope() {
    ProcBudgetManager::Get().ResetForTests();
    StatsSystem::Reset(); // Clean up
                          // tools::Logger::Shutdown();
  }
};
