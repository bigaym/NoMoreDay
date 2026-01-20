# 性能极致优化 - 总体实施计划 (Master Plan)

**Track ID**: `performance_optimization`  
**状态**: 📋 Planned  
**目标帧率**: 180 FPS (帧预算 5.5ms)  
**总预计工时**: 10-12 天

---

## 1. Phase 总览

| Phase | Track ID | 优先级 | 依赖 | 工时 | 收益预估 |
|-------|----------|--------|------|------|----------|
| Phase 1 | `phase1_mdi_rendering` | P0 | 无 | 3-4d | Draw Call → O(1) |
| Phase 2 | `phase2_entt_group` | P1 | 无 | 2d | Cache Miss ↓80% |
| Phase 3 | `phase3_triple_buffer` | P2 | Phase 1 | 2-3d | CPU-GPU 同步 → 0 |
| Phase 4 | `phase4_simd_spatial` | P2 | 无 | 2d | 查询性能 ↑4-6x |
| Phase 5 | `phase5_branchless` | P3 | Phase 2 | 1.5d | 分支失败 ↓95% |

---

## 2. 建议执行策略

### 2.1 并行执行路径

```
Week 1:
├── [Engineer A] Phase 2: EnTT Group ──────────┐
│                                              │
└── [Engineer B] Phase 4: SIMD Spatial ────────┤
                                               │
Week 2:                                        ▼
├── [Engineer A] Phase 1: MDI Rendering ───► Phase 3: Triple Buffer
│
└── [Engineer B] Phase 5: Branchless (依赖 Phase 2 完成)
```

### 2.2 单人执行顺序

若单人执行，建议顺序:

1. **Phase 2 (EnTT Group)** - 2 天
   - 独立且风险低
   - 为 Phase 5 铺垫
   
2. **Phase 4 (SIMD Spatial)** - 2 天
   - 独立且收益明确
   - 可立即验证效果
   
3. **Phase 1 (MDI Rendering)** - 3-4 天
   - 核心优化，工作量最大
   - 需要专注时间
   
4. **Phase 3 (Triple Buffer)** - 2-3 天
   - 依赖 Phase 1 的基础设施
   - GPU 同步知识要求高
   
5. **Phase 5 (Branchless)** - 1.5 天
   - 收尾微优化
   - 依赖 Phase 2

---

## 3. 里程碑检查点

### M1: 基础优化完成 (Phase 2 + Phase 4)
**预计时间**: Week 1 结束

**验收指标**:
- [ ] EnTT Group 遍历已集成
- [ ] SIMD 空间查询已集成
- [ ] 所有现有测试通过

**预期收益**:
- 属性更新耗时 ↓50%
- 空间查询耗时 ↓70%

### M2: GPU Pipeline 优化完成 (Phase 1 + Phase 3)
**预计时间**: Week 2 结束

**验收指标**:
- [ ] MDI 渲染正常工作
- [ ] Triple Buffer 同步正常
- [ ] 10k 实体无卡顿

**预期收益**:
- Draw Call = 1
- CPU-GPU 同步开销 ≈ 0

### M3: 全面优化完成 (All Phases)
**预计时间**: Week 2.5

**验收指标**:
- [ ] 所有 Phase 验收通过
- [ ] 帧时间 < 5.5ms @ 10k 敌人
- [ ] 无新增回归问题

---

## 4. 风险矩阵

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| rlgl 状态冲突 (Phase 1) | 中 | 高 | 绕过 rlgl 使用原生 GL |
| EnTT Group 组件冲突 (Phase 2) | 低 | 中 | 设计时规划好归属 |
| 驱动不支持 Persistent Map (Phase 3) | 低 | 中 | Compat 模式回退 |
| SIMD 指令集兼容 (Phase 4) | 极低 | 低 | xsimd 自动选择 |
| 无分支改动无效果 (Phase 5) | 中 | 低 | Benchmark 验证后再合并 |

---

## 5. 验收标准汇总

### 功能验收
- [ ] 所有现有测试通过
- [ ] 无新增 UB/UAF/内存泄漏
- [ ] Validation Layer 无错误

### 性能验收
| 场景 | 基准 | 目标 |
|------|------|------|
| 10k 敌人帧时间 | > 8ms | < 5.5ms |
| Draw Call 数量 | O(N) | 1 |
| 空间查询 1000 次 | ~0.5ms | < 0.15ms |
| Stats 更新 10k 实体 | TBD | ↓50% |

### 回归验收
- [ ] 游戏逻辑行为一致
- [ ] 渲染结果无差异
- [ ] 存档/读档正常

---

## 6. 文档结构

```
conductor/tracks/performance_optimization/
├── spec.md                     # 总体规格 (本文档关联)
├── plan.md                     # 总体计划 (本文档)
├── phase1_mdi_rendering/
│   ├── spec.md
│   └── plan.md
├── phase2_entt_group/
│   ├── spec.md
│   └── plan.md
├── phase3_triple_buffer/
│   ├── spec.md
│   └── plan.md
├── phase4_simd_spatial/
│   ├── spec.md
│   └── plan.md
└── phase5_branchless/
    ├── spec.md
    └── plan.md
```

---

## 7. 下一步行动

**请确认**:
1. 是否同意建议的执行顺序?
2. 是否需要调整优先级?
3. 是否有额外的约束条件?

确认后可开始 **Phase 2 (EnTT Group)** 的实施。

---

*规划者: Gemini (Skill: designer)*
