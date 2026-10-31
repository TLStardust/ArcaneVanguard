# 判别诊断：动画到底有没有在我们的巫师网格上生效？+ 双骨架骨顺序比对
import unreal, json, math

OUT = "D:/UEProject/ArcaneVanguard/Saved/Logs/megaspear_diag.json"
ANIM = "/Game/SpearAnimationsV2/Animations/Character/2Hand/Pose01/A_2H_Idle_01"
WIZ_MESH = "/Game/Wizard/Mesh/SK_wizard"
PACK_MESH = "/Game/SpearAnimationsV2/Mannequin/Character/Mesh/SK_Mannequin"
PROBE_BONES = ["hand_l", "hand_r", "thumb_01_l", "lowerarm_l", "foot_l", "pelvis"]

out = {}
world = unreal.EditorLevelLibrary.get_editor_world()
mesh_wiz = unreal.EditorAssetLibrary.load_asset(WIZ_MESH)
mesh_pack = unreal.EditorAssetLibrary.load_asset(PACK_MESH)
anim = unreal.EditorAssetLibrary.load_asset(ANIM)

out["pack_mesh_skeleton"] = mesh_pack.get_editor_property("skeleton").get_path_name()
out["wiz_mesh_skeleton"] = mesh_wiz.get_editor_property("skeleton").get_path_name()

def spawn(mesh_asset, x):
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SkeletalMeshActor, unreal.Vector(x, 0, 0), unreal.Rotator(0, 0, 0))
    comp = a.skeletal_mesh_component
    comp.set_editor_property("skeletal_mesh", mesh_asset)
    return a, comp

a2, c2 = spawn(mesh_wiz, 500)

def snap(comp):
    d = {}
    for b in PROBE_BONES:
        v = comp.get_socket_location(b)
        d[b] = [round(v.x, 2), round(v.y, 2), round(v.z, 2)]
    return d

# 1) 参考姿势（未播放任何动画）
ref = snap(c2)
out["wizard_ref_pose"] = ref

# 2) 播放动画，t=0 与 t=0.5
try:
    c2.play_animation(anim, True)
    out["play_call"] = "OK"
except Exception as e:
    out["play_call"] = "ERR %s" % e
try:
    out["anim_mode_after"] = str(c2.get_editor_property("animation_mode"))
except Exception as e:
    out["anim_mode_after"] = "ERR %s" % e
try:
    out["current_anim_prop"] = str(c2.get_editor_property("animation"))
except Exception as e:
    out["current_anim_prop"] = "ERR %s" % e

c2.set_play_rate(0.0)
c2.set_position(0.0)
t0 = snap(c2)
c2.set_position(0.5)
t05 = snap(c2)
out["wizard_t0"] = t0
out["wizard_t05"] = t05

def moved(a, b):
    return round(math.sqrt(sum((x-y)**2 for x, y in zip(a, b))), 2)
out["wizard_ref_vs_t0_cm"] = {b: moved(ref[b], t0[b]) for b in PROBE_BONES}
out["wizard_t0_vs_t05_cm"] = {b: moved(t0[b], t05[b]) for b in PROBE_BONES}

# 3) 骨顺序比对（前 12 根 + weapon/ik 附近）
def bone_order(comp):
    n = comp.get_num_bones()
    return [str(comp.get_bone_name(i)) for i in range(n)]

a1, c1 = spawn(mesh_pack, 0)
o1 = bone_order(c1)
o2 = bone_order(c2)
out["pack_order"] = o1
out["wizard_order"] = o2
out["order_identical_prefix"] = o1[:12] == o2[:12]
# 找第一处不一致
first_diff = None
for i in range(min(len(o1), len(o2))):
    if o1[i] != o2[i]:
        first_diff = (i, o1[i], o2[i])
        break
out["first_order_diff"] = first_diff

unreal.EditorLevelLibrary.destroy_actor(a1)
unreal.EditorLevelLibrary.destroy_actor(a2)

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print(json.dumps({k: out[k] for k in ["play_call", "anim_mode_after", "current_anim_prop",
      "wizard_ref_vs_t0_cm", "wizard_t0_vs_t05_cm", "order_identical_prefix", "first_order_diff",
      "pack_mesh_skeleton", "wiz_mesh_skeleton"]}, indent=1, ensure_ascii=False))
