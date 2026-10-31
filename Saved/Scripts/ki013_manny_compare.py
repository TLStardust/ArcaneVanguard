# 对比 UE5 Manny 骨架骨骼名 vs SKE_Wizard 骨骼树
import re, json

manny_path = r"C:\Program Files\Epic Games\UE_5.4\Engine\Plugins\Experimental\MoverExamples\Content\Characters\Mannequins\Meshes\SKM_Manny.uasset"
wizard_tree = json.load(open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\ki013_weapon_bone_probe2.json", encoding="utf-8"))["bones"]
wizard_bones = set(wizard_tree.values())

data = open(manny_path, "rb").read()
strings = set(s.decode("ascii") for s in re.findall(rb"[\x20-\x7e]{3,}", data))

# 人形骨骼名词汇表（UE4/UE5 mannequin 惯例全量），用来从噪声中筛出骨骼名
VOCAB = set("""
root pelvis spine_01 spine_02 spine_03 spine_04 spine_05
clavicle_l clavicle_r upperarm_l upperarm_r lowerarm_l lowerarm_r hand_l hand_r
index_01_l index_02_l index_03_l index_01_r index_02_r index_03_r
middle_01_l middle_02_l middle_03_l middle_01_r middle_02_r middle_03_r
ring_01_l ring_02_l ring_03_l ring_01_r ring_02_r ring_03_r
pinky_01_l pinky_02_l pinky_03_l pinky_01_r pinky_02_r pinky_03_r
thumb_01_l thumb_02_l thumb_03_l thumb_01_r thumb_02_r thumb_03_r
neck_01 neck_02 head
thigh_l thigh_r calf_l calf_r foot_l foot_r ball_l ball_r
calf_twist_01_l calf_twist_01_r thigh_twist_01_l thigh_twist_01_r
lowerarm_twist_01_l lowerarm_twist_01_r upperarm_twist_01_l upperarm_twist_01_r
ik_foot_root ik_foot_l ik_foot_r ik_hand_root ik_hand_gun ik_hand_l ik_hand_r
weapon_l weapon_r
""".split())

manny_bones = strings & VOCAB

only_manny = sorted(manny_bones - wizard_bones)
only_wizard = sorted(wizard_bones - manny_bones)
common = sorted(manny_bones & wizard_bones)

out = {
    "wizard_bone_count": len(wizard_bones),
    "manny_matched_count": len(manny_bones),
    "common": common,
    "only_in_manny": only_manny,
    "only_in_wizard": only_wizard,
}
with open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\ki013_manny_vs_wizard.json", "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)

print("SKE_Wizard 骨骼数:", len(wizard_bones))
print("Manny 命中词汇表骨骼数:", len(manny_bones))
print()
print("=== 仅 Manny 有（SKE_Wizard 没有）===")
print(only_manny)
print()
print("=== 仅 SKE_Wizard 有（Manny 没有）===")
print(only_wizard)
print()
print("=== 共有骨骼数:", len(common), "===")
