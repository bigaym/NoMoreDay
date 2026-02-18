# VFX Preview (V3)

`preview_v3.py` provides:

- Timeline visualization for event distribution.
- Hot-reload diff when JSON file content changes.
- Basic controls: play/pause, seek, and runtime tier switch.

## Usage

```powershell
python tools/vfx_preview/preview_v3.py assets/vfx/templates/v3/melee_slash_flash_v3.json --tier High
```

## Controls

- `Enter`: refresh one step
- `p`: play/pause
- `seek <seconds>`: jump timeline cursor
- `tier <Low|Medium|High|Ultra>`: switch preview tier label
- `q`: quit
