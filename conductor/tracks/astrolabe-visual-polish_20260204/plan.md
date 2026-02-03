# 🌌 星盘视觉修复与优化 - 实现计划

| 属性 | 值 |
|-----|---|
| **Track ID** | `astrolabe-visual-polish_20260204` |
| **优先级** | **HIGH** |
| **状态** | 🚧 IN PROGRESS |
| **预估总工时** | **4-5 小时** |

---

## 任务清单

### Phase 1: 数据基础 (Data Foundation) - 0.5h

- [x] **1.1** 扩展 `profession_talents.json` 测试数据
  - **文件**: `assets/data/profession_talents.json`
  - **内容**: 为 6 个职业各添加 5 个 Tier 1 (Minor) + 2 个 Tier 2 (Major) + 1 个 Tier 3 (Core) 节点
  - **验证**: 加载后 `TalentGraph.nodes.size() >= 48`
  - **预估**: 20min

- [x] **1.2** 验证 `TalentLayoutService::computeNodePositions` 正确性
  - **文件**: `src/game/systems/skill/TalentLayoutService.cpp`
  - **验证**: 各 Tier 节点分布于对应轨道半径 (R2/R3/R4)
  - **预估**: 10min

---

### Phase 2: 节点状态视觉增强 (Node State Rendering) - 1.5h

- [x] **2.1** 重构 `AstrolabeRenderer::DrawNodes` 状态驱动渲染
  - **文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`
  - **修改点**:
    ```cpp
    // 替换现有的简单 DrawCircle 调用为状态分支:
    switch (status) {
        case Locked:    // 暗灰 + 无光晕
        case Available: // 琥珀色 + 脉动光晕 (CPU sin 动画)
        case Activated: // 天蓝 + 进度环
        case FullyActivated: // 金色 + 外发光
        case Sealed:    // 暗紫色
    }
    ```
  - **验证**: 用调试命令手动设置节点状态，观察渲染变化
  - **预估**: 45min

- [x] **2.2** 添加 `getNodeRadius` 辅助函数
  - **文件**: `src/game/systems/ui/AstrolabeRenderer.hpp`
  - **内容**: 根据 `TalentNodeType` 返回对应半径常量
  - **预估**: 10min

- [x] **2.3** 增强 Available 节点脉动效果
  - **文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`
  - **实现**: 利用 `view.time` + `sinf()` 生成 [0.6, 1.0] 区间脉动因子
  - **验证**: 可见琥珀色光晕周期性明暗变化
  - **预估**: 15min

- [x] **2.4** 增强进度环视觉
  - **文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`
  - **修改**: 增加进度环厚度 (r+2 ~ r+5) 和亮度
  - **预估**: 10min

- [x] **2.5** 添加 FullyActivated 外发光效果
  - **文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`
  - **实现**: 双层 DrawCircle (外层淡金色, 内层实金色)
  - **预估**: 10min

---

### Phase 3: 轨道与扇区视觉增强 (Orbit & Sector Enhancement) - 0.5h

- [x] **3.1** 增强轨道环对比度
  - **文件**: `src/game/systems/ui/AstrolabeRenderer.cpp` (`DrawOrbits`)
  - **修改**: 
    - 轨道线透明度: 0.1f → 0.15f
    - 增加轨道描边或使用 `DrawCircleLinesV` 替代
  - **预估**: 15min

- [x] **3.2** 加深扇区分界线
  - **文件**: `src/game/systems/ui/AstrolabeRenderer.cpp` (`DrawOrbits`)
  - **修改**:
    - 颜色: 淡白色 → 淡金色 (`Fade(GOLD, 0.2f)`)
    - 线宽: 1.0f → 2.0f
  - **预估**: 10min

- [x] **3.3** (可选) 添加扇区背景淡色填充
  - **文件**: `src/game/systems/ui/AstrolabeRenderer.cpp`
  - **实现**: 使用 `DrawCircleSector` 绘制交替明暗的 60° 扇形背景
  - **预估**: 15min (如需实现)

---

### Phase 4: 本命星渲染优化 (Profession Star Polish) - 0.5h

- [x] **4.1** 增强誓约状态视觉反馈
  - **文件**: `src/game/systems/ui/AstrolabeRenderer.cpp` (`DrawProfessionStars`)
  - **修改**:
    - 未誓约: 所有本命星淡金色 + 脉动
    - 已誓约 (主修): 金色实心 + 光芒放射
    - 已誓约 (辅修): 暗紫色 + 封印环
  - **预估**: 30min

---

### Phase 5: 编译验证与视觉测试 (Verification) - 0.5h

- [x] **5.1** 编译通过
  - **命令**: `.\build.bat`
  - **验证**: 无编译错误/警告

- [x] **5.2** 运行时视觉检查
  - **验证项**:
    - [x] 6 个本命星可见
    - [x] 节点按 Tier 分布于不同轨道
    - [x] Locked 节点暗淡
    - [x] Available 节点有脉动光晕
    - [x] 点击节点后状态正确切换
    - [x] 扇区分界线和轨道环清晰可辨

- [x] **5.3** 截图对比
  - 与设计文档预期效果对比，确认差距已弥合

---

### Phase 6 (Future): GPU Shader 特效 - Not in Scope

> 以下任务作为后续 Track 规划，当前 Track 采用 CPU fallback 实现

- [ ] 创建 `assets/shaders/talent_node.fs`
- [ ] 实现 Shader 驱动的脉动/发光/封印纹理
- [ ] 迁移 CPU 动画逻辑至 GPU

---

## 工时汇总

| Phase | 描述 | 预估工时 |
|-------|------|----------|
| Phase 1 | 数据基础 | 0.5h |
| Phase 2 | 节点状态视觉 | 1.5h |
| Phase 3 | 轨道与扇区 | 0.5h |
| Phase 4 | 本命星优化 | 0.5h |
| Phase 5 | 编译验证 | 0.5h |
| **总计** | | **3.5h ~ 4h** |

---

## 任务依赖图

```
Phase 1 (数据) ──────────────────────────────────────────────┐
                                                             │
Phase 2 (节点状态) ──┬── 2.1 状态渲染                        │
                     ├── 2.2 辅助函数                        │
                     ├── 2.3 脉动效果 ──┐                    │
                     ├── 2.4 进度环 ────┴─────────────────────┤
                     └── 2.5 外发光                          │
                                                             │
Phase 3 (轨道) ──────┬── 3.1 轨道对比度                      │
                     └── 3.2 扇区分界线                      │
                                                             │
Phase 4 (本命星) ────── 4.1 誓约视觉                         │
                                                             │
Phase 5 (验证) ←─────────────────────────────────────────────┘
```

---

## 状态追踪

| 日期 | 更新内容 |
|------|----------|
| 2026-02-04 | Track 创建，规划完成 |

---

*计划版本: 1.0*
*创建时间: 2026-02-04*
