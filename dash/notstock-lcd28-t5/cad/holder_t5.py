"""holder_t5.py DIR [T]: the VW T5 vent gauge holder (52 mm ring, a 3MF
exported to DIR/holder.stl) turned into a frame for the Waveshare
RP2350-Touch-LCD-2.8 in landscape, its face flush with the louvers' front
edges: one flat face, nothing standing out.

The ring goes; in its place a rectangular housing: a 1.6 mm lip over the
glass's black border at the front, the module put in from the back, held by
a rear plate (DIR/plate.stl) with four pads on the board's corners and four
M2 x 8 screws into bosses at the housing's corners. Both short sides have a
notch behind the board for the cables (USB-C: a 90 degree plug, the UART
connector's CAN wires); the louver stubs there are cut back to let them by.

T: the module's thickness, glass front to the back of the board (no
connectors), mm; default 7.0. Measure it: the housing's depth follows.
Writes DIR/holder28.stl, DIR/plate.stl, DIR/fit_check.txt.

Glass 73.06 x 50.54, active 57.60 x 43.20, board 69.00 x 49.90 (Waveshare's
drawing)."""
import sys
import numpy as np
import trimesh
import manifold3d as mf

D = sys.argv[1]
T = float(sys.argv[2]) if len(sys.argv) > 2 else 7.0

holder = trimesh.load(f"{D}/holder.stl")
trimesh.repair.fix_winding(holder)
trimesh.repair.fix_normals(holder)

# the ring's axis (fitted from sections), to take the ring out
C0 = np.array([48.0, 25.76, 0.0])
AX = np.array([0.0, 27.82 - 25.76, 15.0]); AX /= np.linalg.norm(AX)
RING_OUT = 34.6                   # the ring's outer radius, and a little
MR = np.eye(4); MR[:3, 0] = [1, 0, 0]; MR[:3, 1] = np.cross(AX, [1, 0, 0])
MR[:3, 2] = AX; MR[:3, 3] = C0
# the louvers' front edges lie in one plane, z = 26.06 - 0.2424 y (fitted,
# +-0.5 mm): the display's face goes into it, flush, and nothing stands out
# of it (what did is cut off)
FRONT_Z0, FRONT_DZDY = 26.06, -0.2424
Y_MID = 29.7                      # the holder's middle, top to bottom
W = np.array([0.0, -FRONT_DZDY, 1.0]); W /= np.linalg.norm(W)
U = np.array([1.0, 0, 0])
V = np.cross(W, U)
F = np.array([48.0, Y_MID, FRONT_Z0 + FRONT_DZDY * Y_MID])
M = np.eye(4); M[:3, 0] = U; M[:3, 1] = V; M[:3, 2] = W; M[:3, 3] = F

GLASS = (73.06, 50.54)
PCB = (69.0, 49.9)
PLAY = 0.6
POCKET = (GLASS[0] + PLAY, GLASS[1] + PLAY)
R_POCKET = 3.0
WALL = 2.2
LIP = 1.6                         # over the glass's border, 1.5 mm of it
WINDOW = (GLASS[0] - 3.0, GLASS[1] - 3.0)
PAD_H = 7.0                       # plate pads: room for the board's connectors
PLATE = 2.0
W_PCB = -(LIP + T)                # the board's back
W_BACK = W_PCB - PAD_H            # the housing's back face, the plate on it
BOSS_R, HOLE_R, HOLE_D = 3.2, 0.85, 9.0
BOSS_C = (POCKET[0] / 2 + 2.5, POCKET[1] / 2 + 2.5)
NOTCH_V = 15.0                    # half height of the cable notches
CORNERS = [(sx * BOSS_C[0], sy * BOSS_C[1]) for sx in (-1, 1) for sy in (-1, 1)]


def rbox(w, h, r, w0, w1):
    cs = mf.CrossSection.square((w - 2 * r, h - 2 * r), True).offset(
        r, mf.JoinType.Round, 2.0, 48)
    return cs.extrude(w1 - w0).translate((0, 0, w0))


def box(u0, u1, v0, v1, w0, w1):
    return mf.Manifold.cube((u1 - u0, v1 - v0, w1 - w0)).translate((u0, v0, w0))


def cyl(r, w0, w1, u=0.0, v=0.0, seg=48):
    return mf.Manifold.cylinder(w1 - w0, r, r, seg).translate((u, v, w0))


def to_mf(m):
    return mf.Manifold(mf.Mesh(vert_properties=np.asarray(m.vertices, np.float32),
                               tri_verts=np.asarray(m.faces, np.uint32)))


def to_tm(x):
    g = x.to_mesh()
    return trimesh.Trimesh(np.asarray(g.vert_properties)[:, :3], np.asarray(g.tri_verts))


def place(x):
    return x.transform(M[:3, :].tolist())


OUT = (POCKET[0] + 2 * WALL, POCKET[1] + 2 * WALL)
housing = rbox(OUT[0], OUT[1], R_POCKET + WALL, W_BACK, 0)
for u, v in CORNERS:
    housing += cyl(BOSS_R, W_BACK, W_BACK + 10, u, v)
housing -= rbox(POCKET[0], POCKET[1], R_POCKET, W_BACK - 1, -LIP)
housing -= rbox(WINDOW[0], WINDOW[1], 2.0, -LIP - 1, 1)
for u, v in CORNERS:
    housing -= cyl(HOLE_R, W_BACK - 1, W_BACK + HOLE_D, u, v, 16)
# cable notches through both short walls, behind the board's front
notch_w1 = W_PCB + 3.0
notches = mf.Manifold()
for s in (-1, 1):
    u0 = s * (POCKET[0] / 2 - 1)
    u1 = s * (OUT[0] / 2 + 12)
    notches += box(min(u0, u1), max(u0, u1), -NOTCH_V, NOTCH_V, W_BACK - 30, notch_w1)
housing -= notches

pod = to_mf(holder)
pod -= mf.Manifold.cylinder(160, RING_OUT, RING_OUT, 128).translate(
    (0, 0, -80)).transform(MR[:3, :].tolist())                  # the ring out
pod -= place(rbox(POCKET[0], POCKET[1], R_POCKET, W_BACK - 40, 40))
pod -= place(notches)
out = pod + place(housing)
out -= place(box(-200, 200, -200, 200, 0, 100))                 # flat: the front
res = to_tm(out)
res.export(f"{D}/holder28.stl")

# the rear plate: frame, pads on the board's corners, screw holes
plate = rbox(OUT[0], OUT[1], R_POCKET + WALL, -PLATE, 0)
for u, v in CORNERS:
    plate += cyl(BOSS_R, -PLATE, 0, u, v)
plate -= rbox(PCB[0] - 6, PCB[1] - 6, 3, -PLATE - 1, 1)
for sx in (-1, 1):
    for sy in (-1, 1):
        cu, cv = sx * (PCB[0] / 2 - 3.5), sy * (PCB[1] / 2 - 3.5)
        plate += box(cu - 3.5, cu + 3.5, cv - 3.5, cv + 3.5, -PLATE, PAD_H)
for u, v in CORNERS:
    plate -= cyl(1.15, -PLATE - 1, PAD_H + 1, u, v, 16)
# printed flat: pads up
pl = to_tm(plate)
pl.apply_translation(-pl.bounds[0])
pl.export(f"{D}/plate.stl")

parts = [round(p.volume(), 1) for p in out.decompose()]
with open(f"{D}/fit_check.txt", "w") as f:
    f.write(f"T {T} mm, housing depth {-W_BACK:.1f} mm\n")
    f.write(f"holder watertight {res.is_watertight}, parts {parts}\n")
    f.write(f"bounds {np.round(res.bounds, 2).tolist()}\n")
print(open(f"{D}/fit_check.txt").read())
