"""vent_t5_185.py DIR: the VW T5 vent gauge holder (52 mm ring, a 3MF
exported to DIR/holder.stl) made into a housing for the Waveshare
ESP32-S3-LCD-1.85 with the CAN 1.85 board (../../notstock-can185) behind it.

The ring goes; in its place a tube round the display, its face in the
louvers' front plane (flush, nothing standing out). A lip round the glass
at the front hides the board's edge, so the display goes in from the back.
The stack behind it is held by three M2 x 16 screws through the display's
own three standoffs:

    lid (2.0) | boss (3.5) | CAN board (1.6) | sleeve (5.9) | standoff (3 of 4 mm)

The lid plugs into the tube's back end (crush ribs, no screws into the
tube) and has a window for the CAN board's 4-pin JST PH (J1), the only
thing that shows at the back.

DIR/disp185.stl: Waveshare's STEP of the display as STL (glass front at
z = -0.5, standoffs ending at z = -9.0, x right and y up seen from the
front). Writes DIR/vent185.stl, lid185.stl, sleeve185.stl, fit185.txt.
UP = -1 turns the display (and the lid's window) half round in the vent."""
import sys
import numpy as np
import trimesh
import manifold3d as mf

D = sys.argv[1]
UP = int(sys.argv[2]) if len(sys.argv) > 2 else 1

holder = trimesh.load(f"{D}/holder.stl")
trimesh.repair.fix_winding(holder)
trimesh.repair.fix_normals(holder)
disp = trimesh.load(f"{D}/disp185.stl")

# the old ring: axis fitted from sections (inner r 26.3, the wall to ~28.8)
C0 = np.array([48.0, 25.76, 0.0])
AX = np.array([0.0, 2.06, 15.0]); AX /= np.linalg.norm(AX)
RING_CUT = 28.9
MR = np.eye(4); MR[:3, 0] = [1, 0, 0]; MR[:3, 1] = np.cross(AX, [1, 0, 0])
MR[:3, 2] = AX; MR[:3, 3] = C0

# the louvers' front edges: z = 26.06 - 0.2424 y (as holder_t5.py); the
# display's axis is its normal, its centre where the old ring's axis meets it
FRONT_Z0, FRONT_DZDY = 26.06, -0.2424
W = np.array([0.0, -FRONT_DZDY, 1.0]); W /= np.linalg.norm(W)
U = np.array([1.0, 0, 0]) * UP
V = np.cross(W, U)
t = (FRONT_Z0 + FRONT_DZDY * C0[1] - C0[2]) / (AX[2] - FRONT_DZDY * AX[1])
F = C0 + t * AX
M = np.eye(4); M[:3, 0] = U; M[:3, 1] = V; M[:3, 2] = W; M[:3, 3] = F

# along the axis (w, 0 = the front plane, + forward)
GL = 0.3                     # glass front behind the face
DZ = 0.5 - GL                # display model z -> w
W_LIP = -3.6 + DZ + 0.1      # the lip's back: just in front of the display's board
W_STAND = -9.0 + DZ          # the standoffs' ends
SLEEVE = 5.9
BOARD = 1.6
GAP = 3.5                    # over the CAN board's parts (L1 2.6 mm the tallest)
LID = 2.0
W_BFRONT = W_STAND - SLEEVE
W_BTOP = W_BFRONT - BOARD
W_LID = W_BTOP - GAP         # the lid's inner face
W_TUBE_END = W_LID + 0.4     # the display meets the lip before the lid the tube
R_IN, R_OUT = 26.6, 29.6     # the display's widest 26.28 (its board's corners)

# the display's standoffs (model, seen from the front) and the CAN board's
# J1 body (+0.4) in the same coordinates: the board seen from the back is
# KiCad's top view, so (x, y) = (-x_kicad, -y_kicad)
HOLES = [(15.73, 14.10), (15.73, -14.90), (-21.27, 0.0)]
J1_WIN = (-7.975 - 0.4, 3.975 + 0.4, 14.75 - 0.4, 19.75 + 0.4)


def cyl(r, w0, w1, u=0.0, v=0.0, seg=96):
    return mf.Manifold.cylinder(w1 - w0, r, r, seg).translate((u, v, w0))


def box(u0, u1, v0, v1, w0, w1):
    return mf.Manifold.cube((u1 - u0, v1 - v0, w1 - w0)).translate((u0, v0, w0))


def to_mf(m):
    return mf.Manifold(mf.Mesh(vert_properties=np.asarray(m.vertices, np.float32),
                               tri_verts=np.asarray(m.faces, np.uint32)))


def to_tm(x):
    g = x.to_mesh()
    return trimesh.Trimesh(np.asarray(g.vert_properties)[:, :3], np.asarray(g.tri_verts))


def place(x):
    return x.transform(M[:3, :].tolist())


# the lip's opening: the display's outline from its glass to its board
# (glass with the flex tail at the bottom), every 0.1 mm, filled, + 0.2
outline = mf.CrossSection()
for z in np.arange(-0.55, -3.6, -0.1):
    s = disp.section(plane_origin=[0, 0, z], plane_normal=[0, 0, 1])
    if s is None:
        continue
    for e in s.discrete:          # every loop: the flex tail is one of its own
        outline += mf.CrossSection([e[:, :2].tolist()], mf.FillRule.NonZero)
hole = outline.simplify(0.02).offset(0.2, mf.JoinType.Round, 2.0, 48).simplify(0.02)

tube = cyl(R_OUT, W_TUBE_END, 0.5, seg=180) - cyl(R_IN, W_TUBE_END - 1, W_LIP, seg=180)
tube -= hole.extrude(10).translate((0, 0, W_LIP - 1))

pod = to_mf(holder)
pod -= mf.Manifold.cylinder(160, RING_CUT, RING_CUT, 180).translate(
    (0, 0, -80)).transform(MR[:3, :].tolist())                  # the ring out
pod -= place(cyl(R_IN, -60, 60, seg=180))
pod -= place(cyl(R_OUT + 0.4, -60, W_TUBE_END, seg=180))      # room for the lid
out = pod + place(tube)
out -= place(box(-200, 200, -200, 200, 0, 100))                 # flat: the front
res = to_tm(out)
res.export(f"{D}/vent185.stl")

# the lid: plate, a skirt into the tube with six crush ribs, bosses down to
# the board at the screws, the J1 window
lid = cyl(R_OUT, W_LID - LID, W_LID, seg=180)
lid += cyl(R_IN - 0.15, W_LID, W_LID + 3.0, seg=180) - cyl(R_IN - 1.4, W_LID - 1, W_LID + 4, seg=180)
for k in range(6):
    a = np.radians(30 + 60 * k)
    lid += cyl(0.5, W_LID, W_LID + 3.0, (R_IN - 0.35) * np.cos(a), (R_IN - 0.35) * np.sin(a), 16)
for u, v in HOLES:
    lid += cyl(2.3, W_LID - 0.5, W_BTOP, u, v, 48)
    lid -= cyl(1.15, W_LID - LID - 1, W_BTOP + 1, u, v, 24)
lid -= box(J1_WIN[0], J1_WIN[1], J1_WIN[2], J1_WIN[3], W_LID - LID - 1, W_LID + 5)
lid_placed = place(lid)
lt = to_tm(lid)                     # printed outside face down
lt.apply_transform(np.diag([1, -1, -1, 1]))
lt.apply_translation(-lt.bounds[0])
lt.export(f"{D}/lid185.stl")

sleeve = cyl(2.0, 0, SLEEVE, seg=48) - cyl(1.2, -1, SLEEVE + 1, seg=24)
sl = mf.Manifold()
for k in range(3):
    sl += sleeve.translate((k * 6.0, 0, 0))
to_tm(sl).export(f"{D}/sleeve185.stl")

# checks: the display where it goes, inside the vent and the lid?
dm = disp.copy()
dm.apply_translation([0, 0, DZ])
dm.apply_transform(M)
pts = np.vstack([trimesh.sample.sample_surface(dm, 40000)[0], dm.vertices])
inside = res.contains(pts[::4])
with open(f"{D}/fit185.txt", "w") as f:
    f.write(f"vent watertight {res.is_watertight}, parts {[round(p.volume(), 1) for p in out.decompose()]}\n")
    f.write(f"lid watertight {lt.is_watertight}, sleeve {SLEEVE} mm x 3\n")
    f.write(f"tube depth {-W_TUBE_END:.1f} mm, lid's back at {-(W_LID - LID):.1f} mm behind the face\n")
    f.write(f"display points inside the vent: {int(inside.sum())} of {len(inside)}\n")
    f.write(f"screw: lid {LID} + boss {GAP} + board {BOARD} + sleeve {SLEEVE} = "
            f"{LID + GAP + BOARD + SLEEVE:.1f}; M2 x 16 goes {16 - (LID + GAP + BOARD + SLEEVE):.1f} into the 4 mm standoff\n")
print(open(f"{D}/fit185.txt").read())
dm.export(f"{D}/disp185_placed.stl")
to_tm(lid_placed).export(f"{D}/lid185_placed.stl")
