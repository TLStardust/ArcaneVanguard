import unreal, json

out = {}

# 路 1：AnimSequence 数据模型的骨骼轨道
anim = unreal.load_asset("/Game/Wizard/Animations/Anim_Wizard_Cast_Spell_01")
tracks = []
try:
    dm = anim.get_editor_property("data_model")
    out["dm_type"] = str(type(dm))
    out["dm_bone_members"] = sorted([m for m in dir(dm) if "bone" in m.lower() or "track" in m.lower()])
    try:
        bat = dm.get_editor_property("bone_animation_tracks")
        for t in bat:
            try:
                tracks.append(str(t.get_editor_property("name")))
            except Exception:
                tracks.append(repr(t)[:80])
    except Exception as e:
        out["bat_err"] = str(e)
except Exception as e:
    out["dm_err"] = str(e)
out["track_names"] = tracks

# 路 2：SkeletalMeshEditorSubsystem
try:
    smes = unreal.SkeletalMeshEditorSubsystem
    out["smes_bone_members"] = sorted([m for m in dir(smes) if "bone" in m.lower()])
except Exception as e:
    out["smes_bone_members"] = "ERR %s" % e

with open("D:/UEProject/ArcaneVanguard/Saved/Logs/ki013_introspect2.json", "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print("WROTE introspect2")
