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

add_executable(tests_runner
    main.cpp
)
target_include_directories(tests_runner PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/entt
    ${CMAKE_CURRENT_SOURCE_DIR} # To find the .hpp test files
)
target_link_libraries(tests_runner PRIVATE NoMoreDayCore)
if(WIN32)
    target_compile_definitions(tests_runner PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(MSVC)
    target_compile_options(tests_runner PRIVATE /bigobj)
endif()

