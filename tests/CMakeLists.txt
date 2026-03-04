# === Test Runner ===

# Find all test source files
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "*.cpp"
)
list(FILTER TEST_SOURCES EXCLUDE REGEX "check_header.cpp$")

add_executable(NoMoreDayTests
    ${TEST_SOURCES}
)

# Keep new V5 JFA tests out of Unity amalgamation to avoid TU-local helper
# collisions with existing contract test helpers.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/RenderGraphV5ContractsIntegrationTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/ReleaseGateIntegrationTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/ShadowPipelineTierFallbackIntegrationTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/CombatReleaseGateBenchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/CombatCorePerfBaselineTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/FluidSPHReferenceTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/JFADistanceFieldEvaluatorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/CombatCoreParityHarnessTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/ConditionIRTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/SkillSpecModifierAdapterTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/TalentModifierAdapterTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/TagDomainV2Tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/CombatTelemetryBenchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/FluidSimulationBenchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/MaterialLightingBenchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/PostProcessBenchmark.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/performance/RadianceCascadesBenchmark.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

target_include_directories(NoMoreDayTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(NoMoreDayTests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
    ${CMAKE_SOURCE_DIR}/third_party/entt
)

target_link_libraries(NoMoreDayTests PRIVATE NoMoreDayCore)

# Enable Unity Build for Tests
set_target_properties(NoMoreDayTests PROPERTIES
    UNITY_BUILD ON
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${OUTPUT_BIN_DIR}"
)

if(WIN32)
    target_compile_definitions(NoMoreDayTests PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
endif()

if(MSVC)
    target_compile_options(NoMoreDayTests PRIVATE /bigobj /GR- /FS)
endif()

if(MSVC AND ENABLE_RUNTIME_OPT)
    target_link_options(NoMoreDayTests PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
    )
endif()

option(NMD_ENABLE_CTEST_INTEGRATION "Register doctest groups into CTest" ON)
if(BUILD_TESTING AND NMD_ENABLE_CTEST_INTEGRATION)
    add_test(NAME nmd.tests.ci.nonperf
        COMMAND NoMoreDayTests --test-case-exclude=*Performance*
    )
    set_tests_properties(nmd.tests.ci.nonperf PROPERTIES
        LABELS "ci;nonperf"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )

    add_test(NAME nmd.tests.performance
        COMMAND NoMoreDayTests --test-case=*Performance*
    )
    set_tests_properties(nmd.tests.performance PROPERTIES
        LABELS "performance"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 1200
    )

    add_test(NAME nmd.tests.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*
    )
    set_tests_properties(nmd.tests.unit PROPERTIES
        LABELS "unit"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.integration
        COMMAND NoMoreDayTests --test-case=[Integration]*
    )
    set_tests_properties(nmd.tests.integration PROPERTIES
        LABELS "integration"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )

    add_test(NAME nmd.tests.progression.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*Progression*
    )
    set_tests_properties(nmd.tests.progression.unit PROPERTIES
        LABELS "unit;progression;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.progression.integration
        COMMAND NoMoreDayTests --test-case=[Integration]*Progression*
    )
    set_tests_properties(nmd.tests.progression.integration PROPERTIES
        LABELS "integration;progression;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )

    add_test(NAME nmd.tests.ui.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*UI*
    )
    set_tests_properties(nmd.tests.ui.unit PROPERTIES
        LABELS "unit;ui;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.ui.integration
        COMMAND NoMoreDayTests --test-case=[Integration]*UI*
    )
    set_tests_properties(nmd.tests.ui.integration PROPERTIES
        LABELS "integration;ui;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )

    add_test(NAME nmd.tests.item.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*Item*
    )
    set_tests_properties(nmd.tests.item.unit PROPERTIES
        LABELS "unit;item;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.combat.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*Combat*,[Unit]*ConditionIR*,[Unit]*TagDomainV2*,[Unit]*ModifierGraphV2*,[Unit]*DamageKernelV2*
    )
    set_tests_properties(nmd.tests.combat.unit PROPERTIES
        LABELS "unit;combat;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.combat.integration
        COMMAND NoMoreDayTests --test-case=[Integration]*Combat*,[Integration]*ModifierGraphV2*,[Integration]*DamageKernelParity*
    )
    set_tests_properties(nmd.tests.combat.integration PROPERTIES
        LABELS "integration;combat;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )

    add_test(NAME nmd.tests.combat.parity.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*CombatCoreParityHarness*
    )
    set_tests_properties(nmd.tests.combat.parity.unit PROPERTIES
        LABELS "unit;combat;parity;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.combat.perf.baseline
        COMMAND NoMoreDayTests --test-case=[Performance]*CombatCorePerfBaseline*
    )
    set_tests_properties(nmd.tests.combat.perf.baseline PROPERTIES
        LABELS "performance;combat;perf-baseline;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )

    add_test(NAME nmd.tests.skill.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*Skill*
    )
    set_tests_properties(nmd.tests.skill.unit PROPERTIES
        LABELS "unit;skill;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.skill.integration
        COMMAND NoMoreDayTests --test-case=[Integration]*Skill*
    )
    set_tests_properties(nmd.tests.skill.integration PROPERTIES
        LABELS "integration;skill;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )

    add_test(NAME nmd.tests.world.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*World*
    )
    set_tests_properties(nmd.tests.world.unit PROPERTIES
        LABELS "unit;world;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.ai.unit
        COMMAND NoMoreDayTests --test-case=[Unit]*AI*
    )
    set_tests_properties(nmd.tests.ai.unit PROPERTIES
        LABELS "unit;ai;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 600
    )

    add_test(NAME nmd.tests.ai.integration
        COMMAND NoMoreDayTests --test-case=[Integration]*AI*
    )
    set_tests_properties(nmd.tests.ai.integration PROPERTIES
        LABELS "integration;ai;module-gate"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        TIMEOUT 900
    )
endif()
