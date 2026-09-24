# Add GND stitching vias (F.Cu/B.Cu GND fill -> In1 GND plane) wherever they clear every other-net copper item.
import pcbnew, math
MM = pcbnew.FromMM
b = pcbnew.LoadBoard('/prj/PDM.kicad_pcb')
gnd = b.FindNet('/GND'); R_VIA, CLR = 0.3, 0.35
bb = b.GetBoardEdgesBoundingBox(); X0, Y0, W, HH = bb.GetX() / 1e6, bb.GetY() / 1e6, bb.GetWidth() / 1e6, bb.GetHeight() / 1e6
segs, pts, boxes = [], [], []
for t in b.GetTracks():
    if t.GetNetCode() == gnd.GetNetCode() and t.Type() != pcbnew.PCB_VIA_T: continue
    if t.Type() == pcbnew.PCB_VIA_T:          # all vias, GND included: keep hole-to-hole spacing
        pts.append((t.GetPosition().x / 1e6, t.GetPosition().y / 1e6, t.GetWidth(pcbnew.F_Cu) / 2e6))
    else:
        segs.append((t.GetStart().x / 1e6, t.GetStart().y / 1e6, t.GetEnd().x / 1e6, t.GetEnd().y / 1e6, t.GetWidth() / 2e6))
for fp in b.GetFootprints():
    for p in fp.Pads():
        if p.GetNetCode() == gnd.GetNetCode() and p.GetDrillSize().x == 0: continue
        bb = p.GetBoundingBox(); boxes.append((bb.GetX() / 1e6, bb.GetY() / 1e6, bb.GetRight() / 1e6, bb.GetBottom() / 1e6))
    cy = fp.GetCourtyard(pcbnew.F_Cu if False else pcbnew.F_CrtYd)
def dseg(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1; L = dx * dx + dy * dy
    t = 0 if L == 0 else max(0, min(1, ((px - x1) * dx + (py - y1) * dy) / L))
    return math.hypot(px - x1 - t * dx, py - y1 - t * dy)
def free(x, y):
    r = R_VIA + CLR
    if any(bx0 - r < x < bx1 + r and by0 - r < y < by1 + r for bx0, by0, bx1, by1 in boxes): return False
    if any(math.hypot(x - a, y - c) < r + rr for a, c, rr in pts): return False
    if any(dseg(x, y, *s[:4]) < r + s[4] for s in segs): return False
    return True
n = 0
y = Y0 + 3
outz = [z for z in b.Zones() if z.GetNetCode() != gnd.GetNetCode()]
while y < Y0 + HH - 3:
    x = X0 + 3
    while x < X0 + W - 3:
        if free(x, y) and not any(z.Outline().Contains(pcbnew.VECTOR2I(MM(x), MM(y))) and (z.GetZoneName().startswith('OUT') or z.GetLayer() in (pcbnew.F_Cu, pcbnew.B_Cu)) for z in outz):
            v = pcbnew.PCB_VIA(b); v.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y))); v.SetWidth(MM(0.6)); v.SetDrill(MM(0.3)); v.SetNet(gnd)
            b.Add(v); pts.append((x, y, 0.3)); n += 1
        x += 3.0
    y += 3.0
pcbnew.ZONE_FILLER(b).Fill(b.Zones()); pcbnew.SaveBoard('/prj/PDM.kicad_pcb', b)
print('stitching vias', n)
