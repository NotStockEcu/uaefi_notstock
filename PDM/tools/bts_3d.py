# 3D model for Infineon PG-TSDSO-14 (BTS7002-1EPP) per datasheet Fig. 38, in KiCad footprint orientation
# (pins 1-7 at x = -2.85, pin 1 at the top; model y axis points up = footprint -y).
import cadquery as cq
BODY_X, BODY_Y, H, STANDOFF = 3.9, 4.9, 1.10, 0.05
SPAN, FOOT, LW, LT, PITCH = 6.0, 0.67, 0.25, 0.20, 0.65
body = cq.Workplane('XY').box(BODY_X, BODY_Y, H - STANDOFF, centered=(True, True, False)).translate((0, 0, STANDOFF)).edges('|Z').fillet(0.1)
body = body.faces('>Z').workplane().center(-BODY_X / 2 + 0.55, BODY_Y / 2 - 0.55).hole(0.4, 0.05)   # pin 1 mark
metal = cq.Workplane('XY').box(2.65, 4.0, STANDOFF + 0.01, centered=(True, True, False))              # exposed pad (VBAT)
for i in range(7):
    y = 1.95 - i * PITCH
    for side in (-1, 1):
        yy = y if side < 0 else -y
        shoulder = cq.Workplane('XY').box(0.45, LW, LT, centered=(False, True, False)).translate((BODY_X / 2 - 0.05, 0, 0.45))
        drop = cq.Workplane('XY').box(LT, LW, 0.65, centered=(False, True, False)).translate((BODY_X / 2 + 0.35, 0, 0))
        foot = cq.Workplane('XY').box(SPAN / 2 - (BODY_X / 2 + 0.35), LW, LT, centered=(False, True, False)).translate((BODY_X / 2 + 0.35, 0, 0))
        lead = shoulder.union(drop).union(foot)
        if side < 0: lead = lead.mirror('YZ')
        metal = metal.union(lead.translate((0, yy, 0)))
asm = cq.Assembly()
asm.add(body, name='body', color=cq.Color(0.12, 0.12, 0.12))
asm.add(metal, name='leads', color=cq.Color(0.82, 0.82, 0.84))
asm.save('3d/Infineon_PG-TSDSO-14-22.step')
print('ok')
