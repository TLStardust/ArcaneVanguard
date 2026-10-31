# 批量重定向测试：A_2H_Idle_01 -> SKE_Wizard，并验证姿态正确性
import unreal, json, math

OUT = "D:/UEProject/ArcaneVanguard/Saved/Logs/megaspear_retarget_test.json"
ANIM_SRC = "/Game/SpearAnimationsV2/Animations/Character/2Hand/Pose01/A_2H_Idle_01"
RT = "/Game/Wizard/Animations/_Retarget/RT_Mannequin_to_Wizard"
SRC_MESH = "/Game/SpearAnimationsV2/Mannequin/Character/Mesh/SK_Mannequin"
TGT_MESH = "/Game/Wizard/Mesh/SK_wizard"
T = 0.5

out = {}
src_mesh = unreal.EditorAssetLibrary.load_asset(SRC_MESH)
tgt_mesh = unreal.EditorAssetLibrary.load_asset(TGT_MESH)
rt = unreal.EditorAssetLibrary.load_asset(RT)
ad = unreal.EditorAssetLibrary.find_asset_data(ANIM_SRC)
out["asset_data"] = ad.get_name() if ad else None

res = None
try:
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        [ad], src_mesh, tgt_mesh, rt, search="", replace="", prefix="", suffix="_W")
    out["retarget_call"] = "OK"
except Exception as e:
    out["retarget_call"] = "ERR %s" % e
if res is not None:
    out["retargeted_count"] = len(list(res))

cand = ANIM_SRC + "_W"
retgt = None
if unreal.EditorAssetLibrary.does_asset_exist(cand):
    retgt = unreal.EditorAssetLibrary.load_asset(cand)
out["retargeted_asset_path"] = cand if retgt else None
if retgt:
    out["retargeted_skeleton"] = str(retgt.get_editor_property("skeleton").get_path_name())
    unreal.EditorAssetLibrary.save_loaded_asset(retgt)

world = unreal.EditorLevelLibrary.get_editor_world()

def spawn(mesh_asset, x):
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkeletalMeshActor, unreal.Vector(x, 0, 0), unreal.Rotator(0, 0, 0))
    comp = a.skeletal_mesh_component
    comp.set_editor_property("skeletal_mesh", mesh_asset)
    return a, comp

a1, c1 = spawn(src_mesh, 0)
a2, c2 = spawn(tgt_mesh, 500)
anim_src = unreal.EditorAssetLibrary.load_asset(ANIM_SRC)
c1.play_animation(anim_src, True)
c1.set_play_rate(0.0)
c1.set_position(T)
if retgt:
    c2.play_animation(retgt, True)
    c2.set_play_rate(0.0)
    c2.set_position(T)

def qattr(q):
    if isinstance(q, (tuple, list)):
        return tuple(float(v) for v in q)
    return (float(q.x), float(q.y), float(q.z), float(q.w))

def qconj(q):
    x, y, z, w = q
    return (-x, -y, -z, w)

def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw*bx + ax*bw + ay*bz - az*by,
            aw*by - ax*bz + ay*bw + az*bx,
            aw*bz + ax*by - ay*bx + az*bw,
            aw*bw - ax*bx - ay*by - az*bz)

def parent_of(comp, name):
    try:
        p = str(comp.get_parent_bone(name))
        return p if p and p != "None" else None
    except Exception:
        return None

def wq(comp, name):
    return qattr(comp.get_socket_quaternion(name))

def local_q(comp, name):
    p = parent_of(comp, name)
    q = wq(comp, name)
    return q if p is None else qmul(qconj(wq(comp, p)), q)

def ang(q1, q2):
    d = min(1.0, max(-1.0, abs(sum(a*b for a, b in zip(q1, q2)))))
    return math.degrees(2*math.acos(d))

n2 = c2.get_num_bones()
wiz_bones = [str(c2.get_bone_name(i)) for i in range(n2)]
cmp_bones = [b for b in wiz_bones if not b.startswith("ik_")]
dev = {}
worst = None
mx = 0.0
for b in cmp_bones:
    try:
        a_ = ang(local_q(c1, b), local_q(c2, b))
    except Exception as e:
        dev[b] = "ERR %s" % e
        continue
    dev[b] = round(a_, 3)
    if a_ > mx:
        mx = a_
        worst = b
out["deviation"] = dev
out["max_dev_deg"] = round(mx, 3)
out["worst_bone"] = worst
out["cmp_bone_count"] = len(cmp_bones)

def pos(comp, name):
    v = comp.get_socket_location(name)
    return [round(v.x, 2), round(v.y, 2), round(v.z, 2)]

out["wizard_hand_r"] = pos(c2, "hand_r")
out["wizard_hand_l"] = pos(c2, "hand_l")
hr = pos(c2, "hand_r")
hl = pos(c2, "hand_l")
out["wizard_hands_dist"] = round(math.sqrt(sum((x-y)**2 for x, y in zip(hr, hl))), 2)

unreal.EditorLevelLibrary.destroy_actor(a1)
unreal.EditorLevelLibrary.destroy_actor(a2)

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print("MAX_DEV=%.3f deg worst=%s hands_dist=%.1f retargeted=%s" % (mx, worst, out["wizard_hands_dist"], out.get("retargeted_asset_path")))
