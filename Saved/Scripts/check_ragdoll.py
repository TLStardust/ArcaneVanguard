import unreal
out = []
c = unreal.load_class(None, '/Script/ArcaneVanguard.ArcaneVanguardCharacter')
out.append('Character = %s' % c)
if c:
    cdo = unreal.get_default_object(c)
    for p in ['b_use_ragdoll_on_death', 'respawn_delay', 'death_montage']:
        try:
            out.append('  %-24s = %s' % (p, cdo.get_editor_property(p)))
        except Exception as e:
            out.append('  %-24s ERR %s' % (p, str(e)[:50]))
    for fn in ['drop_weapon_on_death', 'handle_death', 'respawn']:
        out.append('  has %-18s = %s' % (fn, hasattr(cdo, fn)))
w = unreal.load_class(None, '/Script/ArcaneVanguard.ArcaneVanguardWeapon')
out.append('Weapon = %s' % w)
if w:
    out.append('  has enable_physics_drop = %s' % hasattr(unreal.get_default_object(w), 'enable_physics_drop'))
open('D:/UEProject/ArcaneVanguard/Saved/Scripts/ragdoll_check.txt','w',encoding='utf-8').write('\n'.join(out))
