# 搭建 IK Retargeter：UE4 Mannequin(源) -> SKE_Wizard(目标)
# 产出: /Game/Wizard/Animations/_Retarget/ 下 IKR_Mannequin / IKR_Wizard / RT_Mannequin_to_Wizard
import unreal, json

OUT = "D:/UEProject/ArcaneVanguard/Saved/Logs/megaspear_ikr_setup.json"
BASE = "/Game/Wizard/Animations/_Retarget"
SRC_MESH = "/Game/SpearAnimationsV2/Mannequin/Character/Mesh/SK_Mannequin"
TGT_MESH = "/Game/Wizard/Mesh/SK_wizard"

out = {}
tools = unreal.AssetToolsHelpers.get_asset_tools()
src_mesh = unreal.EditorAssetLibrary.load_asset(SRC_MESH)
tgt_mesh = unreal.EditorAssetLibrary.load_asset(TGT_MESH)
out["src_mesh"] = SRC_MESH if src_mesh else None
out["tgt_mesh"] = TGT_MESH if tgt_mesh else None

def make_ikrig(name, mesh):
    path = BASE + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    asset = tools.create_asset(name, BASE, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(asset)
    r = {}
    r["set_mesh"] = bool(ctrl.set_skeletal_mesh(mesh))
    r["auto_retarget_def"] = bool(ctrl.apply_auto_generated_retarget_definition())
    chains = []
    for ch in ctrl.get_retarget_chains():
        chains.append({"name": str(ch.get_editor_property("chain_name")),
                       "start": str(ch.get_editor_property("start_bone")),
                       "end": str(ch.get_editor_property("end_bone"))})
    r["chains"] = chains
    r["retarget_root"] = str(ctrl.get_retarget_root())
    unreal.EditorAssetLibrary.save_asset(path)
    return asset, r

ikr_src, r_src = make_ikrig("IKR_Mannequin", src_mesh)
ikr_tgt, r_tgt = make_ikrig("IKR_Wizard", tgt_mesh)
out["ikr_src"] = r_src
out["ikr_tgt"] = r_tgt

# IKRetargeter
rt_path = BASE + "/RT_Mannequin_to_Wizard"
if unreal.EditorAssetLibrary.does_asset_exist(rt_path):
    unreal.EditorAssetLibrary.delete_asset(rt_path)
rt = tools.create_asset("RT_Mannequin_to_Wizard", BASE, unreal.IKRetargeter, unreal.IKRetargetFactory())
rc = unreal.IKRetargeterController.get_controller(rt)
rc.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, ikr_src)
rc.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, ikr_tgt)
out["has_source"] = bool(rt.has_source_ik_rig())
out["has_target"] = bool(rt.has_target_ik_rig())

map_log = {}
try:
    rc.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    map_log["exact"] = "called"
except Exception as e:
    map_log["exact"] = "ERR %s" % e
# 检查映射结果：目标链是否都拿到了源链
tgt_chains = [ch["name"] for ch in r_tgt["chains"]]
mapped = {}
for cn in tgt_chains:
    try:
        s = rc.get_source_chain(cn)
        mapped[cn] = str(s.get_editor_property("chain_name")) if s else "NONE"
    except Exception as e:
        mapped[cn] = "ERR %s" % e
map_log["mapping"] = mapped
out["mapping_log"] = map_log
unreal.EditorAssetLibrary.save_asset(rt_path)
out["saved_rt"] = rt_path

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, ensure_ascii=False)
print("SRC chains=%d root=%s auto=%s" % (len(r_src["chains"]), r_src["retarget_root"], r_src["auto_retarget_def"]))
print("TGT chains=%d root=%s auto=%s" % (len(r_tgt["chains"]), r_tgt["retarget_root"], r_tgt["auto_retarget_def"]))
print("mapped:", json.dumps(mapped, ensure_ascii=False))
