# Phase 1 / A1-4 Residual Risks

## Open risks

1. Runtime VFX loading in `src/engine/vfx/VFXSequenceManager.cpp` still accepts older schema versions for compatibility, so migration tool enforcement alone does not fully prevent old-format runtime intake.
2. The offline migration slice covers VFX sequence JSON only; other render-adjacent legacy data/config files may still require separate migration/drop passes.
3. The current rejected-format check is header-focused (`schema_version` without `vfx_schema_version`); additional historical variants may exist outside current inventory visibility.

## Mitigation in next slices

- Tighten runtime compatibility window once assets are fully migrated to schema `3`.
- Extend inventory and migration tooling coverage to additional render-related config domains (materials/post-process/VFX recipe variants).
- Expand rejected-format fixtures with archived examples to keep the drop list explicit and test-backed.
