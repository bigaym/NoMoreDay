# App layer: application bootstrap and top-level composition.
add_library(NoMoreDayApp STATIC
    Game.cpp
)

add_dependencies(NoMoreDayApp GenerateTags)

target_precompile_headers(NoMoreDayApp PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.hpp)

target_link_libraries(NoMoreDayApp PUBLIC NoMoreDayGame)

set_target_properties(NoMoreDayApp PROPERTIES UNITY_BUILD OFF)
