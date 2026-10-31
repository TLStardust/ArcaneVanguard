# 扫描动画 .uasset 的名称表，判断动画是否包含 weapon_r / weapon_l 骨骼轨道
# 原理：动画资产的名称表含全部骨骼轨道名；若名称都不在，则该骨骼绝不被动画驱动
import re, json, glob, os

BASE = r"D:\UEProject\ArcaneVanguard\Content\Wizard\Animations"
TARGETS = ["weapon_r", "weapon_l", "hand_r", "index_01_r", "ik_hand_gun", "lowerarm_r"]

result = {}
for path in sorted(glob.glob(os.path.join(BASE, "**", "*.uasset"), recursive=True)):
    data = open(path, "rb").read()
    strings = set(s.decode("ascii") for s in re.findall(rb"[\x20-\x7e]{3,}", data))
    rel = os.path.relpath(path, BASE)
    result[rel] = {t: (t in strings) for t in TARGETS}

with open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\ki013_anim_nametable.json", "w", encoding="utf-8") as f:
    json.dump(result, f, indent=1, ensure_ascii=False)

for k, v in result.items():
    print(k, "->", {t: ("Y" if ok else "-") for t, ok in v.items()})
