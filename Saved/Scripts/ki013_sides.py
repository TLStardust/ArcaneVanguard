import json, math

base = r"D:/UEProject/ArcaneVanguard/Saved/Logs/"
cast = json.load(open(base + "ki013_finger_analysis_cast.json", encoding="utf-8"))
idle = json.load(open(base + "ki013_finger_analysis.json", encoding="utf-8"))


def q_conj(q):
    x, y, z, w = q
    return (-x, -y, -z, w)


def q_rot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx = 2*(y*vz - z*vy); ty = 2*(z*vx - x*vz); tz = 2*(x*vy - y*vx)
    return (vx + w*tx + (y*tz - z*ty),
            vy + w*ty + (z*tx - x*tz),
            vz + w*tz + (x*ty - y*tx))


for label, pack in [("CAST", cast), ("IDLE", idle)]:
    bones = pack["bones"]
    staff = pack["staff"]
    hand = bones["hand_r"]["pos"]
    print("=" * 70)
    print(label, "staff(x,y)=", (round(staff[0], 1), round(staff[1], 1)),
          " hand(x,y)=", (round(hand[0], 1), round(hand[1], 1)))
    # 每根手指第1节相对手的位置（水平面）
    for f in ["thumb_01_r", "index_01_r", "middle_01_r", "ring_01_r", "pinky_01_r"]:
        p = bones[f]["pos"]
        print("  %-12s rel_hand_xy = (%+.2f, %+.2f)" % (f, p[0]-hand[0], p[1]-hand[1]))
    # 手指方向（世界）和到杖轴方向
    print("  --- finger direction & bend dir (world, XY plane) ---")
    pairs = [("index_01_r", "index_02_r"), ("middle_01_r", "middle_02_r"),
             ("ring_01_r", "ring_02_r"), ("pinky_01_r", "pinky_02_r"),
             ("thumb_01_r", "thumb_02_r")]
    for a, b in pairs:
        pa, pb = bones[a]["pos"], bones[b]["pos"]
        fd = (pb[0]-pa[0], pb[1]-pa[1], pb[2]-pa[2])
        fl = math.sqrt(sum(c*c for c in fd))
        fd = tuple(c/fl for c in fd)
        dx, dy = staff[0]-pa[0], staff[1]-pa[1]
        dl = math.hypot(dx, dy)
        print("  %-12s finger_dir=(%+.2f,%+.2f,%+.2f)  to_staff_xy=(%+.2f,%+.2f)" %
              (a, fd[0], fd[1], fd[2], dx/dl, dy/dl))
