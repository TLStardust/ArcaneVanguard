import unreal, json

skm = unreal.load_asset("/Game/Wizard/Mesh/SK_wizard")
ske = unreal.load_asset("/Game/Wizard/Mesh/SKE_Wizard")

def members(o):
    return sorted([m for m in dir(o) if "bone" in m.lower()])

out = {
    "skm_bone_members": members(skm),
    "ske_bone_members": members(ske),
}

# 尝试常见 API
tries = {}
for label, fn in [
    ("skm.get_bone_names", lambda: skm.get_bone_names()),
    ("skm.get_bone_name(0)", lambda: skm.get_bone_name(0)),
    ("ske.get_bone_names", lambda: ske.get_bone_names()),
]:
    try:
        tries[label] = repr(fn())[:2000]
    except Exception as e:
        tries[label] = "ERR %s" % e
out["tries"] = tries

# EditorSkeletalMeshLibrary
try:
    esml = unreal.EditorSkeletalMeshLibrary
    out["esml_members"] = sorted([m for m in dir(esml) if "bone" in m.lower()])
except Exception as e:
    out["esml_members"] = "ERR %s" % e

with open("D:/UEProject/ArcaneVanguard/Saved/Logs/ki013_introspect.json", "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print("WROTE introspect")
