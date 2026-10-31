# 验证：兼容骨架下，包动画在我们的巫师网格上的播放正确性
# 方法：同一动画同时播在 包SK_Mannequin 和 我们SK_wizard 上，冻帧，逐骨对比局部旋转
import unreal, json, math

OUT = "D:/UEProject/ArcaneVanguard/Saved/Logs/megaspear_playtest_validate.json"
ANIM = "/Game/SpearAnimationsV2/Animations/Character/2Hand/Pose01/A_2H_Idle_01"
PACK_MESH = "/Game/SpearAnimationsV2/Mannequin/Character/Mesh/SK_Mannequin"
WIZ_MESH = "/Game/Wizard/Mesh/SK_wizard"
T = 0.5

out = {"anim": ANIM}

world = unreal.EditorLevelLibrary.get_editor_world()
mesh_pack = unreal.EditorAssetLibrary.load_asset(PACK_MESH)
mesh_wiz = unreal.EditorAssetLibrary.load_asset(WIZ_MESH)
anim = unreal.EditorAssetLibrary.load_asset(ANIM)
out["anim_loaded"] = anim.get_class().get_name() if anim else None

def spawn(mesh_asset, x):
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkeletalMeshActor, unreal.Vector(x, 0, 0), unreal.Rotator(0, 0, 0))
    comp = a.skeletal_mesh_component
    comp.set_editor_property("skeletal_mesh", mesh_asset)
    return a, comp

a1, c1 = spawn(mesh_pack, 0)       # 包的网格（原生）
a2, c2 = spawn(mesh_wiz, 500)      # 我们的巫师网格（兼容播放）
out["spawned"] = [a1.get_name(), a2.get_name()]

play_err = {}
try:
    c1.play_animation(anim, True)
    play_err["pack_mesh"] = "OK"
except Exception as e:
    play_err["pack_mesh"] = "ERR %s" % e
try:
    c2.play_animation(anim, True)
    play_err["wizard_mesh"] = "OK"
except Exception as e:
    play_err["wizard_mesh"] = "ERR %s" % e
out["play"] = play_err

for c, tag in [(c1, "pack"), (c2, "wizard")]:
    c.set_play_rate(0.0)
    c.set_position(T)

def bone_map(comp):
    n = comp.get_num_bones()
    return {str(comp.get_bone_name(i)): i for i in range(n)}

bm1 = bone_map(c1)
bm2 = bone_map(c2)
shared = sorted(set(bm1) & set(bm2))
out["pack_bones"] = len(bm1)
out["wizard_bones"] = len(bm2)
out["shared_count"] = len(shared)

def qattr(q):
    if isinstance(q, (tuple, list)):
        return tuple(float(v) for v in q)
    return (float(q.x), float(q.y), float(q.z), float(q.w))

def qconj(q):
    x, y, z, w = q
    return (-x, -y, -z, w)

def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
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
    q = wq(comp, name)
    p = parent_of(comp, name)
    if p is None:
        return q
    return qmul(qconj(wq(comp, p)), q)

def angle_between(q1, q2):
    d = abs(sum(a*b for a, b in zip(q1, q2)))
    d = min(1.0, max(-1.0, d))
    return math.degrees(2*math.acos(d))

results = {}
max_ang = 0.0
worst = None
for b in shared:
    p1 = parent_of(c1, b)
    p2 = parent_of(c2, b)
    if p1 != p2:
        results[b] = "PARENT_MISMATCH %s vs %s" % (p1, p2)
        continue
    try:
        ang = angle_between(local_q(c1, b), local_q(c2, b))
    except Exception as e:
        results[b] = "ERR %s" % e
        continue
    results[b] = round(ang, 4)
    if ang > max_ang:
        max_ang = ang
        worst = b
out["local_rot_deviation_deg"] = results
out["max_deviation_deg"] = round(max_ang, 4)
out["worst_bone"] = worst

def pos(comp, name):
    v = comp.get_socket_location(name)
    return (round(v.x, 2), round(v.y, 2), round(v.z, 2))

out["wizard_hand_r"] = pos(c2, "hand_r")
out["wizard_hand_l"] = pos(c2, "hand_l")
out["wizard_weapon_r"] = pos(c2, "weapon_r")
out["pack_hand_r"] = pos(c1, "hand_r")

hr = c2.get_socket_location("hand_r"); hl = c2.get_socket_location("hand_l")
out["wizard_hands_dist"] = round(math.sqrt((hr.x-hl.x)**2 + (hr.y-hl.y)**2 + (hr.z-hl.z)**2), 2)

unreal.EditorLevelLibrary.destroy_actor(a1)
unreal.EditorLevelLibrary.destroy_actor(a2)
out["cleaned"] = True

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print("MAX_DEV=%.4f deg worst=%s hands_dist=%.1f" % (max_ang, worst, out["wizard_hands_dist"]))
