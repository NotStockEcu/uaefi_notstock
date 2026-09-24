# Initial PCB generator for PDM (KiCad 10 pcbnew API): placement, power copper, zones.
# WARNING: overwrites PDM.kicad_pcb - only for the first draft, not after manual edits.
# Run inside the kicad/kicad:10.x container with the PDM folder mounted at /prj:
#   kicad-cli sch export netlist -o /prj/pdm.net PDM.kicad_sch
#   python3 tools/gen_pcb.py
#   python3 tools/dsn.py            -> PDM.dsn (net classes, In2 = VBAT kept free of signals)
#   java -jar freerouting-2.4.1-executable.jar --gui.enabled=false -de PDM.dsn -do PDM.ses -mp 12   (needs Java 25)
#   python3 tools/ses.py            -> imports the routes, refills zones
import re, pcbnew
exec(open('/prj/tools/net.py').read())

MM = pcbnew.FromMM
W, H = 124.0, 106.0
X0, Y0 = 100.0, 50.0            # board top-left in page coords
STD = '/usr/share/kicad/footprints'

board = pcbnew.CreateEmptyBoard()
board.SetCopperLayerCount(4)

# ---------- footprints + nets ----------
netinfo = {}
for n in nets:
    ni = pcbnew.NETINFO_ITEM(board, n); board.Add(ni); netinfo[n] = ni
pin2net = {}
for n, nodes in nets.items():
    for r, p in nodes: pin2net[(r, p)] = n

fps = {}
for ref, c in comps.items():
    lib, name = c['fp'].split(':')
    path = '/prj/PDM.pretty' if lib == 'PDM' else f'{STD}/{lib}.pretty'
    fp = pcbnew.FootprintLoad(path, name)
    assert fp, c['fp']
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    fp.SetReference(ref); fp.SetValue(c['value'])
    fp.SetPath(pcbnew.KIID_PATH('/' + c['uuid']))
    fp.SetSheetname('/'); fp.SetSheetfile('PDM.kicad_sch')
    for k in ('Description', 'Datasheet'):
        if fp.HasField(k): fp.GetField(k).SetText(c['fields'].get(k, '') if k == 'Description' else ('' if lib == 'PDM' else fp.GetField(k).GetText()))
    fp.SetAttributes(fp.GetAttributes() & ~pcbnew.FP_EXCLUDE_FROM_BOM)
    if c['dnp']: fp.SetAttributes(fp.GetAttributes() | pcbnew.FP_DNP)
    for p in fp.Pads():
        n = pin2net.get((ref, p.GetNumber()))
        if n: p.SetNet(netinfo[n])
    board.Add(fp); fps[ref] = fp

# ---------- floorplan ----------
# Channel blocks left -> right, chosen so each output has a short, non-crossing path to its J1 pins:
#   J1 col 8/7/6 (left): OUT4 A7+A8, OUT5 B8, OUT6 C8, OUT7 B7+C7, OUT8 A6+B6;  col 1/2 (right): OUT1 A, OUT2 B, OUT3 C
order = [7, 5, 6, 4, 8, 1, 2, 3]
bx = {ch: 9.5 + 15 * i for i, ch in enumerate(order)}
BTS_Y = 74.5                                      # BTS rotated 270: control pins up, OUT pins 8-14 down, EP = VBAT
TOP = BTS_Y + 2.1                                 # output copper starts just below the OUT pins
PLANE_Y = BTS_Y + 1.7                             # VBAT / GND planes end here, below are the outputs
Hc = H - 15.9                                     # J1 row A (PCB edge is 7.5 mm below row C)
A, B, C = Hc, Hc + 4.2, Hc + 8.4
X1 = bx[8] + 17.47                                # J1 col 1 x, puts col 6 (OUT8) right under the CH8 block
col = {i + 1: X1 - d for i, d in enumerate([0, 4.15, 7.48, 10.81, 14.14, 17.47, 20.8, 24.95])}
PEGL = (X1 - 32.775, Hc + 6.3, 2.2, 5.0)          # NPTH pegs (x, y, w, h)
PEGR = (X1 + 7.825, Hc + 7.05, 2.2, 3.5)

chre = re.compile(r'^/(OUT|PGND|IS|DEN|PIN|LG|FLT|OVL)(\d)(_F|_N)?$')
groups = {}
def put(g, r): groups.setdefault(g, []).append(r)
cmp_pair = {'U5': (1, 2), 'U6': (3, 4), 'U7': (5, 6), 'U8': (7, 8)}
for ref in comps:
    if ref in ('J1',): continue
    if ref in ('J2', 'D1', 'C1'): put('vbat', ref); continue
    if ref in cmp_pair: put('cmp' + ref, ref); continue
    if re.fullmatch(r'U1[0-7]', ref): continue
    chs = {int(m.group(2)) for (r, p), n in pin2net.items() if r == ref for m in [chre.match(n)] if m}
    if len(chs) == 1: put(f'ch{chs.pop()}', ref); continue
    n = int(re.sub(r'\D', '', ref)); k = re.sub(r'\d', '', ref)
    if k == 'C' and 31 <= n <= 59 and (n - 31) % 4 == 0: put(f'ch{(n - 31) // 4 + 1}', ref); continue
    if k == 'C' and 25 <= n <= 28: put(f'cmpU{n - 20}', ref); continue
    if ref in ('U1', 'U2', 'L1', 'D2', 'D3', 'R1', 'R2') or (k == 'C' and 2 <= n <= 12): put('power', ref); continue
    if ref in ('U3', 'JP1', 'JP2', 'JP3') or (k == 'R' and 3 <= n <= 12) or (k == 'C' and 13 <= n <= 17) or (k == 'D' and 4 <= n <= 8):
        put('inputs', ref); continue
    put('logic', ref)

reg = {'logic': (8, 2, 40, 39), 'vbat': (42, 2, 82, 17), 'inputs': (42, 18.5, 82, 39), 'power': (84, 2, W - 8, 39)}
for ch, x in bx.items(): reg[f'ch{ch}'] = (x - 7.2, 50, x + 7.2, 70.4)
cmp_x = {'U8': 9.5, 'U7': 32, 'U6': 62, 'U5': 92}
for u, x in cmp_x.items(): reg['cmp' + u] = (x - 8, 39.8, x + 8, 49.6)

def bbox(fp):
    cy = fp.GetCourtyard(pcbnew.F_CrtYd)
    return cy.BBox() if cy.OutlineCount() else fp.GetBoundingBox(False)

def place_at(fp, x, y, rot=0):
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(pcbnew.VECTOR2I(MM(X0 + x), MM(Y0 + y)))

overflow = []
def pack(g, refs):
    x0, y0, x1, y1 = reg[g]
    items = sorted(refs, key=lambda r: (-(bbox(fps[r]).GetWidth() * bbox(fps[r]).GetHeight()), r))
    cx, cy, rowh, gap = x0, y0, 0, 1.0
    for r in items:
        fp = fps[r]; place_at(fp, 0, 0)
        b = bbox(fp); w, h = b.GetWidth() / 1e6, b.GetHeight() / 1e6
        ox, oy = (fp.GetPosition().x - b.GetX()) / 1e6, (fp.GetPosition().y - b.GetY()) / 1e6
        if cx + w > x1 + 1e-6: cx, cy, rowh = x0, cy + rowh + gap, 0
        if cy + h > y1 + 1e-6: overflow.append((g, r))
        place_at(fp, cx + ox, cy + oy); cx += w + gap; rowh = max(rowh, h)

place_at(fps['J1'], X1, Hc)
for ch, x in bx.items(): place_at(fps[f'U{9 + ch}'], x, BTS_Y, 270)
vx = (reg['vbat'][0] + reg['vbat'][2]) / 2
place_at(fps['J2'], vx, 9.5); place_at(fps['D1'], vx - 13, 9.5, 90); place_at(fps['C1'], vx + 13, 9.5)
for g, refs in groups.items():
    if g != 'vbat': pack(g, refs)

# ---------- outline, mounting holes, text ----------
def seg(a, b):
    s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(pcbnew.VECTOR2I(MM(X0 + a[0]), MM(Y0 + a[1]))); s.SetEnd(pcbnew.VECTOR2I(MM(X0 + b[0]), MM(Y0 + b[1])))
    s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1)); board.Add(s)
pts = [(0, 0), (W, 0), (W, H), (0, H)]
for i in range(4): seg(pts[i], pts[(i + 1) % 4])
for i, (x, y) in enumerate([(3.5, 3.5), (W - 3.5, 3.5), (3.5, H - 3.5), (W - 3.5, H - 3.5)]):
    mh = pcbnew.FootprintLoad(f'{STD}/MountingHole.pretty', 'MountingHole_3.2mm_M3')
    mh.SetReference(f'H{i + 1}'); mh.SetValue('M3')
    mh.SetAttributes(mh.GetAttributes() | pcbnew.FP_BOARD_ONLY | pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
    place_at(mh, x, y); board.Add(mh)
t = pcbnew.PCB_TEXT(board); t.SetText('PDM 8CH rev 0.1'); t.SetLayer(pcbnew.F_SilkS)
t.SetPosition(pcbnew.VECTOR2I(MM(X0 + 32), MM(Y0 + H - 3))); t.SetTextSize(pcbnew.VECTOR2I(MM(1.5), MM(1.5))); board.Add(t)

# ---------- thermal vias under every BTS exposed pad (VBAT) ----------
for ch, x in bx.items():
    for dx in (-1.35, -0.45, 0.45, 1.35):
        for dy in (-0.6, 0.6):
            v = pcbnew.PCB_VIA(board); v.SetPosition(pcbnew.VECTOR2I(MM(X0 + x + dx), MM(Y0 + BTS_Y + dy)))
            v.SetWidth(MM(0.6)); v.SetDrill(MM(0.3)); v.SetNet(netinfo['/VBAT']); v.SetIsFree(False); board.Add(v)

# ---------- zones ----------
L = {'F': pcbnew.F_Cu, 'I1': pcbnew.In1_Cu, 'I2': pcbnew.In2_Cu, 'B': pcbnew.B_Cu}
def zone(net, layers, rect, prio, pads='full', name=''):
    x0, y0, x1, y1 = rect
    for l in layers:
        z = pcbnew.ZONE(board); z.SetLayer(L[l]); z.SetNet(netinfo[net]); z.SetAssignedPriority(prio)
        z.SetLocalClearance(MM(0.3)); z.SetMinThickness(MM(0.25)); z.SetZoneName(name)
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        ol = z.Outline(); ol.NewOutline()
        for px, py in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)): ol.Append(MM(X0 + px), MM(Y0 + py))
        board.Add(z)

ALL = ('F', 'I1', 'I2', 'B')
rs, rb = 1.125, 1.4                               # J1 pad radius: 1.5 mm / 2.8 mm terminals
out = {                                           # net: (layers, [rects])
    '/OUT7': (('B', 'I2'), [(bx[7] - 3.5, TOP, bx[7] + 3.5, H - 1.5), (bx[7] - 3.5, C + rb + .3, col[7] + 1.3, H - 1.5),
                            (col[7] - 1.6, B - 1.5, col[7] + 1.3, H - 1.5)]),
    '/OUT5': (('B', 'I2'), [(bx[5] - 3.5, TOP, bx[5] + 3.5, B + 1.6), (bx[5] - 3.5, A + rb + .3, PEGL[0] - 1.5, B + 1.6),
                            (PEGL[0] - 1.6, A + rb + .3, PEGL[0] + 1.6, PEGL[1] - PEGL[3] / 2 - .3),
                            (PEGL[0] + 1.5, A + rb + .3, col[8] + 1.5, B + 1.6)]),
    '/OUT6': (('F', 'I1'), [(bx[6] - 3.5, TOP, bx[6] + 3.5, H - 1.5), (bx[6] - 3.5, C + rb + .3, col[7] - rs - .4, H - 1.5),
                            (col[8] - 1.8, B + 2.0, col[8] + 1.8, H - 1.5)]),
    '/OUT4': (ALL, [(bx[4] - 3.5, TOP, bx[4] + 3.5, A - 3), (bx[4] - 3.5, A - 4, col[7] + rs, A + rb)]),
    '/OUT8': (ALL, [(bx[8] - 3.5, TOP, bx[8] + 3.5, A - 4.6), (col[7] + rs + .5, A - 4.8, col[5] + .2, A - 2.0),
                    (col[6] - 1.9, A - 2.4, col[6] + 1.9, B + 1.5)]),
    '/OUT1': (ALL, [(bx[1] - 3.5, TOP, bx[1] + 3.5, A - 3), (col[3] + rs + .35, A - 4, col[1] + 4, A + rb)]),
    '/OUT2': (ALL, [(bx[2] - 3.5, TOP, bx[2] + 3.5, B + 1.6), (col[3] + rs + .35, A + rb + .3, bx[2] + 3.5, B + 1.6)]),
    '/OUT3': (ALL, [(bx[3] - 3.5, TOP, bx[3] + 3.5, H - 1.5), (col[2] - 1.9, C + rb + .1, bx[3] + 3.5, H - 1.5),
                    (col[2] - 1.9, B + 2.0, col[1] + 1.9, H - 1.5)]),
}
# OUT5 / OUT7 run on B.Cu + In2; the BTS OUT pins are on F.Cu, so they also get F.Cu down to the connector area
extra_f = {'/OUT7': (bx[7] - 3.5, TOP, bx[7] + 3.5, C + rb), '/OUT5': (bx[5] - 3.5, TOP, bx[5] + 3.5, B + 1.6)}
# via arrays right below each BTS tie all copper layers of the output together (otherwise only F.Cu carries current there)
for ch, x in bx.items():
    for i in range(5):
        for j in range(6):
            if i == 2 and j == 0: continue          # BTS pin 11 (NC) sits right above
            v = pcbnew.PCB_VIA(board); v.SetPosition(pcbnew.VECTOR2I(MM(X0 + x - 2.4 + 1.2 * i), MM(Y0 + TOP + 1.2 + 1.2 * j)))
            v.SetWidth(MM(0.6)); v.SetDrill(MM(0.3)); v.SetNet(netinfo[f'/OUT{ch}']); v.SetIsFree(False); board.Add(v)
prio = 10                                         # overlapping zones need distinct priorities
for net, r in extra_f.items(): zone(net, ('F',), r, prio, name=net[1:]); prio += 1
for net, (layers, rects) in out.items():
    for r in rects: zone(net, layers, r, prio, name=net[1:]); prio += 1

m = 0.5
zone('/VBAT', ('I2',), (m, m, W - m, PLANE_Y), 5, name='VBAT')                 # VBAT plane stud -> BTS tabs
zone('/VBAT', ('F', 'B'), (reg['vbat'][0], m, reg['vbat'][2], reg['vbat'][3]), 5, name='VBAT_STUD')
zone('/VBAT', ('B',), (m, 48, W - m, PLANE_Y), 5, name='VBAT_B')               # under the channel blocks
zone('/GND', ('I1',), (m, m, W - m, PLANE_Y), 5, 'thermal', 'GND')
zone('/GND', ('F', 'B'), (m, m, W - m, H - m), 0, 'thermal', 'GND_FILL')
zone('/GND', ('I1', 'I2'), (m, PLANE_Y, W - m, H - m), 0, 'thermal', 'GND_FILL')

board.SetFileName('/prj/PDM.kicad_pcb')
pcbnew.SaveBoard('/prj/PDM.kicad_pcb', board)
print('groups', {g: len(r) for g, r in groups.items()})
print('overflow', overflow)
