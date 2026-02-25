# Combat Anti-Meta Layer — Specification

> Track ID: `combat_anti_meta_layer_20260225`  
> Series: CS-M3-01 | Priority: P2 | Milestone: M3

---

## 1. Overview

建立构筑约束层，防止无限叠加导致单一 meta 垄断。

## 2. Mechanisms

### 2.1 互斥 Keystone
- 特定 Keystone 节点互斥（如 "全转火" vs "全转冰"）。
- 选择一个自动禁用另一个。

### 2.2 代价词缀
- 高收益词缀附带代价（如 +50% 暴击 → -20% 攻速）。
- 代价在 UI 和计算中均可见。

### 2.3 收益递减
- 相同来源的增伤在高叠加时递减。
- 公式：`effective = base * (1 - e^(-actual / scale))`。

## 3. Acceptance Criteria

- [ ] 至少 2 组互斥 Keystone 可正确互斥。
- [ ] 递减机制在高叠加时生效且可配置。
- [ ] 四流派强度 P50 差距 ≤ 15%，P90 差距 ≤ 30%（以 Telemetry DPS 统计为准）。
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

## 4. Risks

| Risk | Mitigation |
|---|---|
| 递减公式选型影响体感 | A/B 测试，提供多种预设曲线 |
