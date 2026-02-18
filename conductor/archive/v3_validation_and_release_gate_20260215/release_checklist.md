# V3 Release Checklist

## Scope
- Track: `v3_validation_and_release_gate_20260215`
- Feature flag: `render.v3.enabled`
- Gate runner: `scripts/v3_release_gate.py`

## Pre-release
- [ ] `build.bat`
- [ ] `build.bat analyze`
- [ ] `build.bat perf`
- [ ] `build.bat gate`
- [ ] `python scripts/v3_release_gate.py --update-baseline`

## Mandatory Gate Outputs
- [ ] `bin/release_gate/v3_gate_report.json`
- [ ] `bin/release_gate/v3_gate_report.csv`
- [ ] `bin/release_gate/v3_gate_baseline_snapshot.json`
- [ ] Screenshot report (if enabled): `bin/release_gate/screenshots/screenshot_report.json`
- [ ] If screenshot gate is deferred, carry-over is registered in V4 preflight dependencies (`GPU_Rendering_System_V4.md` §1.4)

## Functional Gate
- [ ] Default framebuffer path validated
- [ ] Offscreen framebuffer path validated
- [ ] Multi-resolution matrix (720p/1080p/1440p) validated
- [ ] Quality tier matrix (Low/Medium/High/Ultra) validated
- [ ] Resize/context-restore/hot-reload checks validated

## Contract Gate
- [ ] ABI snapshot and version checks green
- [ ] Binding registry conflict checks green
- [ ] RenderGraph pass order and ownership checks green
- [ ] Material/VFX schema checks green

## Performance Gate
- [ ] `baseline_270` >= 270 FPS
- [ ] `combat_180` >= 180 FPS
- [ ] `stress_144` >= 144 FPS
- [ ] Pass budget checks green
- [ ] Baseline regression comparator green (`<=10%` regression)
- [ ] Clustered 128-light uplift gate green (`>=5%`) or active approved waiver

## Waiver Governance
- [ ] Active waiver list reviewed: `conductor/validation/v3_gate_waivers.json`
- [ ] Each waiver has owner/reason/expiry/exit criteria
- [ ] Expired waivers are removed or renewed with explicit approval
- [ ] Linked bug items tracked in `conductor/bug_registry.md`

## Stability Gate
- [ ] 30-minute stress script completed
- [ ] VRAM proxy growth within threshold
- [ ] Tier switch stress completed
- [ ] Context restore no black screen regression

## Rollback and Risks
- [ ] Shadow failure injection triggers V2 fallback path
- [ ] Performance regression injection path blocks merge
- [ ] Runtime toggle `render.v3.enabled` switches V3/V2 path
- [ ] Risk checks R-V3-001..R-V3-005 green

## Sign-off
- [ ] Release owner approved
- [ ] Evidence links attached in track `validation.md`
- [ ] Bug registry updated for any failures/regressions
