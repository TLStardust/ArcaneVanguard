"""AV-047：建死亡蒙太奇并挂到玩家 BP。

用法（编辑器内 Python 或桥接 run_editor_python）：
    exec(open(r'D:/UEProject/ArcaneVanguard/Saved/Scripts/setup_death.py').read())

做三件事：
1. 从 Anim_Wizard_Death_Forward 建 AM_Wizard_Death（Montage）
2. 挂到 BP_ThirdPersonCharacter 的 DeathMontage
3. 打印验证结果
"""

import unreal

SEQ_PATH = '/Game/Wizard/Animations/Anim_Wizard_Death_Forward'
DEST_DIR = '/Game/ArcaneVanguard/Animation/Player/Death'
ASSET_NAME = 'AM_Wizard_Death'
BP_PATH = '/Game/ArcaneVanguard/BluePrint/Player/BP_ThirdPersonCharacter'

log = unreal.log


def ensure_montage():
    """已有就复用，没有才建。幂等，可重复执行。"""
    full_path = '{}/{}'.format(DEST_DIR, ASSET_NAME)
    existing = unreal.load_asset(full_path)
    if existing:
        log('[Death] 蒙太奇已存在，复用: {}'.format(full_path))
        return existing

    seq = unreal.load_asset(SEQ_PATH)
    if not seq:
        log('[Death] 找不到序列 {}'.format(SEQ_PATH))
        return None

    factory = unreal.AnimMontageFactory()
    factory.set_editor_property('source_animation', seq)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    montage = tools.create_asset(ASSET_NAME, DEST_DIR, unreal.AnimMontage, factory)
    if not montage:
        log('[Death] 创建蒙太奇失败')
        return None

    unreal.EditorAssetLibrary.save_asset(montage.get_path_name(), only_if_is_dirty=False)
    log('[Death] 已创建 {}（长度 {:.2f}s）'.format(
        montage.get_path_name(), montage.get_play_length()))
    return montage


def attach_to_bp(montage):
    bp = unreal.load_asset(BP_PATH)
    if not bp:
        log('[Death] 找不到 BP {}'.format(BP_PATH))
        return False

    cdo = unreal.get_default_object(bp.generated_class())
    try:
        cdo.set_editor_property('death_montage', montage)
        cdo.set_editor_property('respawn_delay', 3.0)
        saved = unreal.EditorAssetLibrary.save_asset(BP_PATH, only_if_is_dirty=False)
        log('[Death] 已挂到 BP，保存={}'.format(saved))
        return True
    except Exception as exc:
        log('[Death] 挂载失败: {}'.format(str(exc)[:160]))
        return False


def verify():
    bp = unreal.load_asset(BP_PATH)
    cdo = unreal.get_default_object(bp.generated_class())
    for prop in ['death_montage', 'respawn_delay', 'b_is_dead']:
        try:
            val = cdo.get_editor_property(prop)
            name = val.get_name() if hasattr(val, 'get_name') else val
            log('[Death]   {} = {}'.format(prop, name))
        except Exception:
            log('[Death]   {} = (读不到，protected)'.format(prop))


montage = ensure_montage()
if montage:
    attach_to_bp(montage)
    verify()
