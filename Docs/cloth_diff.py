import sys
import numpy as np
from PIL import Image

def diff(p1, p2, thresh=12):
    a = np.asarray(Image.open(p1).convert("RGB"), dtype=np.int16)
    b = np.asarray(Image.open(p2).convert("RGB"), dtype=np.int16)
    if a.shape != b.shape:
        return {"error": "shape mismatch %s vs %s" % (a.shape, b.shape)}
    d = np.abs(a - b).max(axis=2)
    mask = d > thresh
    n = int(mask.sum())
    out = {
        "size": list(a.shape[:2]),
        "changed_px": n,
        "changed_pct": round(100.0 * n / mask.size, 4),
        "max_diff": int(d.max()),
        "mean_diff_changed": round(float(d[mask].mean()), 2) if n else 0.0,
    }
    if n:
        ys, xs = np.nonzero(mask)
        out["bbox_xyxy"] = [int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())]
        out["bbox_w"] = int(xs.max() - xs.min() + 1)
        out["bbox_h"] = int(ys.max() - ys.min() + 1)
    return out

if __name__ == "__main__":
    r = diff(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 12)
    for k, v in r.items():
        print("%-20s %s" % (k, v))
