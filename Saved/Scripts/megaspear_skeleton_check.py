# 解析 Mega Spear 包骨架 SK_Spear_Skeleton.uasset 名称表，与 SKE_Wizard 70 骨比对
import re, json

path = r"D:\UEProject\First5Point1\Content\SpearAnimationsV2\Mesh\SK_Spear_Skeleton.uasset"
data = open(path, "rb").read()
strings = re.findall(rb"[\x20-\x7e]{3,}", data)
strs = [s.decode("ascii") for s in strings]
uniq = sorted(set(strs))

WIZARD = [
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

result = {"present": [], "missing": []}
for t in WIZARD:
    (result["present"] if t in uniq else result["missing"]).append(t)

result["total_unique_strings"] = len(uniq)
bone_like = [s for s in uniq if re.fullmatch(r"[a-z][a-z0-9_]{2,20}", s)]
result["bone_like"] = bone_like
result["bone_like_count"] = len(bone_like)

# 包骨架有、但我们没有的骨（骨名表粗筛，排除明显的非骨名词）
ignore = {"unreal", "skeleton", "animsequence", "object", "package", "none", "class",
          "skeletalmesh", "material", "texture", "physicsasset", "blueprint"}
only_pack = [b for b in bone_like if b not in WIZARD and b not in ignore]
result["maybe_extra_bones"] = only_pack

with open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\megaspear_skeleton_check.json", "w", encoding="utf-8") as f:
    json.dump(result, f, indent=1, ensure_ascii=False)

print("present(%d/%d):" % (len(result["present"]), len(WIZARD)), result["present"])
print("missing:", result["missing"])
print("bone_like_count:", result["bone_like_count"])
print("maybe_extra:", only_pack)
