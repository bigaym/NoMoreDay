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
