import unreal
out = []
cls = unreal.load_class(None, '/Script/ArcaneVanguard.ArcaneVanguardCharacter')
out.append('class = %s' % cls)
if cls:
    cdo = unreal.get_default_object(cls)
    for p in ['death_montage', 'respawn_delay', 'b_is_dead']:
        try:
            out.append('  %-16s = %s' % (p, cdo.get_editor_property(p)))
        except Exception as e:
            out.append('  %-16s ERR %s' % (p, str(e)[:60]))
    for fn in ['handle_death', 'respawn', 'is_dead']:
        out.append('  has %-12s = %s' % (fn, hasattr(cdo, fn)))
open('D:/UEProject/ArcaneVanguard/Saved/Scripts/death_check.txt', 'w', encoding='utf-8').write('\n'.join(out))
