"""pod_a132.py: the Audi B8 vent gauge pod for the AMOLED 1.32: the 52 mm
ring replaced by a slim tube just round the display, sunk so its lowest
point (driver's side) is level with the louvers' fronts; the louvers run on
into the tube; USB-C to the right through a slot in the tube wall (no louver
in front of it); a rear spider with three M2 bosses on the display's
standoffs. Writes pod_a132.stl, disp_placed.stl."""
import sys, numpy as np, trimesh, manifold3d as mf

D = sys.argv[1]
pod = trimesh.load(f"{D}/pod.stl")
trimesh.repair.fix_winding(pod); trimesh.repair.fix_normals(pod)
disp = trimesh.load(f"{D}/disp.stl")

k = 0.1497                      # rim plane z = 0.1497 x + c: 8.5 deg to the driver
W = np.array([-k, 0, 1]); W /= np.linalg.norm(W)
U = np.array([1, 0, k]); U /= np.linalg.norm(U)
V = np.cross(W, U)
z0 = (k * -0.6 + 16.559) / (1 + k * k)
C = np.array([-0.6 - k * z0, 0.1, z0])        # the original ring's axis

OLD_OD = 28.9
TUBE_IN = 19.15                 # the display's widest (r 18.99 at the back) + play
TUBE_OD = 21.3
LOUVER_Z = -1.69                # louvers' front edges
C = C + W * ((LOUVER_Z + TUBE_OD * U[2] - C[2]) / W[2])
M = np.eye(4); M[:3, 0] = U; M[:3, 1] = V; M[:3, 2] = W; M[:3, 3] = C

SEG = 180
def cyl(r, w0, w1, u=0.0, v=0.0, seg=SEG):
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

GLASS = 0.5                     # glass behind the tube's front
DM = np.eye(4); DM[:3, :3] = [[0, 0, 1], [1, 0, 0], [0, 1, 0]]; DM[2, 3] = -1.3 - GLASS
disp_l = disp.copy(); disp_l.apply_transform(DM)

SPIDER = (-15.3, -12.3)
NECK = (-12.3, -10.5)
BOSSES = [(0.0, 15.87), (11.04, -10.8), (-11.04, -10.8)]

tube = cyl(TUBE_OD, SPIDER[0], 0) - cyl(TUBE_IN, SPIDER[0] - 1, 1)
# keys: the USB-C receptacle (r 19.25) and the glass's flex tab (r 19.31)
tube -= box(16.5, 19.7, -5.4, 5.4, SPIDER[0] - 1, 1)
tube -= box(-19.7, -16.5, -5.6, 5.6, SPIDER[0] - 1, 1)
spider = mf.Manifold()
for u, v in BOSSES:
    spider += cyl(3.6, *SPIDER, u, v, 48) + cyl(1.9, *NECK, u, v, 48)
    a = np.arctan2(v, u)
    L = TUBE_IN + 0.8 - np.hypot(u, v)
    spider += box(0, L, -2.6, 2.6, *SPIDER).rotate((0, 0, np.degrees(a))).translate((u, v, 0))
holes = mf.Manifold()
for u, v in BOSSES:
    holes += cyl(1.15, SPIDER[0] - 1, NECK[1] + 1, u, v, 32)
    holes += cyl(2.2, SPIDER[0] - 1, SPIDER[0] + 1.2, u, v, 32)
# USB-C: receptacle at w -10.3..-6.1, v +-4.8: a slot through the tube wall,
# open to the back, for the plug (straight or angled)
usb = box(16.0, TUBE_OD + 3, -6.6, 6.6, SPIDER[0] - 5, -3.6)
adapter = tube + spider - holes - usb

# louvers: the old ring out, the slats carried on to the tube
podm = to_mf(pod) - place(cyl(OLD_OD - 0.1, -80, 80))
SLATS = [(-20.66, -17.66), (-11.06, -8.11), (-1.56, 1.44), (7.97, 10.99), (17.47, 20.54)]
ZB, ZT, RS = -11.69, -1.69, 1.5
ext = mf.Manifold()
for i, (y0, y1) in enumerate(SLATS):
    for x0, x1 in ((-40, 0), (0, 40)):
        if i == 2 and x0 == 0:
            continue            # the right middle one stops short: the USB plug's way
        b = mf.Manifold.cube((x1 - x0, y1 - y0, ZT - RS - ZB)).translate((x0, y0, ZB))
        r = mf.Manifold.cylinder(x1 - x0, (y1 - y0) / 2, (y1 - y0) / 2, 32) \
            .rotate((0, 90, 0)).translate((x0, (y0 + y1) / 2, ZT - RS))
        ext += b + r
ring_zone = place(cyl(OLD_OD + 0.4, -80, 80)) - place(cyl(TUBE_OD - 0.3, -80, 80))
ext = ext ^ ring_zone
# the right middle slat, cut short, held by a bar along the slats' back
# edges to its neighbours
ext += mf.Manifold.cube((2.0, 18.2, 2.5)).translate((30.0, -9.1, ZB))
ext -= place(usb)
out = place(adapter) + podm + ext
res = to_tm(out)
res.export(f"{D}/pod_a132.stl")
print("watertight", res.is_watertight, "parts", [round(p.volume(), 1) for p in out.decompose()],
      "bounds", np.round(res.bounds, 2).tolist())
dl = disp_l.copy(); dl.apply_transform(M)
dl.export(f"{D}/disp_placed.stl")
