# Foundation band wrapper: base domains and shared state (bottom-up).
# Defines no target of its own; each sub-directory is an independent
# sub-target, added in dependency order. ui_shared goes first as the
# ring-2 break for the item/render/ui interchange.
# (M4: physical convergence of data/utils/stats/ui_shared/combat_v2.)

add_subdirectory(ui_shared)      # NoMoreDayGameUiShared (ring 2 break: item/render/ui interchange)
add_subdirectory(data)           # NoMoreDayGameData
add_subdirectory(utils)          # NoMoreDayGameUtils
add_subdirectory(stats)          # NoMoreDayGameStats
add_subdirectory(combat_v2)      # NoMoreDayGameCombatV2
