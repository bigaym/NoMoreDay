# Blade Ascendant VFX Keystone / Trigger / Synergy - Implementation Plan

> **Track ID**: `blade_ascendant_vfx_keystone_trigger_20260222`  
> **Status**: [x] Completed

---

## Phase Overview

| Phase | Name | Core Output | Status |
|---|---|---|---|
| **Phase 1** | Event + Selector Rules | `nodeRoleMask` and selector precedence hardening | [x] |
| **Phase 2** | Keystone VFX | Keystone activate and sustained-state differentiation | [x] |
| **Phase 3** | Trigger/Synergy VFX | Trigger templates + synergy secondary feedback + trigger storm control | [x] |
| **Phase 4** | Verification | Stress validation evidence and track documentation | [x] |

---

## Phase 1: Event + Selector Rules (4 Tasks)

- [x] Task 1.1: Align `nodeRoleMask` role bits with current contracts (`Keystone/Trigger/Synergy/Transmuter`).
- [x] Task 1.2: Add selector precedence rule `Keystone > Trigger > Synergy > Base` in runtime recipe matching.
- [x] Task 1.3: Align `castId` semantics for Trigger feedback and prevent duplicate playback from the same source cast.
- [x] Task 1.4: Define per-skill trigger caps and sampling strategy for high-frequency TriggerProc bursts.

## Phase 2: Keystone VFX (4 Tasks)

- [x] Task 2.1: Land 2-3 representative keystone templates for shape-change activation.
- [x] Task 2.2: Add KeystoneActivate composite VFX (impact ring + afterimage/trail + distortion pulse style).
- [x] Task 2.3: Separate sustained Keystone (`BuffEnter`/`BuffExit`) visuals from instantaneous activate burst.
- [x] Task 2.4: Keep tier fallback safe (Low keeps readable highlight, distortion gated by quality policy).

## Phase 3: Trigger/Synergy VFX (6 Tasks)

- [x] Task 3.1: Define a reusable TriggerProc template (flash + spark + short tail).
- [x] Task 3.2: Keep per-skill TriggerProc readability through existing per-skill recipe variations.
- [x] Task 3.3: Apply weaker secondary feedback for Synergy-triggered events (non-primary path).
- [x] Task 3.4: Add trigger storm control via sampling + carry merge strategy under concurrency pressure.
- [x] Task 3.5: Keep compute safety contract intact (`VFXPass` receives prepared data only; no new compute dispatch).
- [x] Task 3.6: Build and minimal CI verification (`build.bat` + `ctest -L ci`).

## Phase 4: Verification (2 Tasks)

- [x] Task 4.1: Validate trigger-storm stability and cap behavior under performance-tagged scenario.
- [x] Task 4.2: Record implementation and verification evidence into `validation.md`.

---
_Updated by Feature Developer._

