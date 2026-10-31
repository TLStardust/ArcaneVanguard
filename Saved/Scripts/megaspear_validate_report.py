import json
d = json.load(open(r'D:/UEProject/ArcaneVanguard/Saved/Logs/megaspear_playtest_validate.json', encoding='utf-8'))
dev = d['local_rot_deviation_deg']
vals = {k: v for k, v in dev.items() if isinstance(v, (int, float))}
bad = {k: v for k, v in vals.items() if v > 1.0}
print('总骨数:', len(dev), ' 数值型:', len(vals), ' 偏差>1度的:', len(bad))
print()
print('偏差>1度的骨（降序）:')
for k, v in sorted(bad.items(), key=lambda x: -x[1]):
    print('  %-22s %.2f' % (k, v))
print()
print('非数值条目:', {k: v for k, v in dev.items() if not isinstance(v, (int, float))})
print()
print('play:', d['play'])
print('pack_hand_r:', d['pack_hand_r'], ' wizard_hand_r:', d['wizard_hand_r'])
print('wizard_weapon_r:', d['wizard_weapon_r'])
print('hands_dist:', d['wizard_hands_dist'])
print('anim_loaded:', d.get('anim_loaded'))
