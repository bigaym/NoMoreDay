# Application band wrapper: states, UI, input, and render entry points (top).
# Defines no target of its own; each sub-directory is an independent
# sub-target, added in dependency order (mirrors the pre-M6 wiring).
# (M6: physical convergence of scene/persistence/ui/input/render/states
# plus the two header-only progression systems, now under states/.)

add_subdirectory(scene)       # NoMoreDayGameScene (top)
add_subdirectory(persistence) # NoMoreDayGamePersistence (top)
add_subdirectory(ui)          # NoMoreDayGameUi (top)
add_subdirectory(input)       # NoMoreDayGameInput (top)
add_subdirectory(render)      # NoMoreDayGameRender (top)
add_subdirectory(states)      # NoMoreDayGameStates (top)
