import io
import json
import os

# ---- 1. 把两个新头文件加进 UHT manifest ----
mp = r'D:/UEProject/ArcaneVanguard/Intermediate/Build/Win64/ArcaneVanguardEditor/Development/ArcaneVanguardEditor.uhtmanifest'
src = 'D:/UEProject/ArcaneVanguard/Source/ArcaneVanguard/Enemy'
new_headers = [
    src + '/ArcaneVanguardBTService_UpdateTarget.h',
    src + '/ArcaneVanguardBTTask_Attack.h',
]

with io.open(mp, encoding='utf-8') as f:
    d = json.load(f)

for m in d['Modules']:
    if m.get('Name') == 'ArcaneVanguard':
        for h in new_headers:
            if h not in m['PrivateHeaders']:
                m['PrivateHeaders'].append(h)
        print('manifest PrivateHeaders 现在 %d 项' % len(m['PrivateHeaders']))

with io.open(mp, 'w', encoding='utf-8') as f:
    json.dump(d, f, indent=2)

# ---- 2. 为 4 个新翻译单元生成编译参数文件 ----
base = r'D:/UEProject/ArcaneVanguard/Intermediate/Build/Win64/x64/UnrealEditor/Development/ArcaneVanguard'
OLD = 'ArcaneVanguardEnemyAIController'

with io.open(os.path.join(base, OLD + '.cpp.obj.rsp'), encoding='utf-8') as f:
    tmpl_cpp = f.read()
with io.open(os.path.join(base, OLD + '.gen.cpp.obj.rsp'), encoding='utf-8') as f:
    tmpl_gen = f.read()

targets = [
    ('ArcaneVanguardBTService_UpdateTarget', tmpl_cpp, tmpl_gen),
    ('ArcaneVanguardBTTask_Attack', tmpl_cpp, tmpl_gen),
]

for name, t_cpp, t_gen in targets:
    # 先替换 .gen.cpp，再替换 .cpp —— 反过来会破坏 .gen.cpp 的名字
    gen_txt = t_gen.replace(OLD + '.gen.cpp', name + '.gen.cpp')
    with io.open(os.path.join(base, name + '.gen.cpp.obj.rsp'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(gen_txt)

    cpp_txt = t_cpp.replace(OLD + '.cpp', name + '.cpp')
    with io.open(os.path.join(base, name + '.cpp.obj.rsp'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(cpp_txt)

    print('rsp 已生成:', name)

# 校验第一行
for name, _, _ in targets:
    for suffix in ['.cpp', '.gen.cpp']:
        p = os.path.join(base, name + suffix + '.obj.rsp')
        with io.open(p, encoding='utf-8') as f:
            first = f.readline().strip()
        print('  %-52s -> %s' % (name + suffix, first.split('\\')[-1].strip('"')))
