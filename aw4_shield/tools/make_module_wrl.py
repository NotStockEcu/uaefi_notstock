# 3D model (VRML) modulu Waveshare RP2350-Touch-LCD-2 pro rendery v KiCadu – displej ukazuje obrazovku firmwaru AW4
# (stupeň GEAR, MANUAL, LOCKUP, S1-S3). Rozměry z výkresu Waveshare; displej je jen ilustrační.
# Souřadnice v mm, počátek = střed desky modulu, +Y = strana s USB-C, Z od horní plochy základní desky.
# KiCad bere 1 jednotku VRML = 2,54 mm.
import sys
GEAR = int(sys.argv[2]) if len(sys.argv) > 2 else 3
LOCK = True
OUT = sys.argv[1]
U = 1 / 2.54
shapes = []
def mat(c, em=0.0, shin=0.3, spec=0.2, transp=0.0):
    r, g, b = c
    return (f"Material {{ diffuseColor {r:.3f} {g:.3f} {b:.3f} emissiveColor {r*em:.3f} {g*em:.3f} {b*em:.3f} "
            f"specularColor {spec:.3f} {spec:.3f} {spec:.3f} shininess {shin:.3f} transparency {transp:.3f} }}")
def faces(pts, idx, m):
    p = ", ".join(f"{x*U:.5f} {y*U:.5f} {z*U:.5f}" for x, y, z in pts)
    i = ", ".join(" ".join(str(k) for k in f) + " -1" for f in idx)
    shapes.append(f"Shape {{ appearance Appearance {{ material {m} }} geometry IndexedFaceSet {{ coord Coordinate {{ point [ {p} ] }} coordIndex [ {i} ] solid FALSE }} }}")
def box(x0, y0, z0, x1, y1, z1, m):
    P = [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
    faces(P, [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)], m)
def poly(pts2, z, m):   # plochý mnohoúhelník
    faces([(x, y, z) for x, y in pts2], [list(range(len(pts2)))[::-1]], m)   # proti směru hodin při pohledu shora

ZS = 8.5            # výška dutinkové lišty
ZP = ZS + 2.5       # spodek desky modulu (plast kolíkové lišty modulu)
ZT = ZP + 1.6       # horní plocha desky modulu
ZG = ZT + 3.6       # povrch skla
BLACK = mat((0.02,0.02,0.025), shin=0.6, spec=0.3)
# kolíkové lišty modulu (plast) + desky modulu
for x in (-15.24, 15.24):
    box(x-1.27, -17.78, ZS, x+1.27, 17.78, ZP, mat((0.05,0.05,0.05)))
box(-17.5, -24.1, ZP, 17.5, 24.1, ZT, mat((0.03,0.05,0.12), shin=0.4))
# USB-C (na spodní straně u horní hrany) a tlačítka BOOT/RESET
box(-4.5, 24.1-7.3, ZP-3.2, 4.5, 24.1+0.6, ZP, mat((0.75,0.75,0.78), shin=0.8, spec=0.8))
for x in (-10, 10): box(x-1.5, 18.5, ZP-1.5, x+1.5, 21.5, ZP, mat((0.85,0.85,0.85)))
# displej: rámeček + sklo 37,1 x 58,8, přesah 10,6 mm na straně proti USB
GY1 = 24.1; GY0 = GY1 - 58.8
box(-18.55, GY0, ZT, 18.55, GY1, ZG-0.7, mat((0.08,0.08,0.09), shin=0.3))
box(-18.55, GY0, ZG-0.7, 18.55, GY1, ZG, mat((0.01,0.01,0.012), shin=0.9, spec=0.6))
# aktivní plocha 30,48 x 40,64 (240 x 320 px)
AW, AH = 30.48, 40.64; CY = (GY0 + GY1) / 2 + 1.5
PX = AW / 240
AX0, AY1 = -AW/2, CY + AH/2
ZA = ZG + 0.02
def P(px, py): return (AX0 + px*PX, AY1 - py*PX)
poly([P(0,0), P(240,0), P(240,320), P(0,320)], ZA, mat((0,0,0), shin=0.9, spec=0.5))
ZC = ZA + 0.02
def rect(x, y, w, h, m): poly([P(x,y), P(x+w,y), P(x+w,y+h), P(x,y+h)], ZC, m)
# sedmisegmentovka podle firmwaru: sevenSeg(18, 34, 204, 248, th 34)
ON = mat((0.0, 1.0, 0.0), em=1.0, shin=0.1, spec=0.0)
OFF = mat((0.0, 0.11, 0.0), em=0.4, shin=0.1, spec=0.0)
x, y, w, h, th = 18, 34, 204, 248, 34
g = th // 5 + 2; hl = w - th - 2*g; vl = (h - 3*th)//2 + th - 2*g
xl, xr, xh = x, x + w - th, x + th//2 + g
ya, yg, yd = y, y + (h - th)//2, y + h - th
yv1, yv2 = y + th//2 + g, yg + th//2 + g
h2 = th // 2
def segH(sx, sy, ln, m): poly([P(sx, sy+h2), P(sx+h2, sy), P(sx+ln-h2, sy), P(sx+ln, sy+h2), P(sx+ln-h2, sy+th), P(sx+h2, sy+th)], ZC, m)
def segV(sx, sy, ln, m): poly([P(sx+h2, sy), P(sx+th, sy+h2), P(sx+th, sy+ln-h2), P(sx+h2, sy+ln), P(sx, sy+ln-h2), P(sx, sy+h2)], ZC, m)
MASK = [0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F][GEAR]
segs = [(segH,xh,ya,hl),(segV,xr,yv1,vl),(segV,xr,yv2,vl),(segH,xh,yd,hl),(segV,xl,yv2,vl),(segV,xl,yv1,vl),(segH,xh,yg,hl)]
for i, (f, sx, sy, ln) in enumerate(segs): f(sx, sy, ln, ON if MASK >> i & 1 else OFF)
# texty bodovým písmem 5x7 (jako malé písmo firmwaru, výška ~22 px)
FONT = {"M":["10001","11011","10101","10101","10001","10001","10001"],"A":["01110","10001","10001","11111","10001","10001","10001"],
 "N":["10001","11001","10101","10011","10001","10001","10001"],"U":["10001","10001","10001","10001","10001","10001","01110"],
 "L":["10000","10000","10000","10000","10000","10000","11111"],"O":["01110","10001","10001","10001","10001","10001","01110"],
 "C":["01110","10001","10000","10000","10000","10001","01110"],"K":["10001","10010","10100","11000","10100","10010","10001"],
 "P":["11110","10001","10001","11110","10000","10000","10000"],"S":["01111","10000","10000","01110","00001","00001","11110"],
 "1":["00100","01100","00100","00100","00100","00100","01110"],"2":["01110","10001","00001","00010","00100","01000","11111"],
 "3":["11110","00001","00001","01110","00001","00001","11110"]}
def text(x0, y0, s, m, d=2.6):
    for k, ch in enumerate(s):
        for r, row in enumerate(FONT[ch]):
            for c, v in enumerate(row):
                if v == "1": rect(x0 + k*6*d + c*d, y0 + r*d, d - 0.35, d - 0.35, m)
def tw(s, d=2.6): return len(s)*6*d - d
GREENT = mat((0.0, 1.0, 0.0), em=1.0, spec=0.0); DK = mat((0.17,0.17,0.17), em=0.5, spec=0.0); OR = mat((1.0,0.45,0.0), em=1.0, spec=0.0)
text(4 + (150 - tw("MANUAL"))//2, 4, "MANUAL", GREENT)
text(4 + (110 - tw("LOCKUP"))//2, 294, "LOCKUP", OR if LOCK else DK)
TABLE = {1:(1,0),2:(1,1),3:(0,1),4:(0,0)}
sol = list(TABLE[GEAR]) + [LOCK]
for i, on in enumerate(sol): text(132 + i*36 + (34 - tw("S1"))//2, 294, "S" + str(i+1), GREENT if on else DK)
open(OUT, "w").write("#VRML V2.0 utf8\n# RP2350-Touch-LCD-2 (ilustrační model pro AW4 shield)\n" + "\n".join(shapes) + "\n")
print("shapes", len(shapes))
