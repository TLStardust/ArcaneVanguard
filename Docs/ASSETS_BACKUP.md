# 资产备份清单（不进 Git 的部分）

> 用途：把大体积第三方资产从 Git 仓库中剥离，单独备份到 U 盘 / 网盘。
> 仓库里只留「代码 + 配置 + 文档 + 自研资产」，约 **25 MB**，克隆很快。
> 恢复方法见文末。

---

## 一、需要你复制走的目录（合计约 **2.76 GB**）

| 目录 | 体积 | 是什么 | 为什么不在 Git 里 |
|---|---|---|---|
| `Content/Wizard/` | **1.9 GB** | 巫师模型 + 8K 贴图（100 张）+ 材质 | 第三方资源（UECandy 转载），不会变；8K 贴图单张 96 MB，超 GitHub 单文件限制 |
| `Content/Characters/` | **401 MB** | UE5 模板自带的 Manny / Quinn | 引擎模板内容，可从 UE5 模板或商城重新获取 |
| `Content/Stone_Golem/` | **142 MB** | 敌人石头人模型 + 物理资产 | 第三方资源 |
| `Content/SpearAnimationsV2/` | **75 MB** | 402 个动画资产（攻击/受击/待机/移动） | 第三方动画包 |
| `Docs/AssetBackup_2026-10-21/` | **245 MB** | 2026-10-21 的历史资产备份 | 归档材料，不是工程内容 |

**合计：约 2.76 GB**

---

## 二、不需要备份的目录（可重建，不占你的备份空间）

| 目录 | 体积 | 为什么不备份 |
|---|---|---|
| `Intermediate/` | **4.4 GB** | 编译中间产物。重新打开工程 / 编译时自动生成 |
| `Binaries/` | **546 MB** | 编译产物（DLL/PDB）。重新编译即可（但需要先修 UBT，见 HANDOFF §4） |
| `Saved/` | **172 MB** | 日志、截图、自动保存、崩溃报告。**但 `Saved/Scripts/` 里的脚本已进 Git**（编译脚本要跟着仓库走） |
| `DerivedDataCache/` | 0 | 空的 |

> **如果你不想重新编译**，可以额外备份 `Binaries/Win64/`（546 MB），这样恢复后直接能开编辑器。但注意：**DLL 和源码版本必须匹配**，否则还是会重编。

---

## 三、已经在 Git 里的（约 25 MB，不用备份）

| 目录 / 文件 | 体积 | 说明 |
|---|---|---|
| `Source/` | 498 KB | 全部 C++ 源码（28 文件 / 4,190 行） |
| `Config/` | 44 KB | `DefaultEngine.ini` 等（含碰撞通道配置） |
| `Docs/` | 15 MB | 全部技术文档（含两份审计 + 交接文档） |
| `Content/ArcaneVanguard/` | **5.5 MB** | **自研资产**：BP / BT / BB / GA / GE / 蒙太奇 / 输入 |
| `Content/ThirdPerson/` | 3.3 MB | **`ThirdPersonMap.umap`（关卡）** + 光照构建数据 |
| `Content/LevelPrototyping/` | 272 KB | 模板材质（关卡用到） |
| `Content/__ExternalActors__/` | 312 KB | **World Partition 外部 Actor**，删了关卡内容就丢 |
| `Content/__ExternalObjects__/` | 24 KB | 同上 |
| `ArcaneVanguard.uproject` | 1 KB | 工程描述 |
| `.gitignore` / `.gitattributes` | 4 KB | Git 配置 |
| `Saved/Scripts/` | — | 构建脚本（`build_manual.sh` 等） |

---

## 四、一键打包备份（推荐）

在**工程根目录**打开 PowerShell，执行：

```powershell
# 打包成一个 zip，放到桌面（约 2.5 GB 压缩后）
$src = "D:\UEProject\ArcaneVanguard"
$dst = "$env:USERPROFILE\Desktop\ArcaneVanguard_Assets_20261031.zip"

$paths = @(
    "$src\Content\Wizard",
    "$src\Content\Characters",
    "$src\Content\Stone_Golem",
    "$src\Content\SpearAnimationsV2",
    "$src\Docs\AssetBackup_2026-10-21"
)

Compress-Archive -Path $paths -DestinationPath $dst -CompressionLevel Optimal
Write-Host "完成：$dst"
```

> **压缩率预估**：`.uasset` 已经是压缩过的（zlib/oodle），zip 只能再压 10~20%。如果嫌慢，可以用 `-CompressionLevel Fastest`。

**如果只想复制不压缩**（U 盘容量够）：

```powershell
$src = "D:\UEProject\ArcaneVanguard"
$dst = "E:\AV_Backup_20261031"   # 改成你的 U 盘盘符

robocopy "$src\Content\Wizard"            "$dst\Content\Wizard"            /E /NFL /NDL
robocopy "$src\Content\Characters"        "$dst\Content\Characters"        /E /NFL /NDL
robocopy "$src\Content\Stone_Golem"       "$dst\Content\Stone_Golem"       /E /NFL /NDL
robocopy "$src\Content\SpearAnimationsV2" "$dst\Content\SpearAnimationsV2" /E /NFL /NDL
robocopy "$src\Docs\AssetBackup_2026-10-21" "$dst\Docs\AssetBackup_2026-10-21" /E /NFL /NDL
```

---

## 五、恢复步骤（下次开发 / 换机器）

1. **克隆仓库**
   ```bash
   git clone <你的仓库地址> ArcaneVanguard
   ```

2. **解压资产备份到 `Content/`**
   - 如果备份是 zip：解压后把 `Content/Wizard`、`Content/Characters`、`Content/Stone_Golem`、`Content/SpearAnimationsV2` 放回工程根目录的 `Content/` 下
   - 如果备份是 robocopy 目录：整个目录拷回去即可

3. **验证路径**
   确保这些路径存在：
   ```
   Content/Wizard/Mesh/SK_wizard.uasset
   Content/Stone_Golem/...
   Content/SpearAnimationsV2/...
   Content/Characters/Mannequins/...
   ```

4. **打开编辑器**
   - 首次打开会**扫描资产**（几分钟）
   - 如果仍显示红字（missing），右键 `Content` 文件夹 → **Fix Up Redirectors**
   - 极端情况下：`Content/ArcaneVanguard` 里的 BP 可能因为资产路径变化而断引用，需要手动重指（但路径一致的话不会）

5. **编译**
   - 如果 `Binaries/` 没备份：需要重新编译
   - **先修 UBT 配置**（见 `HANDOFF_20261031.md` §4 的第一个坑）
   - 然后 `bash Saved/Scripts/build_manual.sh`

---

## 六、为什么这么分（决策依据）

**进 Git 的**：**会变的东西** —— 你写的代码、配置、文档、自研蓝图。这些需要历史记录、需要 diff、需要回滚。

**不进 Git 的**：**不会变的东西** —— 第三方资源包。它们从下载那一刻起就固定了，版本控制的唯一价值是"防止误删"，而这用一份 zip 备份就够了。

**代价**：克隆后不能立刻跑，要先解压资产。
**收益**：仓库 25 MB 而不是 2.1 GB，克隆几秒钟，免费额度绰绰有余。

---

## 七、附：如果以后想把这些资产也纳入版本控制

三条路，按成本排序：

| 方案 | 成本 | 说明 |
|---|---|---|
| **GitHub LFS 数据包** | $5/月（50 GB 存储 + 50 GB 流量） | 最省心，克隆即用。但 LFS 流量用完会额外收费 |
| **自建 Gitea + LFS** | 一台服务器 | 完全可控，但要自己运维 |
| **Perforce Helix Core** | 免费版支持 5 用户 / 20 工作区 | UE 官方推荐，**大团队标配**。个人项目偏重，但如果以后组队开发值得考虑 |

**当前阶段不建议**：一个人开发、资产不变、仓库只有 25 MB，没必要为版本控制付费。
