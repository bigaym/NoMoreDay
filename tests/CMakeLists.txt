add_executable(StatsSystemTest 
    StatsSystemTest.cpp
)
target_include_directories(StatsSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(StatsSystemTest PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(StatsSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(CombatSystemTest 
    CombatSystemTest.cpp
)
target_include_directories(CombatSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(CombatSystemTest PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(CombatSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(StatsBenchmark 
    StatsBenchmark.cpp
)
target_include_directories(StatsBenchmark PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(StatsBenchmark PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(StatsBenchmark PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(ProgressionSystemTest 
    ProgressionSystemTest.cpp
)
target_include_directories(ProgressionSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(ProgressionSystemTest PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(ProgressionSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(DropSystemTest 
    DropSystemTest.cpp
)
target_include_directories(DropSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(DropSystemTest PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(DropSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(DropSystemBenchmark 
    DropSystemBenchmark.cpp
)
target_include_directories(DropSystemBenchmark PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(DropSystemBenchmark PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(DropSystemBenchmark PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(UISystemTest 
    UISystemTest.cpp
)
target_include_directories(UISystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(UISystemTest PRIVATE NoMoreDayCore)
target_compile_definitions(UISystemTest PRIVATE TEST_HEADLESS)
if(WIN32)
    target_compile_definitions(UISystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(ItemStatsTest 
    ItemStatsTest.cpp
)
target_include_directories(ItemStatsTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(ItemStatsTest PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(ItemStatsTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(RenderSystemTest 
    RenderSystemTest.cpp
)
target_include_directories(RenderSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(RenderSystemTest PRIVATE NoMoreDayCore)
target_compile_definitions(RenderSystemTest PRIVATE TEST_HEADLESS)
if(WIN32)
    target_compile_definitions(RenderSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(AssetLoadingSystemTest 
    AssetLoadingSystemTest.cpp
)
target_include_directories(AssetLoadingSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(AssetLoadingSystemTest PRIVATE NoMoreDayCore)
target_compile_definitions(AssetLoadingSystemTest PRIVATE TEST_HEADLESS)
if(WIN32)
    target_compile_definitions(AssetLoadingSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(AffixSystemTest 
    AffixSystemTest.cpp
)
target_include_directories(AffixSystemTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(AffixSystemTest PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(AffixSystemTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(ItemModificationTest 
    ItemModificationTest.cpp
)
target_include_directories(ItemModificationTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(ItemModificationTest PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(ItemModificationTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

add_executable(LootFilterTest 
    LootFilterTest.cpp
)
target_include_directories(LootFilterTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)
target_link_libraries(LootFilterTest PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(LootFilterTest PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()
