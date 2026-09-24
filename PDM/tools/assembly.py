# JLCPCB-style assembly files: fab/PDM_BOM.csv (grouped) and fab/PDM_CPL.csv (placement, mm from board bottom-left).
import pcbnew, csv, re, collections
b = pcbnew.LoadBoard('/prj/PDM.kicad_pcb')
bb = b.GetBoardEdgesBoundingBox(); X0, Y1 = bb.GetX() / 1e6, bb.GetBottom() / 1e6
key = lambda r: (re.sub(r'\d', '', r), int(re.sub(r'\D', '', r)))
cpl, grp = [], collections.defaultdict(list)
for fp in sorted(b.GetFootprints(), key=lambda f: key(f.GetReference())):
    ref = fp.GetReference(); a = fp.GetAttributes()
    if a & (pcbnew.FP_DNP | pcbnew.FP_BOARD_ONLY | pcbnew.FP_EXCLUDE_FROM_BOM) or ref == 'J2': continue
    mpn = fp.GetField('MPN').GetText() if fp.HasField('MPN') else ''
    man = fp.GetField('Manufacturer').GetText() if fp.HasField('Manufacturer') else ''
    fpname = str(fp.GetFPID().GetLibItemName())
    tht = bool(a & pcbnew.FP_THROUGH_HOLE)
    p = fp.GetPosition()
    cpl.append([ref, f'{p.x / 1e6 - X0:.3f}mm', f'{Y1 - p.y / 1e6:.3f}mm', 'Top' if fp.GetLayer() == pcbnew.F_Cu else 'Bottom',
                f'{fp.GetOrientationDegrees() % 360:.0f}'])
    grp[(fpname, man, mpn, 'THT' if tht else 'SMD')].append((ref, fp.GetValue()))
with open('/prj/fab/PDM_CPL.csv', 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['Designator', 'Mid X', 'Mid Y', 'Layer', 'Rotation']); w.writerows(cpl)
with open('/prj/fab/PDM_BOM.csv', 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['Comment', 'Designator', 'Footprint', 'Quantity', 'Manufacturer', 'Manufacturer Part Number', 'LCSC Part #', 'Type'])
    for (fpn, man, mpn, t), items in sorted(grp.items(), key=lambda kv: (kv[0][3] == 'THT', -len(kv[1]), kv[0][2])):
        refs = sorted((r for r, _ in items), key=key)
        val = min((v for _, v in items), key=len)          # shortest spelling, e.g. '100n' for 100n + 100n/50V
        w.writerow([val, ','.join(refs), fpn, len(refs), man, mpn, '', t])
print('placed', len(cpl), 'bom lines', len(grp), 'board', round(bb.GetWidth() / 1e6, 2), 'x', round(bb.GetHeight() / 1e6, 2))
