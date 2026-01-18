# === Test Runner (Single TU) ===
add_executable(NoMoreDayTests
    main.cpp
)

target_include_directories(NoMoreDayTests PRIVATE 
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/entt
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(NoMoreDayTests PRIVATE NoMoreDayCore)

if(WIN32)
    target_compile_definitions(NoMoreDayTests PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(MSVC)
    target_compile_options(NoMoreDayTests PRIVATE /bigobj /GR-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # target_compile_options(NoMoreDayTests PRIVATE -fno-rtti)
endif()