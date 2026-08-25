# Coordinate System Single Source — 实现审查（初步）

> **Track**: refactor_coordinate_system_20260824
> **Date**: 2026-08-24
> **Status**: ✅ 用户已游戏内验证通过（2026-08-24）；CI 构建仍建议后续补跑
> **审查范围**: Phase 1-4 代码落地（engine-coord / ui-coord / text-msdf / qa-coord）
> **依据**: [坐标系单一来源契约设计](../../plandocs/designs/2026-08-24-coordinate-system-convention-design.md)

---

## 结论（待验证）

**结论：`提交`（用户已游戏内验证无明显问题）**。所有代码已按 R1/R2/R3/R5 落地；本 dsh 沙箱未检测到 MSVC/Visual Studio，无法在此运行构建。用户已在游戏内验证（2026-08-24）未见异常，因此本轮审查判定为 `提交`。后续仍建议在 CI 补跑 `build.bat` + CTest，若发现回归再按本设计回滚/修复。

## 逐条铁律证据

### R1 — 一个转换一个入口
- `src/engine/render/CoordSystem.hpp`：`WorldToScenePixel / ScenePixelToWorld` 成为唯一 world↔screen 实现。
- `MonsterHealthBarController.cpp` 已删除手写 `invZoom` 与 world→screen 公式，改走 `CoordSystem` + `UiViewport`。
- `GameUiHost.cpp` 拖拽幻影改走 `m_viewport.ToLogical`。
- 待办：`GameplayRenderAdapter` / `GPUEntitySystem` / `GPULootSystem` / `OccluderExtractPass` / `TooltipController` / `Astrolabe*` / `InputSystem` 等旧调用点尚未在 Phase 1 全量迁移（后续 Track 继续收口，本 Track 的 guard 至少阻止新增）。

### R2 — MVP 单一来源
- `GPUParticleSystem::BuildMVP` 已委托 `coord::Build2DMvp`，`MatrixOrtho` 不再出现在该生产文件。
- guard 测试禁止 `MatrixOrtho(` 重新出现于生产 render 源。
- 待办：`RenderSystem` 等仍使用 `rlGetMatrixModelview()*rlGetMatrixProjection()` 组合；本 Track 尚未全部替换（属于后续 Phase 1 扩展或独立切片）。

### R3 — Y 翻转只在边界出现
- `GameplayState.cpp` FRAGCOORD 翻转已改 `coord::NativeYToGl`。
- `CoordSystem.hpp` 提供 `RemapTextureV` 作为 UV V 朝向的显式入口。
- 待办：`DrawTexturePro` 负高度、HDR blit 的 target descriptor 翻转尚未全部收敛到 helper（后续 Track）。

### R4 — 字段必须标注 Space
- `CoordSystem.hpp` 新增 `coord::Space` 枚举，作为后续结构注释的规范。
- 待办：`GPULabelInstance` / `GPUGlyphInstance` / `GPUTextQuad` / `GlyphTemplate` 等结构注释的批量标注尚未执行（后续切片）。

### R5 — 度量在导入期归一
- `CoordSystem.hpp` 新增 `MsdfBearingToWorldOffset`，`LootTextBatcher::BuildTemplatesMsdf` 已改用它，保留现有数值语义。
- 测试新增 MSDF helper 往返。

### R6 — 跨域必有 round-trip 测试
- `tests/unit/CoordSystemTests.cpp`：world↔screen、`NativeYToGl` 双射、`Build2DMvp` 等价、MSDF helper。
- `tests/tech/CoordGuardTests.cpp`：R1/R2/R3/R5 防回退。
- 待办：resolution/DRS/HDR 像素矩阵集成 fixture 需后续 qa-coord 扩展。

## 风险与未决项

1. 未执行 `build.bat`；C++ 改动未编译验证。
2. `MonsterHealthBarController` 的 `cmd.width * viewport.Scale()` 逻辑疑似双重缩放，但本 Track 刻意保持现状；后续应单独立项确认。
3. GPU text `Game.cpp` 仍直接拷贝 `bearing` 到 `GPUGlyphMetrics`（em 单位，无 fontSize），不适用 CPU helper；需在 GPU text 路径单独明确这个 em-unit 空间。
4. 旧调用点迁移未全量完成；guard 只保证不新增，不能替代全量替换。

---

## 复审清单（build 后执行）

- [ ] `build.bat` 通过。
- [ ] `bin/NoMoreDayTests.exe --test-case="[Unit]*Coord*"` 通过。
- [ ] `bin/NoMoreDayTests.exe --test-case="[Tech]*CoordGuard*"` 通过。
- [ ] 相关 UI/MSDF 既有测试无回归。
- [ ] 根据结果更新本审查结论。
