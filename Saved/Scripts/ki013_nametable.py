# 解析 SKE_Wizard.uasset 二进制，确认骨骼名（尤其是五指骨骼）
# 方法：.uasset 的 name table 存有全部 FName 字符串，直接搜索目标字符串的存在性
import re, json

path = r"D:\UEProject\ArcaneVanguard\Content\Wizard\Mesh\SKE_Wizard.uasset"
data = open(path, "rb").read()
print("file size:", len(data))

# 提取所有可打印 ASCII 串（长度>=3）
strings = re.findall(rb"[\x20-\x7e]{3,}", data)
strs = [s.decode("ascii") for s in strings]
uniq = sorted(set(strs))

targets = [
    "root", "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head",
    "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r",
    "lowerarm_l", "lowerarm_r", "hand_l", "hand_r",
    "index_01_l", "index_02_l", "index_03_l",
    "middle_01_l", "middle_02_l", "middle_03_l",
    "ring_01_l", "ring_02_l", "ring_03_l",
    "pinky_01_l", "pinky_02_l", "pinky_03_l",
    "thumb_01_l", "thumb_02_l", "thumb_03_l",
    "index_01_r", "index_02_r", "index_03_r",
    "middle_01_r", "middle_02_r", "middle_03_r",
    "ring_01_r", "ring_02_r", "ring_03_r",
    "pinky_01_r", "pinky_02_r", "pinky_03_r",
    "thumb_01_r", "thumb_02_r", "thumb_03_r",
    "thigh_l", "thigh_r", "calf_l", "calf_r", "foot_l", "foot_r", "ball_l", "ball_r",
    "ik_foot_root", "ik_foot_l", "ik_foot_r", "ik_hand_root", "ik_hand_gun", "ik_hand_l", "ik_hand_r",
]

result = {"present": [], "missing": []}
for t in targets:
    if t in uniq:
        result["present"].append(t)
    else:
        result["missing"].append(t)

result["total_unique_strings"] = len(uniq)

# 顺带把所有疑似骨骼名的串列出来（小写 + 下划线/数字，长度<=16）
bone_like = [s for s in uniq if re.fullmatch(r"[a-z][a-z0-9_]{2,15}", s)]
result["bone_like_count"] = len(bone_like)
result["bone_like"] = bone_like

with open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\ki013_bonenames.json", "w", encoding="utf-8") as f:
    json.dump(result, f, indent=1, ensure_ascii=False)

print("present:", result["present"])
print("missing:", result["missing"])
print("bone_like_count:", result["bone_like_count"])
