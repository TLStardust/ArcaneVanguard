# 只读勘查：导出 SKE_Wizard 全部骨骼名 + 关键 AnimBP/动画资产信息
# 目的：为 KI-013 的 A4 方案（AnimBP 手指弯曲层）提供精确骨骼名清单
import unreal, json

out = {}

skm = unreal.load_asset("/Game/Wizard/Mesh/SK_wizard")
ske = unreal.load_asset("/Game/Wizard/Mesh/SKE_Wizard")
out["skm_found"] = skm is not None
out["ske_found"] = ske is not None

# 骨骼名：迭代 get_bone_name(i) 直到失败/NAME_None
bones = []
if skm is not None:
    i = 0
    while i < 300:
        try:
            n = skm.get_bone_name(i)
        except Exception:
            break
        if n is None or str(n) == "None":
            break
        bones.append(str(n))
        i += 1
out["bone_count"] = len(bones)
out["bones"] = bones

# socket 列表（复核此前 "0 socket" 结论）
sockets = []
try:
    sk = skm.get_editor_property("skeleton")
    for attr in ("sockets",):
        try:
            val = sk.get_editor_property(attr)
            sockets = [str(s.get_editor_property("socket_name")) for s in val]
        except Exception as e:
            sockets = ["ERR %s: %s" % (attr, e)]
except Exception as e:
    sockets = ["ERR skeleton: %s" % e]
out["sockets"] = sockets

# AnimBP 信息
animbp = unreal.load_asset("/Game/Wizard/Animations/ThirdPerson/AnimBP_Wizard_ThirdPerson")
out["animbp_found"] = animbp is not None
if animbp is not None:
    try:
        out["animbp_target_skeleton"] = str(animbp.get_editor_property("target_skeleton"))
    except Exception as e:
        out["animbp_target_skeleton"] = "ERR %s" % e

# 关键动画时长
anim_paths = [
    "/Game/Wizard/Animations/Anim_Wizard_Cast_Spell_01",
    "/Game/Wizard/Animations/Anim_Wizard_Cast_Spell_02",
    "/Game/Wizard/Animations/Anim_Wizard_Magic_Attack",
    "/Game/Wizard/Animations/Anim_Wizard_Magic_Heal",
    "/Game/Wizard/Animations/Anim_Wizard_Death_Forward",
    "/Game/Wizard/Animations/ThirdPerson/Anim_Wizard_ThirdPersonIdle",
]
anims = {}
for p in anim_paths:
    a = unreal.load_asset(p)
    if a is None:
        anims[p] = None
    else:
        anims[p] = {
            "length": a.get_editor_property("sequence_length"),
            "num_frames": a.get_editor_property("number_of_sampled_frames"),
        }
out["animations"] = anims

outpath = "D:/UEProject/ArcaneVanguard/Saved/Logs/ki013_survey.json"
with open(outpath, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print("WROTE", outpath)
