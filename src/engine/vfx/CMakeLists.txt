# NoMoreDayEngineVfx: VFX budget estimation and sequence management. Depends on
# RenderCore (render resources); reached via the PUBLIC link surface of
# NoMoreDayEngineRenderCore, which also supplies the shared compile surface.

add_library(NoMoreDayEngineVfx STATIC
    ${CMAKE_SOURCE_DIR}/src/engine/vfx/VFXBudgetEstimator.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/vfx/VFXSequenceManager.cpp
)

target_link_libraries(NoMoreDayEngineVfx PUBLIC NoMoreDayEngineRenderCore)
target_link_libraries(NoMoreDayEngineVfx PRIVATE NoMoreDayAnalyzeFlags)

target_precompile_headers(NoMoreDayEngineVfx PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.hpp)

add_dependencies(NoMoreDayEngineVfx GenerateTags)
