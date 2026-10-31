# Mega Spear 包扫描 + 骨名比对（资产拷入工程后，由 AI 经桥接调用）
# 输出: D:/UEProject/ArcaneVanguard/Saved/Logs/megaspear_scan.json
import unreal, json

OUT = "D:/UEProject/ArcaneVanguard/Saved/Logs/megaspear_scan.json"
PACK_ROOTS = [
    "/Game/SpearAnimationsV2",
    "/Game/Wizard/Animations/_Imported/MegaSpear",
    "/Game/MegaSpear",
    "/Game/MegaSpearAnimationPack",
    "/Game/Mega_Spear_Animation_Pack",
    "/Game/Animations/MegaSpear",
]

# SKE_Wizard 70 骨（2026-10-22 实测枚举，ki013_weapon_bone_probe2.json）
WIZARD_BONES = [
    "root", "pelvis", "spine_01", "spine_02", "spine_03",
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
    "index_01_l", "index_02_l", "index_03_l",
    "middle_01_l", "middle_02_l", "middle_03_l",
    "pinky_01_l", "pinky_02_l", "pinky_03_l",
    "ring_01_l", "ring_02_l", "ring_03_l",
    "thumb_01_l", "thumb_02_l", "thumb_03_l",
    "weapon_l", "lowerarm_twist_01_l", "upperarm_twist_01_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
    "index_01_r", "index_02_r", "index_03_r",
    "middle_01_r", "middle_02_r", "middle_03_r",
    "pinky_01_r", "pinky_02_r", "pinky_03_r",
    "ring_01_r", "ring_02_r", "ring_03_r",
    "thumb_01_r", "thumb_02_r", "thumb_03_r",
    "weapon_r", "lowerarm_twist_01_r", "upperarm_twist_01_r",
    "neck_01", "head",
    "thigh_l", "calf_l", "calf_twist_01_l", "foot_l", "ball_l", "thigh_twist_01_l",
    "thigh_r", "calf_r", "calf_twist_01_r", "foot_r", "ball_r", "thigh_twist_01_r",
    "ik_hand_root", "ik_hand_gun", "ik_hand_l", "ik_hand_r",
    "ik_foot_root", "ik_foot_l", "ik_foot_r",
]

ATTACK_KW = ["attack", "hit", "strike", "swing", "thrust", "stab", "slash",
             "combo", "jab", "lunge", "smash", "slam", "chop", "spin", "whirl"]
MOVE_KW = ["idle", "walk", "run", "sprint", "jog", "jump", "fall", "land", "crouch"]
OTHER_KW = ["block", "dodge", "roll", "death", "die", "aim", "equip", "draw", "sheathe"]

out = {"pack_root": None}

# ---- 1. 定位包根目录 ----
for r in PACK_ROOTS:
    try:
        assets = list(unreal.EditorAssetLibrary.list_assets(r, recursive=True, include_folder=False))
    except Exception:
        assets = []
    if assets:
        out["pack_root"] = r
        break
if out["pack_root"] is None:
    out["error"] = "PACK_NOT_FOUND: 未在候选路径找到资产，请确认拷贝位置"
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
    raise SystemExit("PACK_NOT_FOUND")

paths = list(unreal.EditorAssetLibrary.list_assets(out["pack_root"], recursive=True, include_folder=False))
out["total_assets"] = len(paths)

# ---- 2. 按类别清点 ----
by_class = {}
anims = []
skeletons = []
meshes = []
blendspaces = []
others = []
for p in paths:
    try:
        a = unreal.EditorAssetLibrary.load_asset(p)
    except Exception as e:
        others.append({"path": p, "load_err": str(e)[:80]})
        continue
    if a is None:
        others.append({"path": p, "load_err": "None"})
        continue
    cn = a.get_class().get_name()
    by_class[cn] = by_class.get(cn, 0) + 1
    name = p.split("/")[-1].split(".")[0]
    if cn == "AnimSequence":
        anims.append({"name": name, "path": p})
    elif cn == "Skeleton":
        skeletons.append(p)
    elif cn == "SkeletalMesh":
        meshes.append(p)
    elif cn.startswith("BlendSpace"):
        blendspaces.append({"name": name, "path": p})
    else:
        others.append({"name": name, "class": cn, "path": p})

out["by_class"] = by_class
out["anim_count"] = len(anims)
out["anims"] = sorted(anims, key=lambda x: x["name"])
out["skeletons"] = skeletons
out["meshes"] = meshes
out["blendspaces"] = sorted(blendspaces, key=lambda x: x["name"])
out["others"] = others

# ---- 3. 动画名关键词分类 ----
def classify(n):
    nl = n.lower()
    for kw in ATTACK_KW:
        if kw in nl:
            return "attack"
    for kw in MOVE_KW:
        if kw in nl:
            return "move"
    for kw in OTHER_KW:
        if kw in nl:
            return "other"
    return "unclassified"

groups = {}
for a in anims:
    c = classify(a["name"])
    groups.setdefault(c, []).append(a["name"])
out["anim_groups"] = {k: sorted(v) for k, v in groups.items()}
out["attack_count"] = len(groups.get("attack", []))

# ---- 4. 包骨架骨名 vs SKE_Wizard ----
pack_bones = None
if meshes:
    world = unreal.EditorLevelLibrary.get_editor_world()
    actor = None
    for loc in [(0, 0, 0), (3000, 3000, 0), (0, 3000, 3000), (-5000, -5000, 1000), (8000, -8000, 0)]:
        a = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SkeletalMeshActor, unreal.Vector(*loc), unreal.Rotator(0, 0, 0))
        if a is not None:
            actor = a
            break
    if actor:
        try:
            comp = actor.skeletal_mesh_component
            comp.set_editor_property("skeletal_mesh", unreal.EditorAssetLibrary.load_asset(meshes[0]))
            n = comp.get_num_bones()
            pack_bones = [str(comp.get_bone_name(i)) for i in range(n)]
        except Exception as e:
            out["pack_bones_err"] = str(e)[:200]
        unreal.EditorLevelLibrary.destroy_actor(actor)

if pack_bones:
    pb = set(pack_bones)
    wb = set(WIZARD_BONES)
    out["pack_bone_count"] = len(pack_bones)
    out["pack_bones"] = pack_bones
    out["bone_match"] = {
        "matched": sorted(pb & wb),
        "only_in_pack": sorted(pb - wb),
        "only_in_wizard": sorted(wb - pb),
        "match_ratio": "%.1f%%" % (100.0 * len(pb & wb) / max(1, len(wb))),
    }
else:
    out["bone_match"] = "PENDING: 包内无 SkeletalMesh，改用 ki013_nametable.py 离线解析 Skeleton.uasset 名称表"

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print("SCAN_DONE root=%s anims=%d" % (out["pack_root"], len(anims)))
