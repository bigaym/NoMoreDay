# Archived Project Tracks

This file contains completed and archived tracks.

---

## [x] Track: V3 Clustered 2D Lighting (v3_clustered_lighting_20260215)
- **Folder:** [./conductor/archive/v3_clustered_lighting_20260215/](./conductor/archive/v3_clustered_lighting_20260215/)
- **Description:** 完成 V3 Clustered Lighting：cluster 数据结构与 ABI、compute culling pass、LightingPass clustered 集成、fallback/同步屏障、单元/集成/性能矩阵。
- **Status:** COMPLETED (2026-02-18)
- **Priority:** P0
- **Archive Reason:** P1-P4 已实现并验证；性能门禁调整为无回归硬约束，`>=5%` 提升目标并入 `v3_material_lighting_depth_20260215` 持续优化。

---

## [x] Track: V3 Shadow Pipeline (v3_shadow_pipeline_20260215)
- **Folder:** [./conductor/archive/v3_shadow_pipeline_20260215/](./conductor/archive/v3_shadow_pipeline_20260215/)
- **Description:** 完成 V3 Shadow Pipeline：High 档 SDF 路径、Ultra 档 Atlas+SDF Hybrid、关键光 Top-N 选择、确定性 Atlas 淘汰与滞回、Shadow 失败回退到 V2 Lighting、同步屏障与 Tier 联动。
- **Status:** COMPLETED (2026-02-17)
- **Priority:** P0
- **Archive Reason:** Track 全阶段与 DoD 已完成；`build.bat`、`build.bat analyze`、`build.bat perf` 通过，新增 Shadow integration/performance 用例验证通过。

---

## [x] Track: V3 Baseline Contracts (v3_baseline_contracts_20260216)
- **Folder:** [./conductor/archive/v3_baseline_contracts_20260216/](./conductor/archive/v3_baseline_contracts_20260216/)
- **Description:** 完成 V3 基线契约落地：RenderConfig V3 字段与 Feature Flag、GPU ABI V3、Binding 治理、RenderGraph 顺序/Ownership 合同、Tier 能力矩阵与降级序列常量。
- **Status:** COMPLETED (2026-02-17)
- **Priority:** P0
- **Archive Reason:** Track 全阶段与 DoD 已完成；`build.bat`、`build.bat analyze`、`ctest unit/integration/performance` 验证通过。

---

## [x] Track: Render Risk Closure and MSVC Hard Cutover (render-risk-msvc-hardening_20260215)
- **Folder:** [./conductor/archive/render-risk-msvc-hardening_20260215/](./conductor/archive/render-risk-msvc-hardening_20260215/)
- **Description:** 完成 MSVC-only 工具链硬切换、静态分析入口稳定化、MDI 基准契约场景化改造、VFX 材质回退噪声清理，以及构建测试产物路径统一到根 `bin/`。
- **Status:** COMPLETED (2026-02-15)
- **Priority:** P0
- **Archive Reason:** Track 全部阶段（1-5）完成，`build.bat` / `build.bat analyze` / `build.bat perf` 均通过，验证证据已归档。

---

## [x] Track: 材质与 VFX 序列器 (material_vfx_sequencer_20260213)
- **Folder:** [./conductor/archive/material_vfx_sequencer_20260213/](./conductor/archive/material_vfx_sequencer_20260213/)
- **Description:** 实现数据驱动的材质系统（constexpr预设+JSON+SSBO）、VFX序列器（JSON时间线→多层视觉编排）和屏幕扭曲通路（DistortionPass），使新技能特效可纯数据驱动创建。
- **Status:** COMPLETED (2026-02-13)
- **Priority:** HIGH
- **Archive Reason:** 完成 Phase 4 所有开发与验证任务，包括粒子系统修复 (BUG-20260213-001) 与画质分级确认。

---

## [x] Track: HDR + 后处理管线 (hdr_postprocess_pipeline_20260212)
- **Folder:** [./conductor/archive/hdr_postprocess_pipeline_20260212/](./conductor/archive/hdr_postprocess_pipeline_20260212/)
- **Description:** 实现完整 HDR 渲染链：RGBA16F 场景缓冲区、Bloom 提取与模糊、Tone Mapping、FXAA 抗锯齿以及暗角特效，并确立性能基准。
- **Status:** COMPLETED (2026-02-13)
- **Priority:** HIGH
- **Archive Reason:** 完成并通过 Architecture Audit 审计，性能达标。

---

## [x] Track: 动态 2D 光照系统 (dynamic_lighting_system_20260212)
- **Folder:** [./conductor/archive/dynamic_lighting_system_20260212/](./conductor/archive/dynamic_lighting_system_20260212/)
- **Description:** 引入动态 2D 光照系统，包括 GPULight SSBO、LightManager、LightingPass 以及相应的 Shader 实现，支持多光源剔除与优先级排序。
- **Status:** COMPLETED (2026-02-13)
- **Priority:** HIGH
- **Archive Reason:** 完成并通过 Architecture Audit 审计，性能达标（256 光源 < 1.0ms）。

---
## [x] Track: Rendering Foundation Migration (rendering_foundation_migration_20260212)
- **Folder:** [./conductor/archive/rendering_foundation_migration_20260212/](./conductor/archive/rendering_foundation_migration_20260212/)
- **Description:** Complete Phase 0 rendering foundation migration: RenderGraph scaffolding, RenderPass拆分（Scene/VFX/UIWorld/Composite）, TransientResourcePool, QualityTierManager, ScopedGLState guard, and integration regression test.
- **Status:** COMPLETED (2026-02-12)
- **Priority:** HIGH
- **Archive Reason:** Completed - task delivered and validated in build + tests

---
## [x] Track: Performance Test Expansion (performance_test_expansion_20260210)
- **Folder:** [./conductor/archive/performance_test_expansion_20260210/](./conductor/archive/performance_test_expansion_20260210/)
- **Description:** Expand performance coverage from 9 to 21 systems, covering per-frame hot paths (combat, physics, AI, projectile, skill) and event/loading paths (spawn, save/load, item factory, fog/flow GPU systems), and establish baseline metrics.
- **Status:** COMPLETED (2026-02-10)
- **Priority:** P1 MEDIUM
- **Estimated Time:** 2-3 days
- **Archive Reason:** Completed - Work finished successfully

---
## [x] Track: 鍓戜慨鎶€鑳借涓轰慨澶嶄笌琛ュ叏 (blade_ascendant_fix_20260210)
- **Folder:** [./conductor/archive/blade_ascendant_fix_20260210/](./conductor/archive/blade_ascendant_fix_20260210/)
- **Description:** 淇鍓戜慨鎶€鑳藉疄鐜颁笌 `skills.json` 鐨勫亸宸紝瀹屾垚 ID 甯搁噺鍖栥€佸叧閿?ID 閿欎綅淇銆佸厓绱犲垎鏀ˉ鍏ㄣ€佺己澶辨満鍒惰惤鍦颁互鍙婅嚜鍔ㄥ寲 ID 瀹堝崼娴嬭瘯锛涘苟瀹屾垚娓告垙鍐呭洖褰掗獙璇侊紙鍚伒鍓戝彂灏勬姇灏勭墿棰滆壊缁ф壙淇锛夈€?
- **Review:** [blade_ascendant_review_20260210.md](../reviews/blade_ascendant_review_20260210.md)
- **Status:** COMPLETED (2026-02-10)
- **Priority:** P0 URGENT
- **Estimated Time:** 3-4 days (~21h)

---

## [x] Track: fix-portal-rift-resume_20260208
- **Folder:** [./conductor/archive/fix-portal-rift-resume_20260208/](./conductor/archive/fix-portal-rift-resume_20260208/)
- **Description:** 淇缁村害瑁傞殭鎭㈠娴佺▼锛屽寘鎷户缁?鏂板紑纭銆佸洖鍩庣偣鎭㈠銆?灞傚畬鎴愬悗鐨勯噸鎷兼帴鍒嗘祦銆丳ortal 瀹夊叏鍒犻櫎锛屼互鍙婄浉鍏崇ǔ瀹氭€ч棶棰橈紙瀛椾綋銆佸洖鍩庨棬銆佹殏鍋滀笌姝讳骸鍥炲煄閫昏緫锛夈€?
- **Status:** COMPLETED (2026-02-08)
- **Priority:** P0

---

## [x] Track: TODO Closure Core Loop (2026-02-08)
- **Folder:** [./conductor/archive/todo-closure-core-loop_20260208/](./conductor/archive/todo-closure-core-loop_20260208/)
- **Description:** 闂幆淇鏍稿績寰幆涓殑閬楃暀 TODO銆傚寘鎷?SkillSystem 鍙嶅嚮浼ゅ鎺ュ叆 DamagePipeline锛屼互鍙?SaveManager 瀛樻。澶翠俊鎭紙鐜╁鍚嶃€佹父鐜╂椂闀匡級鐨勫姩鎬佸寲瀹炵幇銆?
- **Status:** COMPLETED (2026-02-08)
- **Priority:** P0
- **Estimated Time:** 8-10 hours

---

## [x] Track: Astrolabe Logic Completion & Combat Integration
- **Folder:** [./conductor/archive/astrolabe-logic-completion_20260205/](./conductor/archive/astrolabe-logic-completion_20260205/)
- **Description:** 瑙ｅ喅澶╄祴鏁板€肩缉鏀句笉鍖归厤銆佹牳蹇冩満鍒剁粍浠讹紙濡傚浘/鍓戞剰/鍓戝績锛夋寕杞藉け鏁堝強鐗规畩杞崲鏁堟灉缂哄け鐨勯棶棰樸€傜‘淇濆ぉ璧嬫槦鐩橀€昏緫涓庢垬鏂楃郴缁熸繁搴﹁€﹀悎銆?
- **Status:** COMPLETED (2026-02-08)
- **Priority:** HIGH
- **Estimated Time:** 2-3 days

---

## [x] Track: Astrolabe VFX & Architecture Polish
- **Folder:** [./conductor/archive/astrolabe-vfx-polish_20260204/](./conductor/archive/astrolabe-vfx-polish_20260204/)
- **Description:** 琛ュ叏鏄熺洏绯荤粺鐨勮瑙変笌鏋舵瀯浼樺寲銆傚寘鎷疄鐜伴珮绾?GPU 鑺傜偣鐫€鑹插櫒 (talent_node.fs)銆佽兘閲忔祦鍔ㄧ矑瀛愬弽棣堛€佽秴鏂版槦鐐规弧鐗规晥銆傚悓鏃朵慨澶?CMake 鏋勫缓绯荤粺鏃犳硶璇嗗埆鏂版祴璇曠殑闂锛屽苟瀵?UIAstrolabe 杩涜鑱岃矗瑙ｈ€﹂噸鏋勩€?
- **Status:** COMPLETED (2026-02-05)
- **Priority:** HIGH
- **Estimated Time:** 1.5 days

---

## [x] Track: Astrolabe Audit Polish (Post-Refactor Fixes)
- **Folder:** [./conductor/archive/astrolabe-polish_20260204/](./conductor/archive/astrolabe-polish_20260204/)
- **Description:** 淇鏄熺郴澶╄祴绯荤粺閲嶆瀯鍚庣殑瀹¤缂洪櫡銆傚寘鎷細(FIX-1) 瑾撶害浜屾纭鏈哄埗銆?FIX-2) 娴嬭瘯鏁版嵁 ID 涓€鑷存€с€?FIX-3) AstrolabeSystem 鍗曞厓娴嬭瘯瑕嗙洊銆?FIX-4) 瑙ｉ攣澶辫触 UI 鍙嶉銆?FIX-5) GPU 鑺傜偣鐗规晥 Shader銆?FIX-6) 鍐椾綑 API 娓呯悊銆?
- **Status:** COMPLETED (2026-02-04)
- **Priority:** HIGH
- **Estimated Time:** 10-12 hours

---

## [x] Track: Astrolabe System Refactor (Six-Sector Layout)
- **Folder:** [./conductor/archive/astrolabe-refactor_20260204/](./conductor/archive/astrolabe-refactor_20260204/)
- **Description:** 灏嗗ぉ璧嬬郴缁熶粠鏄熷骇渚濊禆妯″瀷閲嶆瀯涓哄叚鎵囧尯鍚屾簮甯冨眬銆傚紩鍏ヤ簡鍔ㄦ€佸潗鏍囪绠椼€佽亴涓氫翰鍜屽害瑙ｉ攣鏈哄埗浠ュ強涓讳慨瑾撶害闄愬埗銆?
- **Status:** COMPLETED (2026-02-04)
- **Priority:** HIGH
- **Estimated Time:** 3-4 days

---

## [x] Track: Void Astrolabe (Initial Implementation)
- **Folder:** [./conductor/archive/ui-astrolabe_20260201/](./conductor/archive/ui-astrolabe_20260201/)
- **Description:** Implement a Grim Dawn-inspired "Void Astrolabe" UI. This involves an infinite canvas, star-map aesthetics, and a graph-based data structure.
- **Status:** COMPLETED (2026-02-01)
- **Priority:** HIGH
- **Estimated Time:** 3-4 days

---

## [x] Track: Skill Specialization System
- **Folder:** [./conductor/archive/ui-skill-spec_20260201/](./conductor/archive/ui-skill-spec_20260201/)
- **Description:** 閲嶆瀯浜嗘妧鑳戒笓绮?UI锛屽疄鐜颁簡绫讳技 Last Epoch 鐨勨€滀腑蹇冩灑绾?+ 4 鍒嗘敮鈥濆竷灞€銆?
- **Status:** COMPLETED
- **Priority:** HIGH
- **Estimated Time:** 2-3 days

---

## [x] Track: Dimensional Level Selection & Fragment Failsafe
- **Folder:** [./conductor/archive/dimensional-level-select_20260131/](./conductor/archive/dimensional-level-select_20260131/)
- **Description:** 浼樺寲缁村害鍦板浘鐨勮繘鍏ヤ綋楠屻€傛柊澧炰簡绛夌骇閫夋嫨鐣岄潰锛屽疄鐜颁簡榛樿纰庣墖妫€娴嬫満鍒躲€?
- **Status:** COMPLETED
- **Priority:** HIGH

---

## [x] Track: Lock & Concurrency Optimization
- **Folder:** [./conductor/archive/lock_optimization_20260128/](./conductor/archive/lock_optimization_20260128/)
- **Description:** 浼樺寲椤圭洰涓殑閿佷娇鐢ㄣ€傚紩鍏?ThreadSafeRandom锛屽疄鐜?GPUParticleSystem 鐨勬棤閿佸寲鍙戝皠銆?
- **Status:** COMPLETED

---

## [x] Track: Static Variable & Global State Optimization
- **Folder:** [./conductor/archive/static_variable_optimization_20260128/](./conductor/archive/static_variable_optimization_20260128/)
- **Description:** 绯荤粺鎬ч噸鏋勯」鐩腑鐨勯潤鎬佸彉閲忋€傚紩鍏ヤ簡 `std::shared_mutex` 瑙ｅ喅 StatsCache 骞跺彂鏌ヨ銆?
- **Status:** COMPLETED

---

## [x] Track: Test Case Naming Normalization
- **Folder:** [./conductor/archive/test_normalization_20260126/](./conductor/archive/test_normalization_20260126/)
- **Description:** 缁熶竴 NoMoreDay 椤圭洰涓墍鏈夋祴璇曠敤渚嬬殑鍛藉悕鏍煎紡銆?
- **Status:** COMPLETED

---

## [x] Track: GPU Rendering Pipeline Refactor
- **Folder:** [./conductor/archive/rendering_pipeline_refactor_20260126/](./conductor/archive/rendering_pipeline_refactor_20260126/)
- **Description:** 鍩轰簬鏋舵瀯瀹¤鎶ュ憡锛岀郴缁熸€ч噸鏋?GPU 娓叉煋绠＄嚎銆?
- **Status:** COMPLETED


