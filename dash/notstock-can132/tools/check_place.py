#!/usr/bin/env python3
"""Placement check before the board is built: courtyards (from the KiCad
footprint files) must not overlap, must stay inside the outline, and keep
clear of the mounting holes. Prints what is wrong; exit 1 if anything."""
import math
import os
import re
import sys

from design import BOARD_R, HOLES, P

LIB = os.environ.get("KICAD_FOOTPRINT_DIR", "/usr/share/kicad/footprints")
EDGE = 0.3              # courtyard to the board edge
HOLE_KEEP = 2.6         # hole centre to courtyard (M2 head 3.8 / 2 + 0.7)


def courtyard(fpid):
    lib, name = fpid.split(":")
    s = open(os.path.join(LIB, lib + ".pretty", name + ".kicad_mod")).read()
    xs, ys = [], []
    for m in re.finditer(r'\(start ([-\d.]+) ([-\d.]+)\)\s*\(end ([-\d.]+) ([-\d.]+)\)'
                         r'[\s\S]*?\(layer "?([\w.]+)"?\)', s):
        if m.group(5) != "F.CrtYd":
            continue
        a = list(map(float, m.groups()[:4]))
        xs += [a[0], a[2]]
        ys += [a[1], a[3]]
    return min(xs), max(xs), min(ys), max(ys)


def corners(ref):
    p = P[ref]
    x0, x1, y0, y1 = courtyard(p["fp"])
    x, y, r = p["at"]
    a = math.radians(-r)          # KiCad: positive is counter-clockwise, y down
    out = []
    for cx, cy in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        out.append((x + cx * math.cos(a) - cy * math.sin(a),
                    y + cx * math.sin(a) + cy * math.cos(a)))
    return out


def box(ref):
    c = corners(ref)
    return (min(q[0] for q in c), max(q[0] for q in c),
            min(q[1] for q in c), max(q[1] for q in c))


bad = 0
refs = [r for r in P if not r.startswith("H")]
for i, a in enumerate(refs):
    A = box(a)
    for b in refs[i + 1:]:
        B = box(b)
        if A[0] < B[1] and B[0] < A[1] and A[2] < B[3] and B[2] < A[3]:
            print("overlap %s %s" % (a, b))
            bad += 1
    far = max(math.hypot(x, y) for x, y in corners(a))
    if far > BOARD_R - EDGE:
        print("%s past the edge: %.2f" % (a, far))
        bad += 1
    for hx, hy in HOLES:
        dx = max(A[0] - hx, 0, hx - A[1])
        dy = max(A[2] - hy, 0, hy - A[3])
        if math.hypot(dx, dy) < HOLE_KEEP:
            print("%s near hole (%g, %g): %.2f" % (a, hx, hy, math.hypot(dx, dy)))
            bad += 1
print("placement: %d problems" % bad)
sys.exit(1 if bad else 0)
