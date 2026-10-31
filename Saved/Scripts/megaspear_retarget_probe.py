# Mega Spear 重定向 API 探测（编辑器在线时由 AI 经桥接调用）
# 目的：确定"AnimSequence -> SKE_Wizard"批量重定向走哪条路：
#   A) duplicate_asset + set_editor_property("skeleton") —— 骨名相同时最干净
#   B) IK Retargeter（Python API）
#   C) 仅能手动（编辑器右键 Retarget Anim Assets）
# 输出: D:/UEProject/ArcaneVanguard/Saved/Logs/megaspear_retarget_probe.json
import unreal, json

OUT = "D:/UEProject/ArcaneVanguard/Saved/Logs/megaspear_retarget_probe.json"
PACK_ROOTS = [
    "/Game/SpearAnimationsV2",
    "/Game/Wizard/Animations/_Imported/MegaSpear",
    "/Game/MegaSpear",
    "/Game/MegaSpearAnimationPack",
    "/Game/Mega_Spear_Animation_Pack",
    "/Game/Animations/MegaSpear",
]
WIZARD_MESH = "/Game/Wizard/Mesh/SK_wizard.SK_wizard"

out = {}

# ---- 1. 模块级 API 清点 ----
out["module_retarget_hits"] = sorted([n for n in dir(unreal) if "retarget" in n.lower()])
out["module_anim_hits"] = sorted([n for n in dir(unreal) if "anim" in n.lower() and ("librar" in n.lower() or "util" in n.lower() or "library" in n.lower())])

for cls_name in ["AnimationBlueprintLibrary", "EditorAnimLibrary", "IKRigController",
                 "IKRetargeterController", "RetargetController", "AnimLibrary"]:
    try:
        cls = getattr(unreal, cls_name)
        out["cls_" + cls_name] = sorted([m for m in dir(cls) if not m.startswith("_")])
    except Exception as e:
        out["cls_" + cls_name] = "MISSING"

try:
    out["ikretargeter_asset"] = sorted([m for m in dir(unreal.IKRetargeter) if not m.startswith("_")])
except Exception as e:
    out["ikretargeter_asset"] = "ERR %s" % e

# ---- 2. 功能测试 A: duplicate + set skeleton ----
wizard_mesh = unreal.EditorAssetLibrary.load_asset(WIZARD_MESH)
wizard_skel = wizard_mesh.get_editor_property("skeleton") if wizard_mesh else None
out["wizard_skeleton"] = wizard_skel.get_path_name() if wizard_skel else "NOT_FOUND"

# 找包内第一个 AnimSequence（优先 Character 文件夹的角色动画）
pack_anim = None
pack_root = None
cands = []
for r in PACK_ROOTS:
    try:
        assets = list(unreal.EditorAssetLibrary.list_assets(r, recursive=True, include_folder=False))
    except Exception:
        assets = []
    for p in assets:
        try:
            a = unreal.EditorAssetLibrary.load_asset(p)
        except Exception:
            continue
        if a is not None and a.get_class().get_name() == "AnimSequence":
            pri = 0 if "/Animations/Character/" in p else 1
            cands.append((pri, p, a))
    if cands:
        pack_root = r
        break
if cands:
    cands.sort(key=lambda x: x[0])
    pack_anim = cands[0][2]
out["pack_root"] = pack_root
out["pack_anim"] = pack_anim.get_path_name() if pack_anim else None

if pack_anim and wizard_skel:
    src = pack_anim.get_path_name()
    dst_dir = "/Game/Wizard/Animations/_Imported/_retarget_test"
    dst = dst_dir + "/_probe_temp"
    test = {"src": src, "dst": dst}
    try:
        dup = unreal.EditorAssetLibrary.duplicate_asset(src, dst)
        test["duplicate"] = "OK %s" % (dup.get_path_name() if dup else "None")
    except Exception as e:
        test["duplicate"] = "ERR %s" % e
        dup = None
    if dup:
        try:
            dup.set_editor_property("skeleton", wizard_skel)
            test["set_skeleton"] = "OK"
        except Exception as e:
            test["set_skeleton"] = "ERR %s" % e
        # 验证是否生效
        try:
            test["skeleton_after"] = str(dup.get_editor_property("skeleton").get_path_name())
        except Exception as e:
            test["skeleton_after"] = "ERR %s" % e
        # 清理
        try:
            unreal.EditorAssetLibrary.delete_asset(dst)
            test["cleanup"] = "OK"
        except Exception as e:
            test["cleanup"] = "ERR %s" % e
    out["test_A_duplicate_set_skeleton"] = test

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print("PROBE_DONE")
