# 法杖/长柄武器攻击动画 · 搜索关键词手册（中英双语）

- 日期：2026-10-23 ｜ 用途：AV-023 采购连击动画时的平台搜索
- 关键词均在 Fab 实测过（2026-10-23），产出差异见 §4 速查表
- 前置结论（不必再怀疑）：**UE4 Mannequin / UE5 Manny / Mixamo 三类来源对本骨架都通**，搜索时不用按骨架筛选，按内容筛选（见 `../SkeletonDecision_20261023/README.md`）

---

## 1. 核心词汇表（中英对照）

### A. 武器本体词（决定捞到"杖"还是"剑"）

| 中文 | English | 说明 |
|---|---|---|
| 法杖 | staff / magic staff / wizard staff / mage staff | 主力词 |
| 棍、长棍、短棍 | stave / rod / club / stick / cudgel | stave=杖的文学用法，偶有包用 |
| 长棍（武术） | quarterstaff / bo staff / bo | quarterstaff 偏 niche，可淘到武术棍 |
| 长柄武器（统称） | polearm / pole arm / two-handed staff / long staff | 注意：**polearm 在 Fab 上大量命中"武器模型"而非动画** |
| 矛、长戟（动作可借鉴） | spear / glaive / halberd / naginata | 刺击/挥砍动作与法杖通用 |
| 僧棍/武术棍 | monk staff / kung fu staff | 棍花类动作，巫师用偏武侠但可截段 |

> ⚠️ **staff 的歧义坑**：Fab 搜 staff 会命中 "Hotel Staff Animation"（酒店员工）——务必搭配 attack/combo/combat/magic 使用。

### B. 动作/类型词

| 中文 | English |
|---|---|
| 攻击 | attack / attacking |
| 连击、连续技 | combo / combination / chain / multi-hit / sequence |
| 挥舞、挥砍 | swing / slash / swing attack |
| 近战 | melee / melee combat / close combat |
| 战斗 | combat / fight / battle |
| 魔法、施法 | magic / spell / spellcast / casting |
| 动作补间包 | animation pack / animation set / mocap pack / animations |

### C. 组合公式（直接可抄）

```
magic staff animation        staff attack animation      staff combo animation
melee combo animation        combat magic animations     two handed staff animation
spear combat animations      quarterstaff                monk staff animation
mage staff attack            wizard combat animations
```

## 2. 分平台搜索策略

### Fab（主战场，原 UE Marketplace）

- 直接搜 §1.C 的组合词；
- **必须用左侧过滤器**：Asset Type 选 `Animation`（或勾 Rigged）、Engine 选 UE5、按价格排序——否则 staff/magic staff 前缀的词 80% 命中法杖**3D 模型/VFX 包**（如 Naked Singularity 的 Dark Fantasy Staff 系列 €12.92 是模型不是动画）；
- 商品页看 "Technical Details"：Number of Animations / Skeleton / 是否附 IKRig。

### Mixamo（免费备选）

- 搜 `staff`、`polearm`、`club`、`spear`，左侧筛选 Character Animation；
- 下载选 **Without Skin + FBX**，走 IK Retargeter 进 SKE_Wizard（链路已验证）；
- 局限：连击多为单段，需自己截段拼 AnimMontage；通用人形、无持杖特质。

### 其他平台（补充）

| 平台 | 搜什么 | 说明 |
|---|---|---|
| itch.io | `staff animation pack`、`melee combo unreal` | 独立作者，便宜/免费多 |
| Gumroad | 同上 | 同上，注意看许可 |
| Sketchfab | `staff attack animated` | 有带动画的模型，需能导出 FBX |
| ArtStation Marketplace | `staff animation` | 量少，质量参差 |

## 3. 避坑排除词（命中这些直接跳过）

| 排除词 | 为什么 |
|---|---|
| static mesh / 3d model / PBR / game-ready | 只是模型，没动画 |
| VFX / Niagara / particle / ribbon | 只是特效 |
| blueprint / combo system / combo graph / buffering | 只是连击**系统**，不含动画资源（除非你的连击系统也缺，可单独买，如 Melee - Combo System $2.76） |
| unrigged / without skin | 没绑骨架 |
| Hotel Staff | 酒店员工（staff 歧义） |

## 4. 实测速查表（2026-10-23，Fab 实搜）

| 关键词 | 实际产出 | 效率评价 |
|---|---|---|
| **staff combo animation** | Essential Magic Staff、Simple Magic Staff、Mage with Staff + 一堆连击系统书 | ⭐ 找持杖包最高效 |
| **melee combo animation** | Melee Combo Vol 1/2/3（ZzGERTzZ，$48.69/个）、Melee+ (AI+FP/TP+Two Hands)（$41.73）、Freeflow Combat（$111）、Stylish Action Combat（$20.85）、Brawler Animations（免费/$5.54） | ⭐ 找连击结构最高效 |
| **staff attack animation** | 混入大量斧/剑/僵尸模型包；捞出 Simple Magic Staff、Mage with Staff、Spear Animations Pack、Cool Spear Combat（$15.03）、Combat Magic Animations | 可用，需人工筛 |
| magic staff animation | 几乎全是法杖模型/VFX 包 | ❌ 低效，别用 |
| polearm animation | 几乎全是长柄武器模型；仅 Spear Combat/Fishing/Survival Mocap Pack（nikoff，$21.48）相关 | ❌ 低效，别用 |
| quarterstaff | 模型为主；可淘到 G2: Monk（Quang Phan，武术棍角色）、Oriental Dragon Staff |  niche，偶尔有宝 |

**本次实测新发现（待核实，已同步 TechnicalDesign §5.8.1）**：Combat Magic Animations（SoerGame）、Spear Combat Mocap Pack（nikoff $21.48）、Melee+ Two Hands（$41.73）、G2: Monk（Quang Phan）、4 Weapons Essential Animations（Gaming Maestro $62.60，疑似含杖）。

## 5. 推荐搜索流程（约 30 分钟）

1. **Fab → `staff combo animation`** + 过滤器（Animation/Rigged/UE5）→ 法杖专用包（重点看 Essential Magic Staff 的连击段数）；
2. **Fab → `melee combo animation`** → 连击结构完整的通用包（空手包需 KI-013 握持修复兜底，已就绪）；
3. **Fab → `combat magic animations` / `spear combat animations`** → 魔法/长柄补充；
4. **Mixamo → `staff` / `polearm`** → 免费备选；
5. 每个候选过 §6 检查清单。

## 6. 下单前检查清单

- [ ] **骨架兼容**：页面写 UE4 Mannequin / UE5 Manny / Epic skeleton 任一即可（三者对本骨架等价）；没写也能用，走 IK Retargeter；
- [ ] **是否持杖**：空手包 = 需要 weapon_r + 手指层兜底（KI-013 方案已备好，不额外花钱）；
- [ ] **连击段数**：满足 3/4/5 连击设计；单段包能否截段拼 AnimMontage；
- [ ] **根运动**：是否含 root motion、与原动画是否一致（位移叠加/丢失风险）；
- [ ] **格式**：FBX（可自选 skeleton 导入）还是 .uasset 包（走 IK Retargeter）；
- [ ] **许可**：允许商用（作品集公开前必须闭合授权链）；
- [ ] **价格**：Fab 价格随地区货币显示不同，以结账页为准。

## 7. 相关文档

- 采购候选表：`../TechnicalDesign.md` §5.8.1
- 骨架兼容决策：`../SkeletonDecision_20261023/README.md`
- 握持修复（空手包必备）：`../KI013_FingerGrip_20261022/README.md`
