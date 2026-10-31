import json, math

base = r"D:/UEProject/ArcaneVanguard/Saved/Logs/"
idle = json.load(open(base + "ki013_finger_analysis.json", encoding="utf-8"))
cast = json.load(open(base + "ki013_finger_analysis_cast.json", encoding="utf-8"))


def q_conj(q):
    x, y, z, w = q
    return (-x, -y, -z, w)


def q_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw*bx + ax*bw + ay*bz - az*by,
            aw*by - ax*bz + ay*bw + az*bx,
            aw*bz + ax*by - ay*bx + az*bw,
            aw*bw - ax*bx - ay*by - az*bz)


def q_rot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx = 2*(y*vz - z*vy); ty = 2*(z*vx - x*vz); tz = 2*(x*vy - y*vx)
    return (vx + w*tx + (y*tz - z*ty),
            vy + w*ty + (z*tx - x*tz),
            vz + w*tz + (x*ty - y*tx))


def vsub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])


def vnorm(v):
    l = math.sqrt(sum(c*c for c in v))
    return tuple(c/l for c in v) if l > 1e-9 else (0.0, 0.0, 0.0)


def vcross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def vdot(a, b):
    return sum(a[i]*b[i] for i in range(3))


FINGERS = {
    "thumb": ["thumb_01_r", "thumb_02_r", "thumb_03_r"],
    "index": ["index_01_r", "index_02_r", "index_03_r"],
    "middle": ["middle_01_r", "middle_02_r", "middle_03_r"],
    "ring": ["ring_01_r", "ring_02_r", "ring_03_r"],
    "pinky": ["pinky_01_r", "pinky_02_r", "pinky_03_r"],
}


def analyze(pack, label):
    bones = pack["bones"]
    staff = pack["staff"]

    def pos(b):
        return tuple(bones[b]["pos"])

    def quat(b):
        return tuple(bones[b]["quat"])

    print("=" * 78)
    print("POSE:", label, " staff(x,y):", (round(staff[0], 1), round(staff[1], 1)))
    print("=" * 78)
    results = {}
    for fname, bs in FINGERS.items():
        for i, b in enumerate(bs):
            parent = "hand_r" if i == 0 else bs[i-1]
            child = bs[i+1] if i+1 < len(bs) else None
            qb = quat(b)
            qp = quat(parent)
            local = q_mul(q_conj(qp), qb)
            entry = {}
            if child:
                fd = vnorm(vsub(pos(child), pos(b)))           # 手指方向（世界）
                p = pos(b)
                # 修正：法杖轴是竖直的，水平方向用 XY 平面
                to_staff = vnorm((staff[0]-p[0], staff[1]-p[1], 0.0))
                proj = vdot(fd, to_staff)
                d = vnorm(vsub(to_staff, tuple(fd[c]*proj for c in range(3))))
                curl_world = vnorm(vcross(fd, d))
                curl_local = vnorm(q_rot(q_conj(qb), curl_world))
                entry["curl_axis_local"] = [round(c, 3) for c in curl_local]
                entry["child_dir_local"] = [round(c, 3) for c in vnorm(q_rot(q_conj(qb), fd))]
            results[b] = entry
    for b, e in results.items():
        print("%-13s curl_axis_local=%s" % (b, e.get("curl_axis_local")))
    return results


r_idle = analyze(idle, "IDLE")
print()
r_cast = analyze(cast, "CAST_SPELL_01 @1.13s")

print()
print("=" * 78)
print("一致性检查（idle vs cast，弯曲轴点积，1.0 = 完全一致）")
print("=" * 78)
for b in r_idle:
    if "curl_axis_local" in r_idle[b]:
        a = r_idle[b]["curl_axis_local"]
        c = r_cast[b]["curl_axis_local"]
        dot = sum(a[i]*c[i] for i in range(3))
        print("%-13s dot=%.4f  %s" % (b, dot, "OK" if dot > 0.95 else "CHECK"))

# 主成分判断：每个骨的弯曲轴最接近哪根局部主轴
print()
print("=" * 78)
print("弯曲轴最近主轴判断（用 cast 姿势数据）")
print("=" * 78)
AXES = {"+X": (1, 0, 0), "-X": (-1, 0, 0), "+Y": (0, 1, 0), "-Y": (0, -1, 0), "+Z": (0, 0, 1), "-Z": (0, 0, -1)}
for b, e in r_cast.items():
    if "curl_axis_local" not in e:
        continue
    v = e["curl_axis_local"]
    best = max(AXES.items(), key=lambda kv: sum(v[i]*kv[1][i] for i in range(3)))
    dot = sum(v[i]*best[1][i] for i in range(3))
    print("%-13s -> %-3s (align=%.3f)  full=%s" % (b, best[0], dot, v))
