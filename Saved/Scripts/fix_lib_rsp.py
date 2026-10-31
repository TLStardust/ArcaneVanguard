import io

p = r'D:/UEProject/ArcaneVanguard/Intermediate/Build/Win64/x64/UnrealEditor/Development/ArcaneVanguard/UnrealEditor-ArcaneVanguard.lib.rsp'

with io.open(p, encoding='utf-8') as f:
    lines = f.read().splitlines()

# 先清掉之前写坏的行
lines = [l for l in lines if 'EnemyAIController' not in l]

# 用一条正常的 obj 行当模板，保证路径分隔符风格完全一致
tmpl = None
for l in lines:
    if l.strip().endswith('ArcaneVanguardEnemy.cpp.obj"'):
        tmpl = l
        break

if tmpl is None:
    raise SystemExit('找不到模板行 ArcaneVanguardEnemy.cpp.obj')

new1 = tmpl.replace('ArcaneVanguardEnemy.cpp.obj', 'ArcaneVanguardEnemyAIController.cpp.obj')
new2 = tmpl.replace('ArcaneVanguardEnemy.cpp.obj', 'ArcaneVanguardEnemyAIController.gen.cpp.obj')

idx = max(i for i, l in enumerate(lines) if l.strip().lower().endswith('.obj"'))
lines.insert(idx + 1, new1)
lines.insert(idx + 2, new2)

with io.open(p, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(lines) + '\n')

print('模板行:', tmpl)
print('新行1 :', new1)
print('新行2 :', new2)
print('总行数:', len(lines))
