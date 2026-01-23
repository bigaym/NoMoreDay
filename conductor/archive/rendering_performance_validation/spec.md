# Rendering Performance Validation Spec

## 1. Objective
To rigorously validate the performance and stability improvements achieved in the "Rendering Performance & Sync Optimization" track. This involves quantitative benchmarking against defined targets and ensuring no regression in existing functionality.

## 2. Validation Scope
The validation will cover the three optimized subsystems:
1.  **GPUParticleSystem**: Verify async update performance and particle capacity.
2.  **PopupRenderer**: Verify Instanced Rendering performance and visual correctness.
3.  **GPUEntitySystem**: Verify Dirty Flag logic and Shadow Buffer upload speed.

## 3. Benchmark Scenarios

### 3.1 Scenario A: Particle Stress Test
- **Configuration**:
  - Max Particles: 100,000
  - Emission Rate: 10,000 / sec
  - Duration: 10 seconds
- **Metrics**:
  - `GPUParticle_Update` CPU Time (Mean, 1% Low)
  - **Target**: < 0.5ms (CPU)

### 3.2 Scenario B: Popup Spam Test
- **Configuration**:
  - Active Popups: 500+
  - New Popups: 50 / frame
- **Metrics**:
  - `PopupRenderer::Render` CPU Time
  - **Target**: < 0.3ms (CPU submit time, adjusted for Integrated Graphics)

### 3.3 Scenario C: Entity Horde Test
- **Configuration**:
  - Entity Count: 20,000
  - Moving Entities: 50% (10,000)
- **Metrics**:
  - `GPUEntity_Update` CPU Time (Sync + Upload)
  - **Target**: < 3.0ms (Baseline for full simulation step on Integrated Graphics)

## 4. Regression Testing
- **Suite**: `NoMoreDayTests.exe`
- **Focus Areas**:
  - `RenderSystemTest`
  - `ParticleSystemTest`
  - `CombatSystemTest` (indirectly tests popups)

## 5. Acceptance Criteria
1.  All benchmark targets met on the test machine.
2.  All unit and integration tests pass.
3.  `architecture-auditor` approves the final code state (no new threading/memory risks).
