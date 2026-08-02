# Core layer: platform-wide utilities, logging, and crash handling.
# NoMoreDayTypes is the only dependency; its PUBLIC include root propagates to
# every upper layer.
add_library(NoMoreDayCore STATIC
    logging/CrashHandler.cpp
    logging/Logger.cpp
)

target_link_libraries(NoMoreDayCore PUBLIC NoMoreDayTypes)
target_link_libraries(NoMoreDayCore PRIVATE spdlog::spdlog)

if(WIN32)
    target_compile_definitions(NoMoreDayCore PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
    target_link_libraries(NoMoreDayCore PRIVATE dbghelp)
endif()

# The MS-1 contract reserves src/pch.hpp for the aggregate layer and forbids a
# Core PCH that references Engine, Game, or App headers.
set_target_properties(NoMoreDayCore PROPERTIES UNITY_BUILD OFF)
