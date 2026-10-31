# /Game/Wizard 资产清理记录

- 时间：2026-10-21 21:10
- 目标：只保留与角色模型相关的资产（网格、贴图、材质、动画、骨骼、物理资产、分件、武器）
- 方式：先做全量引用分析，再经编辑器删除（非直接删文件），最后核对完整性

## 清理前后

| 项目 | 清理前 | 清理后 |
|---|---|---|
| 资产数 | 351 | **193** |
| 体积 | 1965.7 MB | **1852.3 MB** |
| 删除资产数 | — | **158** |
| 释放体积 | — | **113.4 MB** |

## 删除的内容（157 个）

| 路径 | 资产数 | 体积 | 说明 |
|---|---|---|---|
| `Maps/DemoRoom/Materials` | 59 | 5.5 MB | DemoRoom 房间材质 |
| `Maps/DemoRoom/Meshes` | 34 | 13.9 MB | DemoRoom 静态网格 |
| `Maps/DemoRoom/BluePrint` | 11 | 2.6 MB | DemoRoom 交互蓝图 |
| `Maps/DemoRoom/Mannequin/**` | 26 | 12.1 MB | 第三人称 Mannequin（网格/动画/材质/贴图） |
| `Maps/DemoRoom/Textures` | 7 | 3.1 MB | DemoRoom 贴图 |
| `Maps/DemoRoom/ThirdPerson/Meshes` | 6 | 0.6 MB | DemoRoom 第三人称网格 |
| `Maps/DemoRoom/SkeletalMeshes` | 4 | 0.4 MB | 刚体演示骨骼网格 |
| `Maps/DemoRoom/ExampleContent/**` | 5 | 0.1 MB | 音频物理材质、材质实例、接口蓝图 |
| `Maps/Environment/HDRI_Wizard` | 1 | 69.0 MB | 环境 HDRI（体积最大的一项） |
| `Maps/Lookdev/FilmGrain` | 3 | 5.6 MB | 后期胶片颗粒 |
| `Maps/DemoRoom/ThirdPersonBP/Blueprints/ThirdPersonCharacter` | 1 | 0.1 MB | 巫师角色演示蓝图（第二轮追加删除） |
| `Blueprints/BP_Wizard_AreaRectLight` | 1 | 0.4 MB | 矩形光蓝图（非角色资产） |

> 注：`ThirdPersonCharacter` 在第一轮被保留（作为玩家角色的潜在起点），用户确认不需要后于第二轮删除。
> `Maps/` 目录随之整体移除。

## 保留的内容（193 个）

| 路径 | 资产数 | 体积 |
|---|---|---|
| `Mesh` | 12 | 51.5 MB |
| `Mesh/Skin` | （含上） | SK_wizard_Dark / Fire / Ice |
| `Mesh/Weapon` | （含上） | 法杖 ×4、书 ×1 |
| `SeparatedMesh` | 7 | 34.0 MB |
| `Materials` | 57 | 48.5 MB |
| `Textures` | 100 | 1707.2 MB |
| `Animations` | 17 | 11.1 MB |

## 安全性验证

1. **引用分析**：对 158 个待删资产逐个查询 `find_package_referencers_for_asset`，**全部在待删集合之外没有任何引用者**。即删除不会弄断任何保留资产。
2. **删除后完整性**：
   - 关键资产全部可加载：`SK_wizard` / `SKE_Wizard` / `PHY_Wizard` / 三个皮肤 / `AnimBP_Wizard_ThirdPerson`
   - `SK_wizard` 20 个材质槽**全部有效，无空引用**
   - 布料仍挂在 section 10，未受影响
   - 关卡中的 `SK_Wizard` Actor 网格引用完好
3. **残留检查**：资产注册表 193 个资产，**0 个 Redirector**（删除未产生重定向器）。
4. **目录清理**：共删除 29 个空目录，`Maps/` 已整体移除。最终只剩 `Animations` / `Materials` / `Mesh` / `SeparatedMesh` / `Textures` 五个角色相关目录。

## 备份与回滚

删除的 157 个资产已完整备份（保留原目录结构与原始时间戳）：

```
Docs/AssetBackup_2026-10-21/Wizard_Maps_to_delete/
  Maps/...
  Blueprints/...
```

回滚方式：把上述 `Maps/` 与 `Blueprints/` 目录覆盖回 `Content/Wizard/` 下即可，然后在编辑器中刷新资产。

## 遗留

- `Textures` 占 1707 MB（占剩余的 92%），是体积主体。其中包含 Dark / Fire / Ice 三套元素皮肤的贴图。若要进一步瘦身，需要确认哪些皮肤不用。
- 最终目录结构：`Animations` / `Materials` / `Mesh` / `SeparatedMesh` / `Textures`，全部为角色相关内容。
