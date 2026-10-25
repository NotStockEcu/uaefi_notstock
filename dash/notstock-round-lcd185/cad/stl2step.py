"""stl2step.py IN.stl OUT.step: a mesh as a STEP solid (its triangles
sewn, coplanar ones merged), for CAD programs that do not take STL."""
import sys
import trimesh
from OCP.gp import gp_Pnt
from OCP.BRepBuilderAPI import (BRepBuilderAPI_MakePolygon, BRepBuilderAPI_MakeFace,
                                BRepBuilderAPI_Sewing, BRepBuilderAPI_MakeSolid)
from OCP.TopoDS import TopoDS
from OCP.ShapeUpgrade import ShapeUpgrade_UnifySameDomain
from OCP.STEPControl import STEPControl_Writer, STEPControl_AsIs

m = trimesh.load(sys.argv[1])
sew = BRepBuilderAPI_Sewing(1e-4)
for tri in m.vertices[m.faces]:
    poly = BRepBuilderAPI_MakePolygon()
    for p in tri:
        poly.Add(gp_Pnt(*map(float, p)))
    poly.Close()
    face = BRepBuilderAPI_MakeFace(poly.Wire())
    if face.IsDone():
        sew.Add(face.Face())
sew.Perform()
shape = sew.SewedShape()
try:
    shape = BRepBuilderAPI_MakeSolid(TopoDS.Shell_s(shape)).Solid()
except Exception:
    pass                       # not one closed shell: write it as it is
uni = ShapeUpgrade_UnifySameDomain(shape, True, True, True)
uni.Build()
w = STEPControl_Writer()
w.Transfer(uni.Shape(), STEPControl_AsIs)
w.Write(sys.argv[2])
