# NoMoreDayEngineResource: engine-side asset loading and resource management.
# Depends on RenderCore (debug/ShaderReloadGovernance, GPUUtils,
# resources/GPUResourceRegistry); reached via the PUBLIC link surface of
# NoMoreDayEngineRenderCore, which also supplies the shared compile surface.
#
# ResourceManager.cpp performs raw OS file I/O and must not enter the PCH, so
# it keeps SKIP_PRECOMPILE_HEADERS ON exactly as in the pre-split manifest.

add_library(NoMoreDayEngineResource STATIC
    ${CMAKE_SOURCE_DIR}/src/engine/resource/AssetLoadingSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/resource/ResourceManager.cpp
)

set_source_files_properties(${CMAKE_SOURCE_DIR}/src/engine/resource/ResourceManager.cpp PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)

target_link_libraries(NoMoreDayEngineResource PUBLIC NoMoreDayEngineRenderCore)
target_link_libraries(NoMoreDayEngineResource PRIVATE NoMoreDayAnalyzeFlags)

target_precompile_headers(NoMoreDayEngineResource PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.hpp)

add_dependencies(NoMoreDayEngineResource GenerateTags)
