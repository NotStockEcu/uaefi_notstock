# Initial PCB placement generator for PDM (KiCad 10 pcbnew API).
# WARNING: overwrites PDM.kicad_pcb - only for the first draft, not after manual edits.
# Run inside the kicad/kicad:10.x container with the PDM folder mounted at /prj:
#   kicad-cli sch export netlist -o /prj/pdm.net PDM.kicad_sch
#   python3 tools/gen_pcb.py 120 95        # board width, height [mm]
import re, sys, pcbnew
exec(open('/prj/tools/net.py').read())

MM = pcbnew.FromMM
W, H = float(sys.argv[1]), float(sys.argv[2])
X0, Y0 = 100.0, 50.0            # board top-left in page coords
STD = '/usr/share/kicad/footprints'

board = pcbnew.CreateEmptyBoard()
board.SetCopperLayerCount(4)
ds = board.GetDesignSettings()

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
    for k, v in c['fields'].items():
        if k in ('Footprint', 'Datasheet', 'Description'):
            if k == 'Footprint': continue
            fld = fp.GetField(k) if fp.HasField(k) else None
            if fld: fld.SetText(v)
    fp.SetAttributes(fp.GetAttributes() & ~pcbnew.FP_EXCLUDE_FROM_BOM)
    if c['dnp']: fp.SetAttributes(fp.GetAttributes() | pcbnew.FP_DNP)
    for p in fp.Pads():
        n = pin2net.get((ref, p.GetNumber()))
        if n: p.SetNet(netinfo[n])
    board.Add(fp); fps[ref] = fp

# ---------- grouping ----------
order = [5, 6, 7, 8, 4, 3, 2, 1]                 # left -> right, matches J1 columns
chx = {ch: 7.5 + 15 * i for i, ch in enumerate(order)}
chre = re.compile(r'^/(OUT|PGND|IS|IS|DEN|PIN|LG|FLT|OVL)(\d)(_F|_N)?$')
groups = {}
def put(g, r): groups.setdefault(g, []).append(r)
cmp_pair = {'U5': (1, 2), 'U6': (3, 4), 'U7': (5, 6), 'U8': (7, 8)}
for ref in comps:
    if ref in ('J1',): continue
    if ref in ('J2', 'D1', 'C1'): put('vbat', ref); continue
    if ref in cmp_pair: put('cmp' + ref, ref); continue
    chs = {int(m.group(2)) for (r, p), n in pin2net.items() if r == ref for m in [chre.match(n)] if m}
    if len(chs) == 1: put(f'ch{chs.pop()}', ref); continue
    n = int(re.sub(r'\D', '', ref)); k = re.sub(r'\d', '', ref)
    if k == 'C' and 31 <= n <= 59 and (n - 31) % 4 == 0: put(f'ch{(n - 31) // 4 + 1}', ref); continue
    if k == 'C' and 25 <= n <= 28: put(f'cmpU{n - 20}', ref); continue
    if ref in ('U1', 'U2', 'L1', 'D2', 'D3', 'R1', 'R2') or (k == 'C' and 2 <= n <= 12): put('power', ref); continue
    if ref in ('U3', 'JP1', 'JP2', 'JP3') or (k == 'R' and 3 <= n <= 12) or (k == 'C' and 13 <= n <= 17) or (k == 'D' and 4 <= n <= 8):
        put('inputs', ref); continue
    put('logic', ref)

# regions (x0, y0, x1, y1) relative to board
Hc = H - 15.9                                     # J1 row A y (edge 7.5 mm below row C)
reg = {'vbat': (38, 2, 82, 19), 'power': (86, 58, W - 2, H - 2), 'logic': (2, 58, 35, H - 2), 'inputs': (37, 58, 84, Hc - 3.5)}
for ch, x in chx.items(): reg[f'ch{ch}'] = (x - 7.2, 20, x + 7.2, 46)
for u, (a, b) in cmp_pair.items():
    xc = (chx[a] + chx[b]) / 2; reg['cmp' + u] = (xc - 14, 47, xc + 14, 57)

def bbox(fp):
    b = fp.GetCourtyard(pcbnew.F_CrtYd).BBox() if fp.GetCourtyard(pcbnew.F_CrtYd).OutlineCount() else fp.GetBoundingBox(False)
    return b

def place_at(fp, x, y, rot=0):
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(pcbnew.VECTOR2I(MM(X0 + x), MM(Y0 + y)))

overflow = []
def pack(g, refs, first=None):
    x0, y0, x1, y1 = reg[g]
    items = sorted(refs, key=lambda r: -(bbox(fps[r]).GetWidth() * bbox(fps[r]).GetHeight()))
    cx, cy, rowh, gap = x0, y0, 0, 0.6
    if first:                                   # big part centred on top, the rest packed below
        items.remove(first); fp = fps[first]; place_at(fp, 0, 0); b = bbox(fp)
        place_at(fp, (x0 + x1) / 2 - (b.GetCenter().x - fp.GetPosition().x) / 1e6, y0 + (fp.GetPosition().y - b.GetY()) / 1e6)
        cy = y0 + b.GetHeight() / 1e6 + gap
    for r in items:
        fp = fps[r]; place_at(fp, 0, 0)
        b = bbox(fp); w, h = b.GetWidth() / 1e6, b.GetHeight() / 1e6
        ox, oy = (fp.GetPosition().x - b.GetX()) / 1e6, (fp.GetPosition().y - b.GetY()) / 1e6
        if cx + w > x1 + 1e-6: cx, cy, rowh = x0, cy + rowh + gap, 0
        if cy + h > y1 + 1e-6: overflow.append((g, r))
        place_at(fp, cx + ox, cy + oy); cx += w + gap; rowh = max(rowh, h)

# connector: pattern centre (-12.475) on board centre, PCB edge 7.5 mm below row C
place_at(fps['J1'], W / 2 + 12.475, Hc)
place_at(fps['J2'], W / 2, 10.5)
for g, refs in groups.items():
    if g == 'vbat':
        place_at(fps['D1'], W / 2 - 14, 10.5, 90); place_at(fps['C1'], W / 2 + 14, 10.5)
    elif g.startswith('ch'):
        ch = int(g[2:]); u = f'U{9 + ch}'
        pack(g, refs, first=u)
    else:
        pack(g, refs)

# ---------- outline, mounting holes, text ----------
def seg(a, b, layer=pcbnew.Edge_Cuts, w=0.1):
    s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(pcbnew.VECTOR2I(MM(X0 + a[0]), MM(Y0 + a[1]))); s.SetEnd(pcbnew.VECTOR2I(MM(X0 + b[0]), MM(Y0 + b[1])))
    s.SetLayer(layer); s.SetWidth(MM(w)); board.Add(s)
pts = [(0, 0), (W, 0), (W, H), (0, H)]
for i in range(4): seg(pts[i], pts[(i + 1) % 4])
for i, (x, y) in enumerate([(4, 4), (W - 4, 4), (4, H - 4), (W - 4, H - 4)]):
    mh = pcbnew.FootprintLoad(f'{STD}/MountingHole.pretty', 'MountingHole_3.2mm_M3')
    mh.SetReference(f'H{i + 1}'); mh.SetValue('M3')
    mh.SetAttributes(mh.GetAttributes() | pcbnew.FP_BOARD_ONLY | pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
    place_at(mh, x, y); board.Add(mh)
t = pcbnew.PCB_TEXT(board); t.SetText('PDM 8CH rev 0.1'); t.SetLayer(pcbnew.F_SilkS)
t.SetPosition(pcbnew.VECTOR2I(MM(X0 + 47), MM(Y0 + 53))); t.SetTextSize(pcbnew.VECTOR2I(MM(1.5), MM(1.5))); board.Add(t)

# ---------- zones ----------
def zone(net, layer, poly, prio=0):
    z = pcbnew.ZONE(board); z.SetLayer(layer); z.SetNet(netinfo[net]); z.SetAssignedPriority(prio)
    z.SetLocalClearance(MM(0.3)); z.SetMinThickness(MM(0.25))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL if net == '/VBAT' else pcbnew.ZONE_CONNECTION_THERMAL)
    ol = z.Outline(); ol.NewOutline()
    for x, y in poly: ol.Append(MM(X0 + x), MM(Y0 + y))
    board.Add(z); return z
m = 0.5
full = [(m, m), (W - m, m), (W - m, H - m), (m, H - m)]
vb = [(m, m), (W - m, m), (W - m, 27.5), (m, 27.5)]            # VBAT bus under stud + BTS tabs
zone('/GND', pcbnew.In1_Cu, full)
zone('/VBAT', pcbnew.In2_Cu, vb, 1); zone('/GND', pcbnew.In2_Cu, full)
zone('/VBAT', pcbnew.F_Cu, vb, 1)
zone('/VBAT', pcbnew.B_Cu, vb, 1); zone('/GND', pcbnew.B_Cu, full)
pcbnew.ZONE_FILLER(board).Fill(board.Zones())

board.SetFileName('/prj/PDM.kicad_pcb')
pcbnew.SaveBoard('/prj/PDM.kicad_pcb', board)
print('groups', {g: len(r) for g, r in groups.items()})
print('overflow', overflow)
