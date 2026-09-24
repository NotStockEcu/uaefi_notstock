# Close the gaps the autorouter left: for every connectivity island of a net, try short L-shaped links
# (F.Cu / B.Cu / In1, vias where needed) to the nearest item of the main island. Candidates are pre-filtered
# geometrically and then checked with a full DRC run; the first clean one is kept.
import pcbnew, math, re, subprocess, shutil, itertools
MM = pcbnew.FromMM
BASE, TRY = '/prj/PDM.kicad_pcb', '/prj/t/PDM.kicad_pcb'
W_TRK, R_VIA, CLR = 0.25, 0.3, 0.22
LAYERS = [pcbnew.F_Cu, pcbnew.B_Cu, pcbnew.In1_Cu]

def drc(path):
    subprocess.run(['kicad-cli', 'pcb', 'drc', '--severity-error', '--refill-zones', '-o', '/tmp/d.rpt', path], capture_output=True)
    t = open('/tmp/d.rpt').read()
    return int(re.search(r'Found (\d+) DRC violations', t).group(1)), int(re.search(r'Found (\d+) unconnected pads', t).group(1))

def load():
    b = pcbnew.LoadBoard(BASE); pcbnew.ZONE_FILLER(b).Fill(b.Zones()); b.BuildConnectivity(); return b

def islands(b, net):
    c = b.GetConnectivity(); code = b.FindNet(net).GetNetCode()
    items = [t for t in b.GetTracks() if t.GetNetCode() == code] + [p for f in b.GetFootprints() for p in f.Pads() if p.GetNetCode() == code]
    seen, groups = set(), []
    for it in items:
        k = it.m_Uuid.AsString()
        if k in seen: continue
        grp = {i.m_Uuid.AsString() for i in c.GetConnectedItems(it)} | {k}
        seen |= grp; groups.append([i for i in items if i.m_Uuid.AsString() in grp])
    return sorted(groups, key=len, reverse=True)

def anchors(it):
    """(x, y, layers the anchor can connect on)"""
    if it.Type() == pcbnew.PCB_VIA_T: return [(it.GetPosition().x / 1e6, it.GetPosition().y / 1e6, set(LAYERS))]
    if it.Type() == pcbnew.PCB_PAD_T:
        th = it.GetDrillSize().x > 0
        return [(it.GetPosition().x / 1e6, it.GetPosition().y / 1e6, set(LAYERS) if th else {pcbnew.F_Cu})]
    return [(p.x / 1e6, p.y / 1e6, {it.GetLayer()}) for p in (it.GetStart(), it.GetEnd())]

def obstacles(b, code):
    obs = {l: [] for l in LAYERS}          # per layer: ('seg', x1, y1, x2, y2, r) / ('box', x0, y0, x1, y1)
    for t in b.GetTracks():
        if t.GetNetCode() == code: continue
        if t.Type() == pcbnew.PCB_VIA_T:
            x, y = t.GetPosition().x / 1e6, t.GetPosition().y / 1e6
            for l in LAYERS: obs[l].append(('seg', x, y, x, y, t.GetWidth(pcbnew.F_Cu) / 2e6))
        elif t.GetLayer() in obs:
            obs[t.GetLayer()].append(('seg', t.GetStart().x / 1e6, t.GetStart().y / 1e6, t.GetEnd().x / 1e6, t.GetEnd().y / 1e6, t.GetWidth() / 2e6))
    for f in b.GetFootprints():
        for p in f.Pads():
            if p.GetNetCode() == code: continue
            bb = p.GetBoundingBox(); box = ('box', bb.GetX() / 1e6, bb.GetY() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6)
            for l in (LAYERS if p.GetDrillSize().x > 0 else [pcbnew.F_Cu]): obs[l].append(box)
    for z in b.Zones():                    # output copper and the VBAT plane must not be crossed
        if z.GetNetCode() != code and (z.GetZoneName().startswith('OUT') or z.GetLayer() == pcbnew.In2_Cu):
            bb = z.GetBoundingBox(); box = ('box', bb.GetX() / 1e6, bb.GetY() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6)
            if z.GetLayer() in obs: obs[z.GetLayer()].append(box)
    return obs

def seg_seg(a, b):
    def d(p, s):
        (px, py), (x1, y1, x2, y2) = p, s
        dx, dy = x2 - x1, y2 - y1; L = dx * dx + dy * dy
        t = 0 if L == 0 else max(0, min(1, ((px - x1) * dx + (py - y1) * dy) / L))
        return math.hypot(px - x1 - t * dx, py - y1 - t * dy)
    def cross(s, t):
        o = lambda a, b, c: (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        p1, p2, p3, p4 = s[:2], s[2:], t[:2], t[2:]
        return o(p1, p2, p3) * o(p1, p2, p4) < 0 and o(p3, p4, p1) * o(p3, p4, p2) < 0
    if cross(a, b): return 0
    return min(d(a[:2], b), d(a[2:], b), d(b[:2], a), d(b[2:], a))

def clear(obs, layer, s, r):
    for o in obs[layer]:
        if o[0] == 'seg':
            if seg_seg(s, o[1:5]) < r + o[5] + CLR: return False
        else:
            x0, y0, x1, y1 = o[1] - r - CLR, o[2] - r - CLR, o[3] + r + CLR, o[4] + r + CLR
            if seg_seg(s, (x0, y0, x1, y0)) == 0 or seg_seg(s, (x0, y1, x1, y1)) == 0 or seg_seg(s, (x0, y0, x0, y1)) == 0 or \
               seg_seg(s, (x1, y0, x1, y1)) == 0 or (x0 < s[0] < x1 and y0 < s[1] < y1) or (x0 < s[2] < x1 and y0 < s[3] < y1): return False
    return True

def candidates(obs, A, Bp):
    ax, ay, al = A; bx, by, bl = Bp
    for L in LAYERS:
        for corner in ((bx, ay), (ax, by)):
            segs = [(ax, ay, *corner), (*corner, bx, by)]
            if not all(clear(obs, L, s, W_TRK / 2) for s in segs if s[:2] != s[2:]): continue
            vias = [p for p, ls in (((ax, ay), al), ((bx, by), bl)) if L not in ls]
            if any(not all(clear(obs, l, (*v, *v), R_VIA) for l in LAYERS) for v in vias): continue
            yield L, corner, vias

def apply(net, A, Bp, L, corner, vias):
    b = pcbnew.LoadBoard(BASE); n = b.FindNet(net); P = lambda x, y: pcbnew.VECTOR2I(MM(x), MM(y))
    for s, e in ((A[:2], corner), (corner, Bp[:2])):
        if s == e: continue
        t = pcbnew.PCB_TRACK(b); t.SetStart(P(*s)); t.SetEnd(P(*e)); t.SetWidth(MM(W_TRK)); t.SetLayer(L); t.SetNet(n); b.Add(t)
    for v in vias:
        o = pcbnew.PCB_VIA(b); o.SetPosition(P(*v)); o.SetWidth(MM(0.6)); o.SetDrill(MM(0.3)); o.SetNet(n); b.Add(o)
    pcbnew.SaveBoard(TRY, b)

v0, u0 = drc(BASE); print('start', v0, u0, flush=True)
rep = open('/tmp/d.rpt').read()
bad = set(re.findall(r'\[(/[^\]]+)\]', ''.join(bl for bl in re.split(r'\n(?=\[)', rep) if bl.startswith('[unconnected'))))
b = load()
nets = sorted(bad); print('nets', nets, flush=True)
for net in nets:
    net = str(net); grp = islands(b, net)
    if len(grp) < 2: continue
    obs = obstacles(b, b.FindNet(net).GetNetCode())
    main = [a for it in grp[0] for a in anchors(it)]
    for isl in grp[1:]:
        pairs = sorted(((a, m) for it in isl for a in anchors(it) for m in main), key=lambda p: math.hypot(p[0][0] - p[1][0], p[0][1] - p[1][1]))[:40]
        ok = False; tried = 0
        for A, Bp in pairs:
            for L, corner, vias in candidates(obs, A, Bp):
                tried += 1; apply(net, A, Bp, L, corner, vias); v, u = drc(TRY)
                if v <= v0 and u < u0:
                    shutil.copy(TRY, BASE); v0, u0 = v, u; ok = True
                    print('joined', net, A[:2], '->', Bp[:2], 'on', pcbnew.BOARD.GetStandardLayerName(L), 'vias', len(vias), '->', v, u, flush=True)
                    break
                if tried >= 12: break
            if ok or tried >= 12: break
        if not ok: print('open', net, 'island of', len(isl), 'items, tried', tried, flush=True)
        else: b = load(); obs = obstacles(b, b.FindNet(net).GetNetCode()); main = [a for it in islands(b, net)[0] for a in anchors(it)]
print('end', v0, u0)
