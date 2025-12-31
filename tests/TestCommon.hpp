#pragma once

#include "doctest.h"
#include "../src/tools/Logger.hpp"
#include "../src/core/ItemFactory.hpp" // If TestSetupScope uses it

using namespace NoMoreDay;

// RAII Helper for Logger
struct LoggerScope {
    LoggerScope() { tools::Logger::Init(); }
    ~LoggerScope() { tools::Logger::Shutdown(); }
};

// RAII Helper for Logger and ItemFactory
struct TestSetupScope {
    TestSetupScope() { 
        tools::Logger::Init();
        ItemFactory::initialize();
    }
    ~TestSetupScope() { 
        tools::Logger::Shutdown();
    }
};