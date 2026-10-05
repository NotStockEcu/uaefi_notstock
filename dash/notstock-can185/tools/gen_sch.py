#!/usr/bin/env python3
"""Write notstock-can185.kicad_sch from design.py.

Every pin gets a net label at its end (no wires): what the schematic
connects is exactly design.py's table, which gen_pcb.py gives the pads.
The symbols are copied from KiCad's own libraries (KICAD_SYMBOL_DIR,
default /usr/share/kicad/symbols) into the file, as KiCad does.
Unused header pins get a no-connect flag; PWR_FLAGs on the supply nets.

    python3 tools/gen_sch.py
"""
import os
import re
import uuid

from design import P

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
NAME = "notstock-can185"
LIBDIR = os.environ.get("KICAD_SYMBOL_DIR", "/usr/share/kicad/symbols")
U = lambda: str(uuid.uuid5(uuid.NAMESPACE_URL, "ns-can185/" + next(_seq)))
_seq = (str(i) for i in range(1, 10 ** 9))
ROOT_UUID = str(uuid.uuid5(uuid.NAMESPACE_URL, "ns-can185/root"))


def sym_uuid(ref):
    return str(uuid.uuid5(uuid.NAMESPACE_URL, "ns-can185/sym/" + ref))


# ----------------------------------------------------------- libraries
_cache = {}


def lib_block(lib_id):
    lib, name = lib_id.split(":")
    path = os.path.join(LIBDIR, lib + ".kicad_sym")
    if path not in _cache:
        _cache[path] = open(path).read()
    s = _cache[path]
    i = s.index('(symbol "%s"' % name)
    d = 0
    for j in range(i, len(s)):
        if s[j] == "(":
            d += 1
        elif s[j] == ")":
            d -= 1
            if d == 0:
                break
    blk = s[i:j + 1]
    m = re.search(r'\(extends "([^"]+)"\)', blk)
    if m:  # flatten: the parent's drawing under this name and value
        parent = lib_block(lib + ":" + m.group(1))[0]
        pname = m.group(1)
        blk = parent.replace('(symbol "%s_' % pname, '(symbol "%s_' % name)
        blk = blk.replace('(symbol "%s"' % pname, '(symbol "%s"' % name, 1)
        blk = re.sub(r'(\(property "Value" )"[^"]*"', r'\1"%s"' % name, blk, 1)
    return blk, name


def embedded(lib_id):
    blk, name = lib_block(lib_id)
    return blk.replace('(symbol "%s"' % name, '(symbol "%s"' % lib_id, 1)


def pins_of(lib_id):
    blk, _ = lib_block(lib_id)
    out = {}
    for m in re.finditer(r'\(pin (\w+) (\w+)\s*\(at ([-\d.]+) ([-\d.]+) (\d+)\)'
                         r'.*?\(name "([^"]*)".*?\(number "([^"]+)"', blk, re.S):
        out[m.group(7)] = (float(m.group(3)), float(m.group(4)),
                           int(m.group(5)), m.group(2))
    return out


# --------------------------------------------------------------- layout
# schematic positions (mm, y down) per block; the title of each block
BLOCKS = [
    ("Input: fuse, reverse polarity, load dump", 30, 50,
     ["J1", "F1", "D1", "D2", "C1", "C2"]),
    ("12 V -> 5 V", 30, 140,
     ["U1", "R1", "C3", "D3", "L1", "R2", "R3", "C4", "C5"]),
    ("CAN transceiver, ESD, termination (JP1 open: the car's bus is "
     "terminated already)", 230, 50,
     ["U2", "C6", "C7", "D4", "R4", "R5", "C8", "JP1"]),
    ("To the display ESP32-S3-Touch-LCD-1.85: UART socket (SH 1.0 cable, "
     "1:1) and USB-C (5 V)", 230, 172, ["J2", "R6", "J3", "R7", "R8"]),
    ("Mounting: the display's M2 holes", 230, 245, ["H1", "H2", "H3"]),
]
STEP = {"U1": 45, "U2": 45, "J1": 30, "J2": 30, "J3": 40}
WRAP = 150

FONT = '(effects (font (size 1.27 1.27))'


def fmt(v):
    return ("%.2f" % v).rstrip("0").rstrip(".")


def label(net, x, y, ang):
    just = {0: "left", 180: "right", 90: "left", 270: "right"}[ang]
    return ('  (label "%s" (at %s %s %d) (fields_autoplaced)\n'
            '    %s (justify %s bottom))\n    (uuid "%s")\n  )\n'
            % (net, fmt(x), fmt(y), ang, FONT, just, U()))


def label_angle(pin_ang):
    # the label leaves the pin end away from the symbol
    return {0: 180, 180: 0, 90: 270, 270: 90}[pin_ang]


def symbol(ref, lib_id, value, fp, x, y, pins_nets, extra_props=()):
    pins = pins_of(lib_id)
    s = ['  (symbol (lib_id "%s") (at %s %s 0) (unit 1)\n'
         '    (in_bom %s) (on_board yes) (dnp %s)\n    (uuid "%s")\n'
         % (lib_id, fmt(x), fmt(y), "no" if ref.startswith(("H", "#"))
            else "yes", "yes" if P.get(ref, {}).get("dnp") else "no",
            sym_uuid(ref))]
    ry = y - 3 if pins else y - 2
    flag = ref.startswith("#")
    props = [("Reference", ref, x + 2.5, ry, "left", flag),
             ("Value", value, x + 2.5, ry + 2, "left", flag),
             ("Footprint", fp, x, y, None, True),
             ("Datasheet", "~", x, y, None, True)] + list(extra_props)
    for k, v, px, py, just, hide in props:
        j = " (justify %s)" % just if just else ""
        h = " hide" if hide else ""
        s.append('    (property "%s" "%s" (at %s %s 0)\n      %s%s%s))\n'
                 % (k, v, fmt(px), fmt(py), FONT, j, h))
    for n in pins:
        s.append('    (pin "%s" (uuid "%s"))\n' % (n, U()))
    s.append('    (instances\n      (project "%s"\n        (path "/%s"\n'
             '          (reference "%s") (unit 1)\n        )\n      )\n    )\n'
             '  )\n' % (NAME, ROOT_UUID, ref))
    out = "".join(s)
    for n, (px, py, ang, _) in pins.items():
        ax, ay = x + px, y - py
        net = pins_nets.get(n)
        if net:
            out += label(net, ax, ay, label_angle(ang))
        else:
            out += '  (no_connect (at %s %s) (uuid "%s"))\n' % (fmt(ax), fmt(ay), U())
    return out


def text(t, x, y, size=2.0):
    return ('  (text "%s" (at %s %s 0)\n    (effects (font (size %s %s) bold)'
            ' (justify left bottom))\n    (uuid "%s")\n  )\n'
            % (t, fmt(x), fmt(y), fmt(size), fmt(size), U()))


def main():
    used = {"power:PWR_FLAG"}
    body = []
    for title, bx, by, refs in BLOCKS:
        body.append(text(title, bx - 10, by - 14))
        x, y = bx, by
        for ref in refs:
            p = P[ref]
            used.add(p["sym"])
            # LCSC and part number for JLCPCB's assembly (and its tools)
            props = [(k, v, x, y, None, True) for k, v in
                     (("LCSC", p["lcsc"]), ("MPN", p["mpn"])) if v]
            body.append(symbol(ref, p["sym"], p["val"], p["fp"], x, y,
                               p["pins"], props))
            x += STEP.get(ref, 26)
            if x > bx + WRAP:
                x, y = bx, y + 40
    # PWR_FLAGs: these nets are fed from outside or through passives
    fx = 30
    for net in ("GND", "VIN", "+5V", "+3V3", "VBAT"):
        ref = "#FLG_%s" % net.strip("+")
        body.append(symbol(ref, "power:PWR_FLAG", "PWR_FLAG", "", fx, 270,
                           {"1": net}).replace('"Reference" "%s" (at' % ref,
                           '"Reference" "%s" (at' % ref, 1))
        fx += 22
    body.append(text("NOT STOCK CAN 1.85: power and CAN board behind the "
                     "Waveshare ESP32-S3-Touch-LCD-1.85", 20, 22, 2.5))
    body.append(text("Supply flags", 20, 256))

    out = ['(kicad_sch (version 20230121) (generator eeschema)\n\n'
           '  (uuid "%s")\n\n  (paper "A3")\n\n' % ROOT_UUID,
           '  (title_block\n    (title "NOT STOCK CAN 1.85")\n'
           '    (rev "1")\n    (comment 1 "12 V -> 5 V, CAN (TJA1051T/3) for '
           'the round gauge")\n  )\n\n  (lib_symbols\n']
    for lib_id in sorted(used):
        out.append("    " + embedded(lib_id).replace("\n", "\n    ") + "\n")
    out.append("  )\n\n")
    out += body
    out.append('\n  (sheet_instances\n    (path "/" (page "1"))\n  )\n)\n')
    open(os.path.join(ROOT, NAME + ".kicad_sch"), "w").write("".join(out))
    print("wrote", NAME + ".kicad_sch")


if __name__ == "__main__":
    main()
