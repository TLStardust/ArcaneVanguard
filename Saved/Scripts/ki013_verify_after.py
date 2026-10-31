# -*- coding: utf-8 -*-
"""KI-013 验收：Cast_Spell_01 冻帧测量指尖到法杖轴距离，与基线对比。

通过 MCP 桥接分两段执行（每段单独一次 run_editor_python 调用）：
  PART_A: 触发动画 + 立即冻帧到 1.13s（pose 在下一 tick 求值）
  PART_B: 测量 + 与基线对比 + 截图 + 写判定 JSON

用法：
  1. 先跑基线：把下面 MODE 改成 "baseline"（挂点变更后必须重测，否则误判）
  2. 再验收：MODE 改回 "verify"
判定：每根手指指尖到法杖竖直轴的水平距离应显著小于基线（指尖弯向法杖）。
若当前挂点与基线记录的不一致 → verdict = REBASELINE_NEEDED。
"""
import unreal, json, math

MODE = "verify"  # "baseline" | "verify"

MAP = "/Game/ThirdPerson/Maps/ThirdPersonMap"
BASELINE = "D:/UEProject/ArcaneVanguard/Saved/Logs/ki013_finger_analysis_cast.json"
OUT = "D:/UEProject/ArcaneVanguard/Saved/Logs/ki013_verify_after.json"

TIPS = ["thumb_03_r", "index_03_r", "middle_03_r", "ring_03_r", "pinky_03_r"]
MIDS = ["thumb_02_r", "index_02_r", "middle_02_r", "ring_02_r", "pinky_02_r"]


def get_pawn():
    world = unreal.load_asset(MAP)
    for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Character):
        if c.get_class().get_name() == "BP_ThirdPersonCharacter_C":
            return c
    raise RuntimeError("pawn not found")


def axis_dist(p, staff):
    return math.hypot(p[0] - staff[0], p[1] - staff[1])


# ---------------------------------------------------------------- PART_A
pawn = get_pawn()
mesh = pawn.get_editor_property("mesh")
pawn.debug_play_animation(0)
mesh.set_play_rate(0)
mesh.set_position(1.13)
print("[KI013-verify] triggered + frozen at 1.13s (MODE=%s)" % MODE)

# ---------------------------------------------------------------- PART_B
pawn = get_pawn()
mesh = pawn.get_editor_property("mesh")
weapon = pawn.get_editor_property("weapon_mesh")

# 当前挂点（用于检测基线是否过期；weapon_r 切换后法杖轴会移动，旧基线失效）
try:
    socket_now = str(pawn.get_editor_property("weapon_attach_socket"))
except Exception:
    socket_now = "?"


def pos(b):
    v = mesh.get_socket_location(b)
    return (v.x, v.y, v.z)


staff = weapon.get_world_location()
staff = (staff.x, staff.y, staff.z)

result = {"mode": MODE, "attach_socket": socket_now, "staff": [round(c, 2) for c in staff], "fingers": {}}

if MODE == "baseline":
    for f in TIPS + MIDS:
        p = pos(f)
        result["fingers"][f] = {"dist": round(axis_dist(p, staff), 2)}
    result["verdict"] = "BASELINE_SAVED"
else:
    base = json.load(open(BASELINE, encoding="utf-8"))
    socket_base = base.get("attach_socket")
    all_pass = True
    for f in TIPS + MIDS:
        p = pos(f)
        d = axis_dist(p, staff)
        bd = axis_dist(tuple(base["bones"][f]["pos"]), tuple(base["staff"]))
        delta = bd - d
        ok = delta > 0.5  # 指尖向法杖收拢超过 0.5cm 视为通过
        if not ok:
            all_pass = False
        result["fingers"][f] = {
            "dist_after": round(d, 2),
            "dist_before": round(bd, 2),
            "moved_toward_cm": round(delta, 2),
            "pass": ok,
        }
    if socket_base is not None and socket_base != socket_now:
        # 挂点变了（如 hand_r → weapon_r）：法杖轴自身移动会造成假 PASS，必须先重测基线
        result["verdict"] = "REBASELINE_NEEDED"
        result["note"] = "baseline attach_socket=%s, now=%s" % (socket_base, socket_now)
    else:
        result["verdict"] = "PASS" if all_pass else "FAIL"

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=1, ensure_ascii=False)

unreal.SystemLibrary.execute_console_command(unreal.load_asset(MAP), "HighResShot 1600x900")
print("[KI013-verify] verdict =", result["verdict"])
print(json.dumps(result["fingers"], indent=1))
