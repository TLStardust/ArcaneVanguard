# 运行时验证分析：Cast_Spell_01 动画中 weapon_r 是否随 hand_r 刚性运动
import json, math

rt = json.load(open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\ki013_weapon_rt_check.json", encoding="utf-8"))
ref = json.load(open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\ki013_weapon_bone_probe.json", encoding="utf-8"))
cast_old = json.load(open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\ki013_finger_analysis_cast.json", encoding="utf-8"))

def qconj(q): return (-q[0], -q[1], -q[2], q[3])
def qrot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx = 2*(y*vz - z*vy); ty = 2*(z*vx - x*vz); tz = 2*(x*vy - y*vx)
    return (vx + w*tx + (y*tz - z*ty), vy + w*ty + (z*tx - x*tz), vz + w*tz + (x*ty - y*tx))

ref_hand = ref["bones"]["hand_r"]
ref_wr = ref["bones"]["weapon_r"]
ref_off_world = [ref_wr["pos"][i] - ref_hand["pos"][i] for i in range(3)]
ref_off_local = qrot(qconj(ref_hand["quat"]), ref_off_world)

print("=== 参考姿势 weapon_r 相对 hand_r ===")
print("world offset (cm):", [round(v, 2) for v in ref_off_world])
print("hand_r local offset (cm):", [round(v, 2) for v in ref_off_local])

print()
print("=== Cast_Spell_01 动画中 ===")
results = {}
for frame_name, frame in rt["frames"].items():
    hand = frame["hand_r"]; wr = frame["weapon_r"]
    off_world = [wr["pos"][i] - hand["pos"][i] for i in range(3)]
    off_local = qrot(qconj(hand["quat"]), off_world)
    drift_world = math.sqrt(sum((off_world[i] - ref_off_world[i])**2 for i in range(3)))
    drift_local = math.sqrt(sum((off_local[i] - ref_off_local[i])**2 for i in range(3)))
    results[frame_name] = {"off_world": off_world, "off_local": off_local,
                           "drift_world_cm": drift_world, "drift_local_cm": drift_local}
    print(f"[{frame_name}] world offset: {[round(v,2) for v in off_world]}  local offset: {[round(v,2) for v in off_local]}")
    print(f"          漂移 vs 参考: world {drift_world:.2f} cm, hand_r局部 {drift_local:.2f} cm")

# 与上一轮 -game 实测的 cast@1.13 对比 hand_r（验证可复现性）
old = cast_old["bones"]["hand_r"]
new = rt["frames"]["t1.13"]["hand_r"]
d = math.sqrt(sum((new["pos"][i] - old["pos"][i])**2 for i in range(3)))
print()
print(f"=== 复现性核对：本轮 hand_r@1.13 vs 上轮 hand_r@1.13 位移 = {d:.2f} cm ===")

# 动画中 weapon_r 轴到各手指骨的距离（用上轮同帧手指数据 + 本轮 weapon_r 位置）
print()
print("=== Cast@1.13 法杖轴到手指骨距离（cm）===")
wr_new = rt["frames"]["t1.13"]["weapon_r"]["pos"]
hand_new = rt["frames"]["t1.13"]["hand_r"]["pos"]
print(f"{'bone':<14}{'to hand_r轴':>12}{'to weapon_r轴':>14}")
for b, bd in cast_old["bones"].items():
    p = bd["pos"]
    d_hand = math.hypot(p[0]-hand_new[0], p[1]-hand_new[1])
    d_wr = math.hypot(p[0]-wr_new[0], p[1]-wr_new[1])
    print(f"{b:<14}{d_hand:>12.2f}{d_wr:>14.2f}")

verdict = "RIGID" if max(r["drift_local_cm"] for r in results.values()) < 1.0 else "DRIVEN"
print()
print("判定: weapon_r 在动画中" + ("随 hand_r 刚性运动（未被独立驱动）" if verdict == "RIGID" else "被动画驱动！"))
