# Rendering Performance Validation Plan

## Phase 1: Benchmark Implementation
**Est. Time**: 0.5 Days

- [x] **Task 1.1: Create Benchmark Test Suite**
    - Create `tests/performance/RenderingBenchmark.cpp`.
    - Implement `Scenario A` (Particles), `Scenario B` (Popups), and `Scenario C` (Entities) using `doctest` and `ScopedTimer`.
    - Ensure tests can run in a "headless" or minimal window mode if possible, or standard windowed mode.

## Phase 2: Execution & Analysis
**Est. Time**: 0.2 Days

- [x] **Task 2.1: Run Benchmarks**
    - Execute the benchmark suite.
    - Capture output logs.
- [x] **Task 2.2: Analyze Results**
    - Compare against targets defined in `spec.md`.
    - Document findings in a report.

## Phase 3: Regression & Audit
**Est. Time**: 0.3 Days

- [x] **Task 3.1: Run Full Test Suite**
    - Execute `./build/bin/NoMoreDayTests.exe`.
    - Fix any discovered regressions.
- [x] **Task 3.2: Final Architecture Audit**
    - Activate `architecture-auditor` to review `GPUParticleSystem.cpp`, `PopupRenderer.cpp`, and `GPUEntitySystem.cpp`.
- [x] **Task 3.3: Documentation**
    - Update `GEMINI.md` memories with key performance characteristics.
    - Close this track.

## Total Estimated Time: 1 Day
