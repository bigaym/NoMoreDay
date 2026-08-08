# Game layer: gameplay systems, states, UI, world, item, combat, and skill logic.
#
# M4 completes the bottom-up split (design §5.3 Phase C): every physical
# subdirectory is now its own standalone static sub-target, and the aggregate
# NoMoreDayGame becomes a source-less library that PUBLIC-links all of them.
# NoMoreDayGame remains the target name referenced by check_module_boundaries.py
# and CTest. The contract STATIC library (contracts/impl) is added early so it
# can link the bottom sub-targets it sits above; the middle band comes next,
# then combat (depends on skill/world/stats/combat_v2), then the top band
# (scene/persistence/input/render/states/ui), which is the widest dependency fan.
#
# Skill behavior OBJECT files are linked PUBLIC so static registration objects
# reach the final executable and test binaries.

add_subdirectory(contracts)      # NoMoreDayGameContractsCore (INTERFACE)
add_subdirectory(ui_shared)      # NoMoreDayGameUiShared (ring 2 break: item/render/ui interchange)
add_subdirectory(data)           # NoMoreDayGameData
add_subdirectory(utils)          # NoMoreDayGameUtils
add_subdirectory(stats)          # NoMoreDayGameStats
add_subdirectory(combat_v2)      # NoMoreDayGameCombatV2
add_subdirectory(contracts/impl) # NoMoreDayGameContracts (STATIC)
add_subdirectory(systems/modifier)   # NoMoreDayGameModifier (middle)
add_subdirectory(systems/skill)      # NoMoreDayGameSkill (middle)
add_subdirectory(systems/world)      # NoMoreDayGameWorld (middle)
add_subdirectory(systems/item)       # NoMoreDayGameItem (middle)
add_subdirectory(systems/physics)    # NoMoreDayGamePhysics (middle)
add_subdirectory(systems/ai)         # NoMoreDayGameAi (middle)
add_subdirectory(systems/vfx)        # NoMoreDayGameVfx (middle)
add_subdirectory(systems/nemesis)    # NoMoreDayGameNemesis (middle)
add_subdirectory(systems/combat)     # NoMoreDayGameCombat (upper)
add_subdirectory(scene)              # NoMoreDayGameScene (top)
add_subdirectory(persistence)        # NoMoreDayGamePersistence (top)
add_subdirectory(systems/ui)         # NoMoreDayGameUi (top)
add_subdirectory(systems/input)      # NoMoreDayGameInput (top)
add_subdirectory(render)             # NoMoreDayGameRender (top)
add_subdirectory(states)             # NoMoreDayGameStates (top)

add_library(NoMoreDayGame STATIC)

add_dependencies(NoMoreDayGame GenerateTags)

target_precompile_headers(NoMoreDayGame PRIVATE ${CMAKE_SOURCE_DIR}/src/game/pch.hpp)

# PUBLIC bottom-up order mirrors the add_subdirectory order: contracts above
# data/utils/stats/combat_v2, middle band above combat, top band last,
# aggregate above engine/skill behaviors.
target_link_libraries(NoMoreDayGame PUBLIC
    NoMoreDayGameContractsCore
    NoMoreDayGameContracts
    NoMoreDayGameUiShared
    NoMoreDayGameData
    NoMoreDayGameUtils
    NoMoreDayGameStats
    NoMoreDayGameCombatV2
    NoMoreDayGameModifier
    NoMoreDayGameSkill
    NoMoreDayGameWorld
    NoMoreDayGameItem
    NoMoreDayGamePhysics
    NoMoreDayGameAi
    NoMoreDayGameVfx
    NoMoreDayGameNemesis
    NoMoreDayGameCombat
    NoMoreDayGameScene
    NoMoreDayGamePersistence
    NoMoreDayGameUi
    NoMoreDayGameInput
    NoMoreDayGameRender
    NoMoreDayGameStates
    NoMoreDayEngine
)
target_link_libraries(NoMoreDayGame PUBLIC SkillBehaviors)
