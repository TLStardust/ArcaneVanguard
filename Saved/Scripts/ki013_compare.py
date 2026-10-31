import json

base = r"D:/UEProject/ArcaneVanguard/Saved/Logs/"
cast = json.load(open(base + "ki013_finger_analysis_cast.json", encoding="utf-8"))
idle = json.load(open(base + "ki013_finger_analysis.json", encoding="utf-8"))

print("STAFF cast pos:", cast["staff"])
print("hand_r cast pos:", cast["bones"]["hand_r"]["pos"])
print("hand_r idle pos:", idle["bones"]["hand_r"]["pos"])
print()
hdr = "%-14s | %-26s | %-26s" % ("bone", "curl_axis IDLE", "curl_axis CAST")
print(hdr)
print("-" * len(hdr))
for b in cast["analysis"]:
    ca = cast["analysis"][b].get("curl_axis_local")
    ia = idle["analysis"][b].get("curl_axis_local")
    print("%-14s | %-26s | %-26s" % (b, ia, ca))
print()
hdr2 = "%-14s | %-24s | %-24s" % ("bone", "child_dir IDLE", "child_dir CAST")
print(hdr2)
print("-" * len(hdr2))
for b in cast["analysis"]:
    cc = cast["analysis"][b].get("child_dir_local")
    ic = idle["analysis"][b].get("child_dir_local")
    print("%-14s | %-24s | %-24s" % (b, ic, cc))
