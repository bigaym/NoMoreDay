# CPU-GPU Synchronization Analysis & Optimization Report

## 1. Problem Identification
The reported issues ("flickering ghosts", "random positions", "movement viscosity") were traced to fundamental synchronization flaws in the `GPUEntitySystem` and `PersistentBuffer` implementation.

### 1.1 Root Causes
1.  **Double Buffering Race Condition (Flickering)**:
    - The system used a Double Buffer (Slots 0, 1).
    - **Render** typically reads the *Previous* slot (Result of Frame N-1).
    - **Update** writes the *Current* slot (Frame N).
    - If `Update` runs faster or "catches up" (e.g., inside the Accumulator loop), it can wrap around and start writing to Slot 0 while `Render` is still issuing draw commands for Slot 0.
    - The Fence only waited for *Compute* completion, not *Render* completion.
    - Result: Random positional data being drawn (T-posing, teleports).

2.  **Sync Stalls (Viscosity)**:
    - With Double Buffering, if the GPU is slightly behind, the CPU `BeginWrite` checks the fence and **Stalls**.
    - This locks the CPU frame rate to the GPU frame rate (or worse).
    - If `Update` (Physics) is 60Hz and `Render` is 180Hz, the Stall forces the main loop to wait, killing responsiveness.

3.  **Data Overwrite Loop (Initial Stutter)**:
    - `GPUEntitySystem::SyncBack` reads data from the GPU to update the CPU Registry.
    - On Frame 0 and 1, the GPU buffer contains uninitialized data (All Zeros).
    - `SyncBack` overwrote the CPU Registry with (0,0,0).
    - `Update` then wrote (0,0,0) back to the GPU.
    - Result: All entities teleport to origin at start, then snap back once simulation stabilizes.

## 2. Implemented Solution: Triple Buffering (Phase 3)

We have advanced the `PersistentBuffer` implementation to support **Triple Buffering**, fulfilling Phase 3 of the Performance Track.

### 2.1 Logic Flow (Cycle of 3)
- **Slot 0**: **Display** (Immutable, used by `Render` thread).
- **Slot 1**: **GPU Compute** (Physics simulation running N-1 -> N).
- **Slot 2**: **CPU Write** (Preparing inputs for Frame N+1).

### 2.2 Critical Changes
1.  **Variable Buffer Count**: `PersistentBuffer` now supports `m_bufferCount = 3`.
    - `BindPrevious` (Render) automatically finds the stable "Completed" frame (Slot N-2).
    - `Read` (SyncBack) reads the "Just Finished" frame (Slot N-1).
2.  **Frame Skipping**: Added `m_frameCounter` to `GPUEntitySystem`.
    - `SyncBack` is disabled for the first 2 frames.
    - This allows the pipeline (CPU->GPU->CPU) to fill with valid data before we start overwriting the Registry.
3.  **Legacy Render Fix**:
    - `RenderLegacy` was incorrectly binding the *Writing* slot (`BindBase`).
    - Fixed to use `BindPrevious` to ensure it draws stable data.

## 3. Performance Impact
- **CPU Stalls**: Eliminated. CPU can write Frame N+1 even if GPU is busy drawing Frame N-1.
- **Latency**: Physics Logic Latency is consistent at ~2 frames (33ms at 60Hz), which is standard for async compute.
- **Visuals**: Interpolation/Extrapolation (if added later) will now have stable data sources. Stuttering should be gone.

## 4. Next Steps
- **GPUParticleSystem**: Currently uses a hybrid sync/async model. We recommend moving it to `PersistentBuffer` based Triple Buffering in a future pass to remove the `m_atomicBuffer` CPU readback stall.
