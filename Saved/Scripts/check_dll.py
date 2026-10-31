import unreal
print("=== DLL 加载自检 ===")
for p in ['/Script/ArcaneVanguard.ArcaneVanguardGameplayAbility',
          '/Script/ArcaneVanguard.ArcaneVanguardCharacter',
          '/Script/ArcaneVanguard.BTService_AVUpdateTarget']:
    print('  %-56s %s' % (p.split('.')[-1], 'OK' if unreal.load_class(None, p) else 'MISSING'))
print("=== done ===")
