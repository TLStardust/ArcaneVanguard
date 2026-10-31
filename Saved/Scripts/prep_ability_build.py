import io
import json
import os

# ---- 1. manifest 加入新头文件 ----
mp = r'D:/UEProject/ArcaneVanguard/Intermediate/Build/Win64/ArcaneVanguardEditor/Development/ArcaneVanguardEditor.uhtmanifest'
new_header = 'D:/UEProject/ArcaneVanguard/Source/ArcaneVanguard/AbilitySystem/ArcaneVanguardGameplayAbility.h'

with io.open(mp, encoding='utf-8') as f:
    d = json.load(f)

for m in d['Modules']:
    if m.get('Name') == 'ArcaneVanguard':
        if new_header not in m['PrivateHeaders']:
            m['PrivateHeaders'].append(new_header)
        print('manifest PrivateHeaders 现在 %d 项' % len(m['PrivateHeaders']))

with io.open(mp, 'w', encoding='utf-8') as f:
    json.dump(d, f, indent=2)

# ---- 2. 生成编译参数文件 ----
base = r'D:/UEProject/ArcaneVanguard/Intermediate/Build/Win64/x64/UnrealEditor/Development/ArcaneVanguard'
OLD = 'ArcaneVanguardEnemyAIController'
NEW = 'ArcaneVanguardGameplayAbility'

with io.open(os.path.join(base, OLD + '.cpp.obj.rsp'), encoding='utf-8') as f:
    tmpl_cpp = f.read()
with io.open(os.path.join(base, OLD + '.gen.cpp.obj.rsp'), encoding='utf-8') as f:
    tmpl_gen = f.read()

# 先替换 .gen.cpp，再替换 .cpp
gen_txt = tmpl_gen.replace(OLD + '.gen.cpp', NEW + '.gen.cpp')
with io.open(os.path.join(base, NEW + '.gen.cpp.obj.rsp'), 'w', encoding='utf-8', newline='\n') as f:
    f.write(gen_txt)

cpp_txt = tmpl_cpp.replace(OLD + '.cpp', NEW + '.cpp')
with io.open(os.path.join(base, NEW + '.cpp.obj.rsp'), 'w', encoding='utf-8', newline='\n') as f:
    f.write(cpp_txt)

print('rsp 已生成:', NEW)

for suffix in ['.cpp', '.gen.cpp']:
    p = os.path.join(base, NEW + suffix + '.obj.rsp')
    with io.open(p, encoding='utf-8') as f:
        first = f.readline().strip()
    print('  %-40s -> %s' % (NEW + suffix, first.split('\\')[-1].strip('"')))
