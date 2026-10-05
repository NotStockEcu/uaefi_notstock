#!/usr/bin/env python3
"""Build notstock-can185.kicad_pcb from design.py with KiCad's pcbnew
module: outline, footprints with their nets (linked to the schematic's
symbols), silkscreen; then routes it with Freerouting, pours GND on both
sides, runs DRC and writes the fabrication files.

    python3 tools/gen_pcb.py [--freerouting path/to/freerouting.jar]

Needs KiCad 7 (its python module and footprint libraries) and, for the
routing, Java 21 and Freerouting 2.x, headless
(github.com/freerouting/freerouting).
Without the jar it stops after placement (notstock-can185.kicad_pcb then
has ratsnest only).
"""
import argparse
import json
import math
import os
import subprocess
import sys
import uuid

import pcbnew

from design import BOARD_R, HOLES, P, POWER

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
NAME = "notstock-can185"
FPLIB = os.environ.get("KICAD_FOOTPRINT_DIR", "/usr/share/kicad/footprints")
CX, CY = 150.0, 100.0          # where the board's centre sits on the page

mm = pcbnew.FromMM


def V(x, y):
    return pcbnew.VECTOR2I(mm(CX + x), mm(CY + y))


def sym_uuid(ref):
    return str(uuid.uuid5(uuid.NAMESPACE_URL, "ns-can185/sym/" + ref))


# ---------------------------------------------------------- project file
def write_project():
    """Net classes live in the project file in KiCad 7."""
    pro = {
        "board": {"design_settings": {
            "defaults": {"board_outline_line_width": 0.1,
                         "copper_line_width": 0.2, "silk_line_width": 0.15,
                         "silk_text_size_h": 0.8, "silk_text_size_v": 0.8,
                         "silk_text_thickness": 0.15},
            "rules": {"min_clearance": 0.15, "min_track_width": 0.15,
                      "min_via_diameter": 0.5, "min_through_hole_diameter": 0.3,
                      "min_copper_edge_clearance": 0.3,
                      "min_hole_to_hole": 0.25, "min_silk_clearance": 0.0},
            # the footprints come from KiCad's standard libraries; outside
            # a KiCad install (the generator) they are not in a table
            "rule_severities": {"lib_footprint_issues": "ignore",
                                "lib_footprint_mismatch": "ignore",
                                "silk_overlap": "ignore"},
        }},
        "meta": {"filename": NAME + ".kicad_pro", "version": 1},
        "erc": {},
        "net_settings": {
            "classes": [
                {"name": "Default", "clearance": 0.2, "track_width": 0.25,
                 "via_diameter": 0.6, "via_drill": 0.3,
                 "diff_pair_gap": 0.25, "diff_pair_width": 0.2,
                 "uvia_diameter": 0.3, "uvia_drill": 0.1,
                 "wire_width": 6, "bus_width": 12, "line_style": 0,
                 "pcb_color": "rgba(0, 0, 0, 0.000)",
                 "schematic_color": "rgba(0, 0, 0, 0.000)"},
                {"name": "Power", "clearance": 0.25, "track_width": 0.6,
                 "via_diameter": 0.8, "via_drill": 0.4,
                 "diff_pair_gap": 0.25, "diff_pair_width": 0.2,
                 "uvia_diameter": 0.3, "uvia_drill": 0.1,
                 "wire_width": 6, "bus_width": 12, "line_style": 0,
                 "pcb_color": "rgba(0, 0, 0, 0.000)",
                 "schematic_color": "rgba(0, 0, 0, 0.000)"},
            ],
            "meta": {"version": 3},
            "netclass_patterns": [{"netclass": "Power", "pattern": n}
                                  for n in sorted(POWER)],
        },
        "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
    }
    with open(os.path.join(ROOT, NAME + ".kicad_pro"), "w") as f:
        json.dump(pro, f, indent=2)


# --------------------------------------------------------------- helpers
def load_fp(fpid):
    lib, name = fpid.split(":")
    fp = pcbnew.FootprintLoad(os.path.join(FPLIB, lib + ".pretty"), name)
    if fp is None:
        sys.exit("footprint not found: " + fpid)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp


def pad_xy(fp, num):
    for p in fp.Pads():
        if p.GetNumber() == num:
            q = p.GetPosition()
            return pcbnew.ToMM(q.x) - CX, pcbnew.ToMM(q.y) - CY
    return None


def silk(board, txt, x, y, size=0.8, layer=pcbnew.F_SilkS, rot=0, bold=False):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(txt)
    t.SetPosition(V(x, y))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(size * (0.2 if bold else 0.15)))
    t.SetTextAngleDegrees(rot)
    if layer == pcbnew.B_SilkS:
        t.SetMirrored(True)
    board.Add(t)
    return t


def place_refs(board):
    """Each visible reference beside its part where it hits no pad, no
    silkscreen line and no other reference: above, below, left, right."""
    def box(bb, grow=0.0):
        return (pcbnew.ToMM(bb.GetLeft()) - grow, pcbnew.ToMM(bb.GetTop()) - grow,
                pcbnew.ToMM(bb.GetRight()) + grow, pcbnew.ToMM(bb.GetBottom()) + grow)

    def hit(a, b):
        return not (a[2] <= b[0] or b[2] <= a[0] or a[3] <= b[1] or b[3] <= a[1])

    taken = [box(d.GetBoundingBox(), 0.1) for d in board.GetDrawings()
             if d.GetClass() == "PCB_TEXT"]
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            taken.append(box(pad.GetBoundingBox(), 0.15))
        for g in fp.GraphicalItems():
            if g.GetLayer() == pcbnew.F_SilkS and g.GetClass() != "PCB_TEXT" \
                    and g.GetClass() != "FP_TEXT":
                taken.append(box(g.GetBoundingBox(), 0.1))
    for fp in sorted(board.GetFootprints(), key=lambda f: f.GetReference()):
        ref = fp.Reference()
        if not ref.IsVisible() or fp.IsFlipped():
            continue
        body = box(fp.GetBoundingBox(False, False))
        cx, cy = (body[0] + body[2]) / 2, (body[1] + body[3]) / 2
        h = 1.1                      # 0.8 text and its stroke, with room
        w = 0.75 * len(fp.GetReference()) + 0.4
        cands = [(cx, body[1] - h / 2 - 0.15), (cx, body[3] + h / 2 + 0.15),
                 (body[0] - w / 2 - 0.2, cy), (body[2] + w / 2 + 0.2, cy)]
        for x, y in cands:
            r = (x - w / 2, y - h / 2, x + w / 2, y + h / 2)
            if math.hypot(x - CX, y - CY) > BOARD_R - 1.0:
                continue
            if not any(hit(r, t) for t in taken):
                ref.SetTextAngleDegrees(0)
                ref.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
                taken.append(r)
                break
        else:
            ref.SetVisible(False)           # on the fab drawing only
            print("no room for", fp.GetReference(), "on the silkscreen")


def edge_keepout(board, n=48):
    """No tracks or vias in the last 0.7 mm to the edge. Freerouting does
    not know KiCad's edge clearance, but it keeps out of rule areas; a
    ring of small ones, as the DSN export drops a rule area's hole."""
    r0, r1 = BOARD_R - 0.7, BOARD_R + 1.0
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1.02) / n
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetDoNotAllowTracks(True)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowCopperPour(False)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        ls = pcbnew.LSET()
        ls.AddLayer(pcbnew.F_Cu)
        ls.AddLayer(pcbnew.B_Cu)
        z.SetLayerSet(ls)
        ol = z.Outline()
        ol.NewOutline()
        for r, a in ((r0, a0), (r1, a0), (r1, a1), (r0, a1)):
            ol.Append(V(r * math.cos(a), r * math.sin(a)))
        z.SetZoneName("edge%d" % i)
        board.Add(z)


def circle_pts(r, n=96):
    return [(r * math.cos(2 * math.pi * i / n), r * math.sin(2 * math.pi * i / n))
            for i in range(n)]


# ----------------------------------------------------------------- build
def build():
    write_project()
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)
    nets = {}
    for p in P.values():
        for n in p["pins"].values():
            if n not in nets:
                ni = pcbnew.NETINFO_ITEM(board, n)
                board.Add(ni)
                nets[n] = ni

    # outline
    c = pcbnew.PCB_SHAPE(board)
    c.SetShape(pcbnew.SHAPE_T_CIRCLE)
    c.SetCenter(V(0, 0))
    c.SetEnd(V(BOARD_R, 0))
    c.SetLayer(pcbnew.Edge_Cuts)
    c.SetWidth(mm(0.1))
    board.Add(c)

    for ref, p in P.items():
        fp = load_fp(p["fp"])
        fp.SetReference(ref)
        fp.SetValue(p["val"])
        fp.SetPath(pcbnew.KIID_PATH("/" + sym_uuid(ref)))
        board.Add(fp)
        x, y, rot = p["at"]
        fp.SetPosition(V(x, y))
        fp.SetOrientationDegrees(rot)
        if p["side"] == "B":
            fp.Flip(fp.GetPosition(), False)
        for pad in fp.Pads():
            net = p["pins"].get(pad.GetNumber())
            if net:
                pad.SetNet(nets[net])
        # small references, out of the way of the pads
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8)))
        fp.Reference().SetTextThickness(mm(0.13))
        # no room on the silkscreen: these keep their reference on the
        # fab layer only (the assembly drawing has them all)
        if ref.startswith("H") or ref == "JP1":
            fp.Reference().SetVisible(False)
        fp.Value().SetVisible(False)

    # silkscreen: what plugs where
    silk(board, "NOT STOCK", 3.5, 6.5, 1.4, bold=True)
    silk(board, "CAN 1.85", 3.5, 8.6, 1.0)
    jx, jy = P["J1"]["at"][:2]
    for i, t in enumerate(("12V", "GND", "CH", "CL")):
        silk(board, t, jx - 3 + 2 * i, jy + 5.8, 0.8, rot=90)
    silk(board, "TERM", P["JP1"]["at"][0] + 2.6, P["JP1"]["at"][1], 0.8, rot=90)
    x, y = P["J2"]["at"][:2]
    silk(board, "UART", x, y + 4.0, 0.8)
    x, y = P["J3"]["at"][:2]
    silk(board, "5V > LCD USB-C", x, y - 6.6, 0.8)
    silk(board, "rev 2", -3.0, -6.6, 0.8)
    place_refs(board)
    edge_keepout(board)

    path = os.path.join(ROOT, NAME + ".kicad_pcb")
    board.Save(path)
    return path


def netclasses(board):
    """Default and Power classes, in the board in memory: the DSN export
    hands their widths to Freerouting. (The project file gets them too, in
    write_project, for whoever opens the board in KiCad.)"""
    ns = board.GetDesignSettings().m_NetSettings
    d = ns.m_DefaultNetClass
    d.SetTrackWidth(mm(0.25))
    d.SetClearance(mm(0.2))
    d.SetViaDiameter(mm(0.6))
    d.SetViaDrill(mm(0.3))
    pw = pcbnew.NETCLASS("Power")
    pw.SetTrackWidth(mm(0.6))
    pw.SetClearance(mm(0.25))
    pw.SetViaDiameter(mm(0.8))
    pw.SetViaDrill(mm(0.4))
    ns.m_NetClasses["Power"] = pw
    for name in POWER:
        net = board.FindNet(name)
        if net:
            net.SetNetClass(pw)


def power_class_in_dsn(dsn):
    """KiCad 7's python cannot assign nets to a class by pattern, so the
    export lists every net under the default class: move the power nets to
    the Power class there, which carries its 0.6 mm width."""
    import re
    t = open(dsn).read()
    m = re.search(r'\(class kicad_default ""([^(]*)', t)
    names = m.group(1).split()
    keep = [n for n in names if n not in POWER]
    moved = [n for n in names if n in POWER]
    t = t[:m.start(1)] + " " + " ".join(keep) + "\n      " + t[m.end(1):]
    t = t.replace("(class Power\n", "(class Power " + " ".join(moved) + "\n", 1)
    open(dsn, "w").write(t)


def route(path, jar):
    board = pcbnew.LoadBoard(path)
    netclasses(board)
    dsn = os.path.join(ROOT, "build", NAME + ".dsn")
    ses = os.path.join(ROOT, "build", NAME + ".ses")
    os.makedirs(os.path.dirname(dsn), exist_ok=True)
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        sys.exit("DSN export failed")
    power_class_in_dsn(dsn)
    if os.path.exists(ses):
        os.remove(ses)
    subprocess.run(["java", "-jar", jar, "--gui.enabled=false",
                    "-de", dsn, "-do", ses, "-mp", "100"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not os.path.exists(ses):
        sys.exit("Freerouting wrote no session file")
    n = import_ses(board, ses)
    print("tracks and vias from Freerouting:", n)
    board.Save(path)


def sexp(text):
    """Specctra text into nested lists."""
    import re
    toks = re.findall(r'\(|\)|"[^"]*"|[^\s()]+', text)
    stack = [[]]
    for t in toks:
        if t == "(":
            stack.append([])
        elif t == ")":
            done = stack.pop()
            stack[-1].append(done)
        else:
            stack[-1].append(t.strip('"'))
    return stack[0][0]


def import_ses(board, ses):
    """The routes of a Freerouting session as tracks and vias (pcbnew's
    own importer needs the editor window)."""
    root = sexp(open(ses).read())
    routes = next(x for x in root if isinstance(x, list) and x[0] == "routes")
    res = next(x for x in routes if isinstance(x, list) and x[0] == "resolution")
    scale = {"um": 1e-3, "mm": 1.0, "mil": 0.0254}[res[1]] / float(res[2])
    layers = {"F.Cu": pcbnew.F_Cu, "B.Cu": pcbnew.B_Cu}
    out = next(x for x in routes if isinstance(x, list) and x[0] == "network_out")
    count = 0
    for net in out[1:]:
        ni = board.FindNet(net[1])
        for item in net[2:]:
            if item[0] == "wire":
                path = next(x for x in item if isinstance(x, list) and x[0] == "path")
                layer, w = layers[path[1]], float(path[2]) * scale
                pts = [(float(path[i]) * scale, -float(path[i + 1]) * scale)
                       for i in range(3, len(path) - 1, 2)]
                for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
                    t = pcbnew.PCB_TRACK(board)
                    t.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
                    t.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
                    t.SetWidth(mm(w))
                    t.SetLayer(layer)
                    t.SetNet(ni)
                    board.Add(t)
                    count += 1
            elif item[0] == "via":
                import re
                m = re.search(r"_(\d+):(\d+)_um", item[1])
                d, drill = (int(m.group(1)) / 1000, int(m.group(2)) / 1000) \
                    if m else (0.6, 0.3)
                v = pcbnew.PCB_VIA(board)
                v.SetPosition(pcbnew.VECTOR2I(mm(float(item[2]) * scale),
                                              mm(-float(item[3]) * scale)))
                v.SetWidth(mm(d))
                v.SetDrill(mm(drill))
                v.SetNet(ni)
                board.Add(v)
                count += 1
    return count


def finish(path):
    board = pcbnew.LoadBoard(path)
    gnd = board.FindNet("GND")
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(gnd)
        z.SetLocalClearance(mm(0.3))
        z.SetMinThickness(mm(0.25))
        z.SetThermalReliefGap(mm(0.4))
        z.SetThermalReliefSpokeWidth(mm(0.4))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        ol = z.Outline()
        ol.NewOutline()
        for x, y in circle_pts(BOARD_R - 0.1):
            ol.Append(V(x, y))
        z.SetZoneName("GND_" + ("TOP" if layer == pcbnew.F_Cu else "BOT"))
        board.Add(z)
    # stitching: GND vias on a 2.5 mm grid wherever they fit, so no piece
    # of the top pour is cut off from the bottom one
    n = 0
    for gx in range(-10, 11):
        for gy in range(-10, 11):
            x, y = gx * 2.5, gy * 2.5
            if math.hypot(x, y) < BOARD_R - 1.6:
                n += add_via(board, gnd, x, y)
    print("GND stitching vias:", n)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(path)
    return board


def add_via(board, net, x, y):
    """A GND via at x, y if it keeps clear of pads, part bodies, other
    nets' copper and the holes; 1 if placed."""
    pos = V(x, y)
    keep = pcbnew.BOX2I(V(x - 0.85, y - 0.85),
                        pcbnew.VECTOR2I(mm(1.7), mm(1.7)))
    for fp in board.GetFootprints():
        if fp.GetReference().startswith("H"):
            continue
        for pad in fp.Pads():
            if pad.GetBoundingBox().Intersects(keep):
                return 0
        if not fp.IsFlipped():
            fp.BuildCourtyardCaches()
            cy = fp.GetCourtyard(pcbnew.F_CrtYd)
            if cy.OutlineCount() and cy.Collide(pos, mm(0.4)):
                return 0
    for hx, hy in HOLES:
        if math.hypot(hx - x, hy - y) < 3.6:
            return 0
    for d in board.GetDrawings():           # not through the lettering
        if d.GetClass() == "PCB_TEXT" and d.GetBoundingBox().Intersects(keep):
            return 0
    for fp in board.GetFootprints():
        r = fp.Reference()
        if r.IsVisible() and r.GetBoundingBox().Intersects(keep):
            return 0
    for t in board.GetTracks():
        if t.GetNetCode() == net.GetNetCode():
            continue
        if t.HitTest(pos, mm(0.3 + 0.3)):
            return 0
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(pos)
    v.SetWidth(mm(0.6))
    v.SetDrill(mm(0.3))
    v.SetNet(net)
    board.Add(v)
    return 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--freerouting")
    a = ap.parse_args()
    path = build()
    print("placed:", path)
    if not a.freerouting:
        return
    route(path, a.freerouting)
    print("routed")
    board = finish(path)
    netclasses(board)
    rpt = os.path.join(ROOT, "build", "drc.rpt")
    pcbnew.WriteDRCReport(board, rpt, pcbnew.EDA_UNITS_MILLIMETRES, True)
    print("DRC:", rpt)
    write_project()        # saving the board wrote KiCad's defaults over it


if __name__ == "__main__":
    main()
