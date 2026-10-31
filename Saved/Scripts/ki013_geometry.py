# 离线几何分析：weapon_r 相对 hand_r 的变换 + 法杖轴与手指的位置关系
import json, math

probe = json.load(open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\ki013_weapon_bone_probe.json", encoding="utf-8"))
bones = probe["bones"]

def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw*bx + ax*bw + ay*bz - az*by,
            aw*by - ax*bz + ay*bw + az*bx,
            aw*bz + ax*by - ay*bx + az*bw,
            aw*bw - ax*bx - ay*by - az*bz)

def qconj(q):
    return (-q[0], -q[1], -q[2], q[3])

def qrot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx = 2*(y*vz - z*vy); ty = 2*(z*vx - x*vz); tz = 2*(x*vy - y*vx)
    return (vx + w*tx + (y*tz - z*ty),
            vy + w*ty + (z*tx - x*tz),
            vz + w*tz + (x*ty - y*tx))

def qeuler(q):
    # UE FRotator: pitch(X) yaw(Y) roll(Z), 单位度
    x, y, z, w = q
    # pitch = asin(2(wx - yz)) 等标准转换
    sinp = 2*(w*x - y*z)
    sinp = max(-1.0, min(1.0, sinp))
    pitch = math.asin(sinp)
    if abs(sinp) < 0.99999:
        yaw = math.atan2(2*(w*y + x*z), 1 - 2*(x*x + y*y))
        roll = math.atan2(2*(w*z + x*y), 1 - 2*(x*x + z*z))
    else:
        yaw = math.atan2(-2*(y*z + w*x), 1 - 2*(y*y + z*z))
        roll = 0.0
    return (math.degrees(pitch), math.degrees(yaw), math.degrees(roll))

out = {}

hand_p = bones["hand_r"]["pos"]; hand_q = bones["hand_r"]["quat"]
wr_p = bones["weapon_r"]["pos"]; wr_q = bones["weapon_r"]["quat"]
wl_p = bones["weapon_l"]["pos"]

# 1) weapon_r 在 hand_r 局部坐标系下的位置
d_world = [wr_p[i] - hand_p[i] for i in range(3)]
d_local = qrot(qconj(hand_q), d_world)
out["weapon_r_offset_from_hand_r_world_cm"] = [round(v, 2) for v in d_world]
out["weapon_r_offset_from_hand_r_local_cm"] = [round(v, 2) for v in d_local]
out["weapon_r_distance_from_hand_r_cm"] = round(math.sqrt(sum(v*v for v in d_world)), 2)

# 2) weapon_r 相对 hand_r 的旋转
rel_q = qmul(qconj(hand_q), wr_q)
out["weapon_r_rel_rot_euler_deg"] = [round(v, 1) for v in qeuler(rel_q)]
out["weapon_r_world_rot_euler_deg"] = [round(v, 1) for v in qeuler(wr_q)]
out["hand_r_world_rot_euler_deg"] = [round(v, 1) for v in qeuler(hand_q)]

# 3) weapon_r 骨骼自身坐标轴的世界方向（若关掉竖直约束、挂到 weapon_r，法杖会跟随的姿态）
out["weapon_r_axis_x_world"] = [round(v, 3) for v in qrot(wr_q, (1, 0, 0))]
out["weapon_r_axis_y_world"] = [round(v, 3) for v in qrot(wr_q, (0, 1, 0))]
out["weapon_r_axis_z_world"] = [round(v, 3) for v in qrot(wr_q, (0, 0, 1))]

# 4) 法杖竖直轴（过 hand_r / weapon_r 的 XY）到各手指骨的距离
FINGERS = ["thumb_01_r", "thumb_02_r", "thumb_03_r",
           "index_01_r", "index_02_r", "index_03_r",
           "middle_01_r", "middle_02_r", "middle_03_r",
           "ring_01_r", "ring_02_r", "ring_03_r",
           "pinky_01_r", "pinky_02_r", "pinky_03_r"]
def axis_d(p, q):
    return math.hypot(p[0]-q[0], p[1]-q[1])

out["axis_dist_cm"] = {}
for f in FINGERS:
    p = bones[f]["pos"]
    out["axis_dist_cm"][f] = {
        "to_hand_r_axis": round(axis_d(p, hand_p), 2),
        "to_weapon_r_axis": round(axis_d(p, wr_p), 2),
    }

# 5) 四指基节质心（理想握持轴的大致位置）
bases = [bones[b]["pos"] for b in ["index_01_r", "middle_01_r", "ring_01_r", "pinky_01_r"]]
cx = sum(p[0] for p in bases)/4; cy = sum(p[1] for p in bases)/4
out["four_finger_base_centroid_xy"] = [round(cx, 2), round(cy, 2)]
out["centroid_to_hand_r_cm"] = round(math.hypot(cx-hand_p[0], cy-hand_p[1]), 2)
out["centroid_to_weapon_r_cm"] = round(math.hypot(cx-wr_p[0], cy-wr_p[1]), 2)

# 6) weapon_l 对称性参考
out["weapon_l_offset_from_weapon_r_cm"] = [round(wl_p[i]-wr_p[i], 2) for i in range(3)]

with open(r"D:\UEProject\ArcaneVanguard\Saved\Logs\ki013_weapon_geometry.json", "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)

print(json.dumps(out, indent=1, ensure_ascii=False))
