# Code Risk Mitigation Spec

## 1. Overview
This track addresses critical stability and performance risks identified in the code analysis report dated 2026-01-16. The primary focus is eliminating a Use-After-Free (UAF) vulnerability in `SkillSystem` and fixing resource leaks in `GPUFlowFieldSystem`.

## 2. Identified Risks

### 2.1 Use-After-Free (UAF) in `SkillSystem`
*   **Problem**: `SkillSystem::UpdateStates` iterates over an EnTT view of `SkillExecution` components. Inside the loop, it calls `s_pre_cast_hooks` and `s_post_cast_hooks` while holding a reference to the component (`auto &exec`).
*   **Risk**: If a hook creates a new entity with `SkillExecution` (e.g., `PhantomFlash` counter-attack or `BehaviorInjection`), the component pool may reallocate, invalidating the `exec` reference held by the loop.
*   **Solution**:
    *   Adopt a **Collect-then-Process** pattern.
    *   Collect valid entities into a local `std::vector<entt::entity>`.
    *   Iterate over the vector.
    *   **Crucially**: Re-fetch the `SkillExecution` component *inside* the loop for each entity. Do not hold persistent references across hook calls if possible, or verify validity after hooks.
    *   Since we need to modify `exec` (state updates), we must fetch it. If a hook causes reallocation, a fresh `registry.get` is safe *after* the hook, but the reference passed *to* the hook might still be dangerous if the hook triggers the realloc mid-execution.
    *   **Refined Solution**: The hook takes `(registry, entity, exec)`. If the hook triggers realloc, `exec` becomes dangling *inside* the hook immediately after the trigger.
    *   **Best Practice Fix**:
        1.  Collect entities.
        2.  Inside loop: Fetch `exec`.
        3.  Call hooks. *Assumption*: Hooks should be careful, but we can't enforce it easily.
        4.  *Wait*, if `exec` is passed by reference to the hook, and the hook causes realloc, the hook itself crashes when accessing `exec` later.
        5.  **Mitigation**: The critical UAF flagged is the *loop* accessing `exec` after the hook returns (e.g. `exec.state = ...`).
        6.  So, re-fetching `exec` after hooks is the minimum requirement.

### 2.2 Resource Leaks in `GPUFlowFieldSystem`
*   **Problem**: `m_integrationBuffer2` and `m_densityBuffer` are not released in `Shutdown()`.
*   **Risk**: VRAM leak upon system restart or shutdown.
*   **Solution**: Add explicit `Release()` calls.

### 2.3 Performance in `GPUFlowFieldSystem`
*   **Problem**: `std::vector<uint32_t> costInt` is allocated every frame in `Update()`.
*   **Risk**: Unnecessary heap allocation overhead.
*   **Solution**: Promote `costInt` to a member variable `m_costCache` and use `resize/clear`.

## 3. Implementation Details

### 3.1 `SkillSystem::UpdateStates` Refactoring
```cpp
void SkillSystem::UpdateStates(entt::registry &registry, float dt) {
  auto view = registry.view<SkillExecution>();
  
  // 1. Collect entities to avoid iterator invalidation
  std::vector<entt::entity> entities(view.begin(), view.end());

  for (auto entity : entities) {
    if (!registry.valid(entity)) continue;

    // 2. Fetch component anew
    // Note: We might need to check if component still exists, though unlikely to be removed unless destroyed
    auto *execPtr = registry.try_get<SkillExecution>(entity);
    if (!execPtr) continue;

    auto &exec = *execPtr;
    exec.timer -= dt;

    if (exec.timer <= 0.0f) {
        // ... State Machine Logic ...
        // CAUTION: When calling hooks, we pass 'exec'. 
        // If 'hook' triggers realloc, 'exec' reference HERE becomes invalid for the rest of this function scope?
        // Yes.
        
        // SAFE PATTERN for Hook Calls:
        if (exec.state == SkillState::Preparing) {
            // ... hooks ...
            // Re-fetch after hooks if we need to write to it
        }
    }
    // ...
  }
}
```

### 3.2 `GPUFlowFieldSystem` Cleanup
*   Add `Release()` calls to `Shutdown`.
*   Add `std::vector<uint32_t> m_costCache` to class.
*   Update `Update` method to use cached vector.

## 4. Verification
*   **UAF**: Hard to verify without a reproduction case that forces reallocation. We rely on code analysis correctness.
*   **Leaks**: Visual inspection of `Shutdown` and repeated Play/Stop cycles in Editor (if available).
*   **Performance**: Verify no heap churn in profiler (optional, code change is obvious).
