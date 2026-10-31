import io
import json
import os
import shutil
import time

TS = time.strftime('%Y%m%d-%H%M%S')
ROOT = r'D:/UEProject/ArcaneVanguard'
SRC = ROOT + '/Source/ArcaneVanguard/Enemy'
NAME = 'ArcaneVanguardBTService_UpdateTarget'

print('=== 1. 备份并删除源文件 ===')
for ext in ['.h', '.cpp']:
    p = os.path.join(SRC, NAME + ext)
    if os.path.exists(p):
        shutil.copy2(p, p + '.bak-' + TS)
        os.remove(p)
        print('  已删（备份 .bak-%s）: %s' % (TS, NAME + ext))
    else:
        print('  不存在: %s' % (NAME + ext))

print()
print('=== 2. 从 UHT manifest 移除 ===')
mp = ROOT + '/Intermediate/Build/Win64/ArcaneVanguardEditor/Development/ArcaneVanguardEditor.uhtmanifest'
with io.open(mp, encoding='utf-8') as f:
    d = json.load(f)
for m in d['Modules']:
    if m.get('Name') == 'ArcaneVanguard':
        before = len(m['PrivateHeaders'])
        m['PrivateHeaders'] = [h for h in m['PrivateHeaders'] if NAME not in h]
        print('  PrivateHeaders: %d -> %d' % (before, len(m['PrivateHeaders'])))
with io.open(mp, 'w', encoding='utf-8') as f:
    json.dump(d, f, indent=2)

print()
print('=== 3. 从 dll.rsp / lib.rsp 移除 ===')
DEV = ROOT + '/Intermediate/Build/Win64/x64/UnrealEditor/Development/ArcaneVanguard'
for rsp in ['UnrealEditor-ArcaneVanguard.dll.rsp', 'UnrealEditor-ArcaneVanguard.lib.rsp']:
    rp = os.path.join(DEV, rsp)
    if not os.path.exists(rp):
        print('  %s 不存在，跳过' % rsp)
        continue
    with io.open(rp, encoding='utf-8') as f:
        lines = f.read().splitlines()
    kept = [l for l in lines if NAME not in l]
    if len(kept) != len(lines):
        with io.open(rp, 'w', encoding='utf-8', newline='\n') as f:
            f.write('\n'.join(kept) + '\n')
        print('  %s: 移除 %d 行' % (rsp, len(lines) - len(kept)))
    else:
        print('  %s: 无需改动' % rsp)

print()
print('=== 4. 删除构建产物 ===')
for ext in ['.cpp.obj', '.gen.cpp.obj', '.cpp.obj.rsp', '.gen.cpp.obj.rsp']:
    p = os.path.join(DEV, NAME + ext)
    if os.path.exists(p):
        os.remove(p)
        print('  已删: %s' % (NAME + ext))

UHT = ROOT + '/Intermediate/Build/Win64/UnrealEditor/Inc/ArcaneVanguard/UHT'
for ext in ['.generated.h', '.gen.cpp']:
    p = os.path.join(UHT, NAME + ext)
    if os.path.exists(p):
        os.remove(p)
        print('  已删: %s' % (NAME + ext))

print()
print('=== 完成 ===')
