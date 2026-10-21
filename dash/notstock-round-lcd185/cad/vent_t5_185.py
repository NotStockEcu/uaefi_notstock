"""vent_t5_185.py DIR: the VW T5 vent gauge holder (52 mm ring, a 3MF
exported to DIR/holder.stl) made into a housing for the Waveshare
ESP32-S3-LCD-1.85 with the CAN 1.85 board (../../notstock-can185) behind it.

The ring goes; in its place a tube round the display, its face in the
louvers' front plane (flush, nothing standing out), no lip: the display
goes in from the front, past whatever stands out at its edge, onto three
sleeves on ribs from the tube's wall, its glass then flush with the face.
The CAN board goes in from the back onto the same sleeves. The stack behind it is held by three M2 x 12 screws through the display's
own three standoffs, their heads down in the lid's bosses on the board:

    boss floor (1.5) | CAN board (1.6) | sleeve (5.9) | standoff (3 of 4 mm)

The lid plugs into the tube's back end and sits on it (crush ribs, no
screws into the tube). It has a window for the CAN board's 4-pin JST XH (J1), the only
thing that shows at the back.

DIR/disp185.stl: Waveshare's STEP of the display as STL (glass front at
z = -0.5, standoffs ending at z = -9.0, x right and y up seen from the
front). Writes DIR/vent185.stl, lid185.stl, fit185.txt, and
tube185.stl: the same tube alone, no vent round it, to put the gauge
anywhere (the same lid).
UP = -1 turns the display (and the lid's window) half round in the vent.

The speaker (the display's, on its 2-pin socket) is glued inside the lid,
over sound holes, in the room between the lid and the CAN board: SPK_T is
how thick it is, SPK_L x SPK_W its outline, an oval (default 5 mm, 30 x 20;
SPK_W = SPK_L for a round one), lying across with its wire lug at one end.
Where it goes in the lid is searched for: the most room to the bosses, J1
and the lid's skirt. The ring that marks its place is open at the lug.
The lid's bosses grow with it; the screws go in
from the lid down a bore in each boss to its 1.5 mm floor on the board,
so M2 x 12 does whatever the speaker."""
import sys
import numpy as np
import trimesh
import manifold3d as mf

D = sys.argv[1]
UP = int(sys.argv[2]) if len(sys.argv) > 2 else 1
SPK_T = float(sys.argv[3]) if len(sys.argv) > 3 else 5.0
SPK_L = float(sys.argv[4]) if len(sys.argv) > 4 else 30.0
SPK_W = float(sys.argv[5]) if len(sys.argv) > 5 else min(20.0, SPK_L)
# the display turned to the driver: YAW degrees to the right (seen from the
# seats; the gauge in the left vent by the door), PITCH degrees down
YAW = float(sys.argv[6]) if len(sys.argv) > 6 else 15.0
PITCH = float(sys.argv[7]) if len(sys.argv) > 7 else 10.0
# and moved out along its axis by PUSH mm; the louvers round it cut back in
# a cone FLARE degrees wide of the axis, so they shade nothing from the side
PUSH = float(sys.argv[8]) if len(sys.argv) > 8 else 6.0
FLARE = 40.0
CORNER_R = 5.0               # the side plates' corners, rounded as the dash's opening

holder = trimesh.load(f"{D}/holder.stl")
trimesh.repair.fix_winding(holder)
trimesh.repair.fix_normals(holder)
disp = trimesh.load(f"{D}/disp185.stl")

# the old ring: axis fitted from sections (inner r 26.3, the wall to ~28.8)
C0 = np.array([48.0, 25.76, 0.0])
AX = np.array([0.0, 2.06, 15.0]); AX /= np.linalg.norm(AX)
RING_CUT = 33.5             # all of it: its back is wider than its front
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
M0 = np.eye(4); M0[:3, 0] = U; M0[:3, 1] = V; M0[:3, 2] = W; M0[:3, 3] = F
ya, pa = np.radians(YAW) * UP, np.radians(PITCH) * UP
RY = np.array([[np.cos(ya), 0, np.sin(ya), 0], [0, 1, 0, 0], [-np.sin(ya), 0, np.cos(ya), 0], [0, 0, 0, 1]])
RX = np.array([[1, 0, 0, 0], [0, np.cos(pa), -np.sin(pa), 0], [0, np.sin(pa), np.cos(pa), 0], [0, 0, 0, 1]])
TP = np.eye(4); TP[2, 3] = PUSH
M = M0 @ RY @ RX @ TP         # the display's frame, turned about its face's middle

# along the axis (w, 0 = the front plane, + forward)
GL = 0.3                     # glass front behind the face
DZ = 0.5 - GL                # display model z -> w
W_STAND = -9.0 + DZ          # the standoffs' ends
SLEEVE = 5.9
BOARD = 1.6
# over the CAN board's parts (L1 2.6 mm the tallest), and the speaker
GAP = 3.0 + SPK_T + 0.4
LID = 2.0
FLOOR = 1.5                  # the bosses' floor on the board, under the heads
BOSS_R, BORE_R = 3.3, 2.15   # the bosses, and their bore for an M2 head
W_BFRONT = W_STAND - SLEEVE
W_BTOP = W_BFRONT - BOARD
W_LID = W_BTOP - GAP         # the lid's inner face
R_IN, R_OUT = 28.0, 30.4     # the display's widest: its glass's flex tail, r 27.4

# the display's standoffs (model, seen from the front) and the CAN board's
# J1 body (+0.4) in the same coordinates: the board seen from the back is
# KiCad's top view, so (x, y) = (-x_kicad, -y_kicad)
HOLES = [(15.73, 14.10), (15.73, -14.90), (-21.27, 0.0)]
J1_WIN = (-8.2 - 0.4, 4.2 + 0.4, 12.6 - 0.4, 18.35 + 0.4)   # rev 7, XH
SPK_RING = 1.2               # the ring round the speaker, out from its outline
SPK_TAB = 3.0                # the lug at one end the wires leave from


def spk_room(at, ang):
    """the least room (mm) from the speaker's ring to a boss, J1 or the skirt;
    the oval as a segment (its middle line) and a radius"""
    d = np.array([np.cos(ang), np.sin(ang)]) * (SPK_L - SPK_W) / 2
    a, b = np.array(at) - d, np.array(at) + d
    rad = SPK_W / 2 + SPK_RING

    def seg(p):
        p = np.asarray(p, float)
        t = np.clip(np.dot(p - a, b - a) / max(np.dot(b - a, b - a), 1e-9), 0, 1)
        return np.linalg.norm(p - (a + t * (b - a)), axis=-1)
    room = [seg(h) - rad - BOSS_R for h in HOLES]
    u0, u1, v0, v1 = J1_WIN
    edge = [(u, v) for u in np.linspace(u0, u1, 25) for v in (v0, v1)]
    edge += [(u, v) for v in np.linspace(v0, v1, 9) for u in (u0, u1)]
    room.append(min(seg(e) for e in edge) - rad)
    far = max(np.linalg.norm(a), np.linalg.norm(b)) + rad
    room.append(R_IN - 1.4 - far)
    tab = np.array(at) + (b - a) / max(np.linalg.norm(b - a), 1e-9) * (SPK_L / 2 + SPK_TAB)
    room += [np.linalg.norm(tab - h) - BOSS_R - 1.0 for h in HOLES]
    room.append(R_IN - 1.4 - 1.0 - np.linalg.norm(tab))
    return min(room)


SPK_AT, SPK_ANG = max((((u, v), np.radians(k)) for u in np.arange(-6, 6.1, 0.5)
                       for v in np.arange(-10, 4.1, 0.5) for k in (0, 180)),
                      key=lambda c: spk_room(*c))


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


def place0(x):                # in the louvers' frame
    return x.transform(M0[:3, :].tolist())


# the tube, from the lid on its end to the face
tube = cyl(R_OUT, W_LID, 0.5, seg=180) - cyl(R_IN, W_LID - 1, 1, seg=180)
# the sleeves between the CAN board and the display's standoffs, each on a
# rib from the wall (the board's front has no parts)
SLEEVE_R, RIB = 2.2, 2.4
for u, v in HOLES:
    a = np.degrees(np.arctan2(v, u))
    r = np.hypot(u, v)
    tube += cyl(SLEEVE_R, W_BFRONT, W_STAND, u, v, 48)
    tube += box(r, R_IN + 0.5, -RIB / 2, RIB / 2, W_BFRONT, W_STAND).rotate((0, 0, a))
    tube -= cyl(1.2, W_BFRONT - 1, W_STAND + 1, u, v, 24)

pod = to_mf(holder)
pod -= mf.Manifold.cylinder(160, RING_CUT, RING_CUT, 180).translate(
    (0, 0, -80)).transform(MR[:3, :].tolist())                  # the ring out
# the louvers between x 12 and 84 (the ring and what is near it) made
# again from their own section at x 12 and 84, running on into the tube,
# so they hold it whichever way it is turned
TO_X = [[0, 1, 0, 0], [0, 0, 1, 0], [1, 0, 0, 0]]             # (y, z, x): x up
FROM_X = [[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0]]
side = to_mf(holder).transform(TO_X)
pod -= box(12, 84, -100, 200, -100, 100)
for x0, x1, xs in ((12, 48, 12.5), (48, 84, 83.5)):
    prof = side.slice(xs)
    pod += prof.extrude(x1 - x0).translate((0, 0, x0)).transform(FROM_X)
pod -= place(cyl(R_IN, -60, 60, seg=180))
pod -= place(cyl(R_OUT + 0.4, -60, W_LID, seg=180))           # room for the lid
pod -= place(mf.Manifold.cylinder(80, R_OUT, R_OUT + 80 * np.tan(np.radians(FLARE)), 180)
             )                                                 # nothing in front of it
pod -= place0(box(-200, 200, -200, 200, 0, 100))                # the louvers' front
out = pod + place(tube - box(-50, 50, -50, 50, 0, 10))         # the tube's own face
# the side plates' corners rounded in their own plane (as seen from the
# side): their outline at x 1, opened by CORNER_R, what that takes off
# (the corners only) cut away across the whole width
plate = side.slice(1.0).hull()                                 # its outline only
rounded = plate.offset(-CORNER_R, mf.JoinType.Round, 2.0, 48).offset(
    CORNER_R, mf.JoinType.Round, 2.0, 48)
corners = plate - rounded
out -= corners.extrude(120).translate((0, 0, -10)).transform(FROM_X)
out = max(out.decompose(), key=lambda p: p.volume())       # no slivers left
res = to_tm(out)
res.export(f"{D}/vent185.stl")

spk_d = np.array([np.cos(SPK_ANG), np.sin(SPK_ANG)]) * (SPK_L - SPK_W) / 2
spk_u = spk_d / max(np.linalg.norm(spk_d), 1e-9) if SPK_L > SPK_W else np.array([np.cos(SPK_ANG), np.sin(SPK_ANG)])


def oval(off):
    c = [mf.CrossSection.circle(SPK_W / 2 + off, 64).translate(tuple(np.array(SPK_AT) + k * spk_d))
         for k in (-1, 1)]
    return (c[0] + c[1]).hull()


def make_lid(r_out, r_in):
        # the lid: plate, a skirt into the tube with six crush ribs, bosses down to
    # the board at the screws, the J1 window
    lid = cyl(r_out, W_LID - LID, W_LID, seg=180)
    lid += cyl(r_in - 0.15, W_LID, W_LID + 3.0, seg=180) - cyl(r_in - 1.4, W_LID - 1, W_LID + 4, seg=180)
    for k in range(6):
        a = np.radians(30 + 60 * k)
        lid += cyl(0.5, W_LID, W_LID + 3.0, (r_in - 0.35) * np.cos(a), (r_in - 0.35) * np.sin(a), 16)
    for u, v in HOLES:
        lid += cyl(BOSS_R, W_LID - 0.5, W_BTOP, u, v, 48)
        lid -= cyl(1.15, W_LID - LID - 1, W_BTOP + 1, u, v, 24)
        lid -= cyl(BORE_R, W_LID - LID - 1, W_BTOP - FLOOR, u, v, 32)    # the head's way
    # sound holes under the speaker, and a ring to find its place by
    for i in range(-9, 10):
        for j in range(-8, 9):
            p = np.array([SPK_AT[0] + 3.2 * (i + 0.5 * (j % 2)), SPK_AT[1] + 2.77 * j])
            q = p - np.array(SPK_AT)
            t = np.clip(np.dot(q, spk_d) / max(np.dot(spk_d, spk_d), 1e-9), -1, 1)
            if np.linalg.norm(q - t * spk_d) <= SPK_W / 2 - 3.5:
                lid -= cyl(0.8, W_LID - LID - 1, W_LID + 1, p[0], p[1], 16)
    ring = (oval(SPK_RING) - oval(0.3)).extrude(0.8).translate((0, 0, W_LID))
    gap = np.array(SPK_AT) + spk_u * (SPK_L / 2)               # open at the lug
    ring -= cyl(3.5, W_LID - 1, W_LID + 2, gap[0], gap[1], 32)
    lid += ring
    lid -= box(J1_WIN[0], J1_WIN[1], J1_WIN[2], J1_WIN[3], W_LID - LID - 1, W_LID + 5)
    return lid


lid = make_lid(R_OUT, r_in=R_IN)
lid_placed = place(lid)
lt = to_tm(lid)                     # printed outside face down
lt.apply_transform(np.diag([1, -1, -1, 1]))
lt.apply_translation(-lt.bounds[0])
lt.export(f"{D}/lid185.stl")

# the tube alone, no vent round it: the gauge anywhere
plain = tube - box(-50, 50, -50, 50, 0, 10)
pt = to_tm(plain)                   # printed face down
pt.apply_transform(np.diag([1, -1, -1, 1]))
pt.apply_translation([0, 0, -pt.bounds[0][2]])
pt.export(f"{D}/tube185.stl")

# checks: the display where it goes, inside the vent and the lid?
dm = disp.copy()
dm.apply_translation([0, 0, DZ])
dm.apply_transform(M)
pts = np.vstack([trimesh.sample.sample_surface(dm, 40000)[0], dm.vertices])
inside = res.contains(pts[::4] + M[:3, 2] * 0.05)    # its standoffs sit on the sleeves
# and on its way in from the front
stuck = [k for k in np.arange(0.5, 32.5, 0.5) if res.contains(pts[::4] + M[:3, 2] * k).any()]
to_tm(lid_placed).export(f"{D}/lid185_placed.stl")
dm.export(f"{D}/disp185_placed.stl")
touch = (out ^ lid_placed).volume()
with open(f"{D}/fit185.txt", "w") as f:
    f.write(f"vent watertight {res.is_watertight}, parts {[round(p.volume(), 1) for p in out.decompose()]}\n")
    f.write(f"tube alone: {pt.extents.round(1)} mm, watertight {pt.is_watertight}\n")
    f.write(f"lid watertight {lt.is_watertight}, sleeves {SLEEVE} mm in the tube\n")
    f.write(f"tube depth {-W_LID:.1f} mm, bore {2 * R_IN:.0f} mm, lid's back at {-(W_LID - LID):.1f} mm behind the face\n")
    f.write(f"display points inside the vent: {int(inside.sum())} of {len(inside)}\n")
    f.write(f"display in from the front: {'stuck at ' + str(stuck) + ' mm' if stuck else 'clear'}\n")
    f.write(f"speaker {SPK_L:g} x {SPK_W:g} x {SPK_T:g} at ({SPK_AT[0]:g}, {SPK_AT[1]:g}), "
            f"turned {np.degrees(SPK_ANG):.0f} deg; its ring {spk_room(SPK_AT, SPK_ANG):.1f} mm "
            f"from the nearest boss, J1 or the skirt\n")
    f.write(f"lid in the vent: {touch:.2f} mm3 (the crush ribs)\n")
    grip = FLOOR + BOARD + SLEEVE
    screw = min(L for L in (12, 14, 16, 18, 20, 22, 25) if L - grip >= 2.5)
    f.write(f"speaker room {SPK_T} mm (boss {GAP:.1f}), screw: boss floor {FLOOR} + board {BOARD} + sleeve {SLEEVE} = "
            f"{grip:.1f}; M2 x {screw} goes {screw - grip:.1f} into the 4 mm standoff\n")
print(open(f"{D}/fit185.txt").read())
