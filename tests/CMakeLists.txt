add_executable(StatsSystemTest 
    StatsSystemTest.cpp 
    ${CMAKE_SOURCE_DIR}/src/systems/StatsSystem.cpp
)
target_include_directories(StatsSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/entt/src
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_SOURCE_DIR}/third_party/taskflow
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
)
target_link_libraries(StatsSystemTest PRIVATE spdlog::spdlog)
if(WIN32)
    target_compile_definitions(StatsSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(CombatSystemTest 
    CombatSystemTest.cpp 
    ${CMAKE_SOURCE_DIR}/src/systems/CombatSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/Logger.cpp
)
target_include_directories(CombatSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/entt/src
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_SOURCE_DIR}/third_party/taskflow
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
)
target_link_libraries(CombatSystemTest PRIVATE spdlog::spdlog raylib)
if(WIN32)
    target_compile_definitions(CombatSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(StatsBenchmark 
    StatsBenchmark.cpp 
    ${CMAKE_SOURCE_DIR}/src/systems/StatsSystem.cpp
)
target_include_directories(StatsBenchmark PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/entt/src
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_SOURCE_DIR}/third_party/taskflow
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
)
target_link_libraries(StatsBenchmark PRIVATE spdlog::spdlog)
if(WIN32)
    target_compile_definitions(StatsBenchmark PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(ProgressionSystemTest 
    ProgressionSystemTest.cpp 
    ${CMAKE_SOURCE_DIR}/src/systems/ProgressionSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/XPAwardingSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/Logger.cpp
)
target_include_directories(ProgressionSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/entt/src
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_SOURCE_DIR}/third_party/taskflow
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
)
target_link_libraries(ProgressionSystemTest PRIVATE spdlog::spdlog)
if(WIN32)
    target_compile_definitions(ProgressionSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(DropSystemTest 
    DropSystemTest.cpp 
    ${CMAKE_SOURCE_DIR}/src/systems/DropSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ItemFactory.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/Logger.cpp
)
target_include_directories(DropSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/entt/src
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_SOURCE_DIR}/third_party/taskflow
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
)
target_link_libraries(DropSystemTest PRIVATE spdlog::spdlog raylib)
if(WIN32)
    target_compile_definitions(DropSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(DropSystemBenchmark 
    DropSystemBenchmark.cpp 
    ${CMAKE_SOURCE_DIR}/src/systems/DropSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ItemFactory.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/Logger.cpp
)
target_include_directories(DropSystemBenchmark PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/entt/src
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_SOURCE_DIR}/third_party/taskflow
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
)
target_link_libraries(DropSystemBenchmark PRIVATE spdlog::spdlog raylib)
if(WIN32)
    target_compile_definitions(DropSystemBenchmark PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(UISystemTest 
    UISystemTest.cpp 
    ${CMAKE_SOURCE_DIR}/src/systems/UISystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/ProgressionSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/XPAwardingSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/MapSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/FogOfWarSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/Logger.cpp
)
target_include_directories(UISystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/entt/src
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
    ${CMAKE_SOURCE_DIR}/third_party/taskflow
    ${CMAKE_SOURCE_DIR}/third_party/raylib/src
)
target_link_libraries(UISystemTest PRIVATE spdlog::spdlog raylib)
if(WIN32)
    target_compile_definitions(UISystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()
