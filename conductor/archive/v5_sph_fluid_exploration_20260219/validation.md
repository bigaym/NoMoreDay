# Validation - v5_sph_fluid_exploration_20260219

## 1. Build & Test Evidence

### 1.1 Build
- `build.bat` ✅ PASS
- `build.bat release notest` ✅ PASS

### 1.2 CTest
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` ✅ PASS
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` ✅ PASS
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` ✅ PASS
- `ctest --test-dir build -C Release -L performance --output-on-failure` ✅ PASS

### 1.3 Focused Tests
- `bin/NoMoreDayTests --test-case="*Fluid SPH reference*"`  
  Result: `2 passed, 0 failed`
- `bin/NoMoreDayTests --test-case="*RenderGraph V5 Contracts*"`  
  Result: `3 passed, 0 failed`
- `bin/NoMoreDayTests --test-case="*FluidSimulation*"`  
  Result: `1 passed, 0 failed`  
  Metrics:
  - `fluid_reference_10k_mean_ms = 1.4305`
  - `fluid_reference_10k_p99_ms = 1.6659`
  - `fluid_reference_10k_target_hit = 0`

## 2. Acceptance Criteria

- `10K 粒子 SPH Pass ≤ 0.80ms (Ultra)` ❌ Not met in current exploration baseline (`1.4305ms`)
- `流体与障碍碰撞` ✅ Compute integrate path includes distance-field collision fallback
- `高温粒子 → Emissive 注入` ✅ Implemented (`v5_fluid_emissive_inject.comp`)
- `高密度液面 → OccluderMask 更新` ✅ Implemented (`v5_fluid_occluder_inject.comp`)
- `render.fluid.enabled=false 资源完全释放` ✅ Runtime path calls `Shutdown()/ReleaseRuntimeBuffers()`
- `不稳定保护 (Leapfrog/CFL 风格保护)` ✅ Delta-time clamp + bounded integration guard

## 3. GO/NO-GO Decision

- Decision: **NO-GO**
- Reason:
  - Core exploration performance gate (`≤0.80ms @10K`) is not met.
  - Feature remains implemented behind `render.fluid.enabled` and can be revisited without blocking V5 core release.

