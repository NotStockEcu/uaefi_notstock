import pcbnew, json, sys
sys.path.insert(0,"/work")
from sexp import parse, find, q
NL=parse(open("/work/pcb/net.net").read())
netmap={}
for n in find(find(NL,'nets')[0],'net'):
    nm=q(find(n,'name')[0][1])
    for nd in find(n,'node'): netmap[(q(find(nd,'ref')[0][1]),q(find(nd,'pin')[0][1]))]=nm
MM=pcbnew.FromMM
d=json.load(open("/work/parts.json"))
parts={p["ref"]:p for p in d["parts"]}
FPDIR="/usr/share/kicad/footprints/"
# placement: ref -> (x, y, rot)
P={}
P["J1"]=(121,107,180); P["F1"]=(129,108,0); P["Q1"]=(158,110,0); P["R1"]=(158,116,0); P["D1"]=(164,116,0)
P["D2"]=(172,108,0); P["C1"]=(181,108,0); P["C2"]=(180,116,0); P["U1"]=(195,110,0); P["C3"]=(195,116,0)
P["R2"]=(201,116,0); P["L1"]=(209,110,0); P["C4"]=(214,108,0); P["JP1"]=(224,106,0)
P["J2"]=(107,130,270)
for n,(K,D,Q,Z,Rpu,Rg,U,Ra) in enumerate([("K1","D12","Q3","D11","R19","R18","U4","R17"),("K2","D14","Q4","D13","R22","R21","U5","R20"),("K3","D16","Q5","D15","R25","R24","U6","R23")]):
    kx=128+20*n
    P[K]=(kx,128,0); P[D]=(kx,121.5,0); P[Q]=(kx-3,155,0); P[Z]=(kx-6,161,0); P[Rpu]=(kx,161,0); P[Rg]=(kx+5,161,0)
    P[U]=(kx-4,166,0); P[Ra]=(kx,176,0)
P["U3"]=(182,128,0); P["R13"]=(183,137,0); P["R14"]=(188,137,0); P["R15"]=(193,137,0)
P["Q2"]=(182,144,0); P["D9"]=(192,144,0); P["R16"]=(183,151,0); P["D10"]=(190,151,0)
P["U2"]=(202,145,90); P["J6"]=(206,152,90)
P["J3"]=(190,194.5,0); P["J5"]=(224,196,0)
for i,(R1k,R10k,C,D) in enumerate([("R3","R4","C5","D3"),("R5","R6","C6","D4"),("R7","R8","C7","D5"),("R9","R10","C8","D6")]):
    x=191+7*i
    P[R1k]=(x,168,90); P[R10k]=(x+3.2,172.5,90); P[C]=(x,177,90); P[D]=(x+3.2,181.5,90)
P["L2"]=(230,178,0); P["C9"]=(229,170,0); P["C10"]=(236,178,0); P["R11"]=(230,183,0); P["R12"]=(236,183,0); P["D7"]=(230,187,0); P["D8"]=(236,187,0)
missing=[r for r in parts if r not in P]; assert not missing, missing

b=pcbnew.CreateEmptyBoard()
nets={}
def net(n):
    if n not in nets:
        ni=pcbnew.NETINFO_ITEM(b,n); b.Add(ni); nets[n]=ni
    return nets[n]
for ref,p in parts.items():
    lib,name=p["fp"].split(":")
    fp=pcbnew.FootprintLoad(FPDIR+lib+".pretty",name)
    fp.SetFPID(pcbnew.LIB_ID(lib,name))
    fp.SetReference(ref); fp.SetValue(p["val"])
    fp.SetPath(pcbnew.KIID_PATH("/"+p["uuid"]))
    fp.SetSheetname("/"); fp.SetSheetfile("aw4_shield.kicad_sch")
    if p["note"]:
        fp.SetLibDescription(p["note"])
        try: fp.SetField("Description", p["note"])
        except Exception as e: print("SetField failed", e)
    x,y,r=P[ref]
    fp.SetPosition(pcbnew.VECTOR2I_MM(x,y)); fp.SetOrientationDegrees(r)
    b.Add(fp)
    for pad in fp.Pads():
        n=netmap.get((ref,pad.GetNumber()))
        assert (n is None)==(pad.GetNumber() not in [k[1] for k in netmap if k[0]==ref]), (ref,pad.GetNumber())
        if n: pad.SetNet(net(n))
# silkscreen labels
def silk(x,y,t,size=1.2,rot=0,bold=False):
    tx=pcbnew.PCB_TEXT(b); tx.SetText(t); tx.SetPosition(pcbnew.VECTOR2I_MM(x,y)); tx.SetLayer(pcbnew.F_SilkS)
    tx.SetTextSize(pcbnew.VECTOR2I_MM(size,size)); tx.SetTextThickness(MM(size*0.15)); tx.SetTextAngleDegrees(rot); tx.SetBold(bold); b.Add(tx)
for i,t in enumerate(["SOL1","SOL2","SOL3","TCU1","TCU2","TCU3"]): silk(117.2,130+5.08*i,t,1.0)
silk(116.3,115.5,"+12V",1.0); silk(121.6,115.5,"GND",1.0)
for i,t in enumerate(["+5V","GND","UP","DOWN","READY","LOCK"]): silk(190+5.08*i,186.6,t,0.9)
for i,t in enumerate(["5V","GND","RX","TX"]): silk(224+2.5*i,190.3,t,0.8,90)
silk(150,188,"AW4 shield v0.1  -  Aisin AW-4 / Arduino Nano",1.8,0,True)
silk(150,192,"READY vyp. / bez napajeni = puvodni TCU",1.2)
# mounting holes
for i,(x,y) in enumerate([(104,104),(241,104),(104,196),(241,196)]):
    fp=pcbnew.FootprintLoad(FPDIR+"MountingHole.pretty","MountingHole_3.2mm_M3")
    fp.SetFPID(pcbnew.LIB_ID("MountingHole","MountingHole_3.2mm_M3"))
    fp.SetReference(f"H{i+1}"); fp.SetValue("M3"); fp.SetPosition(pcbnew.VECTOR2I_MM(x,y))
    fp.SetBoardOnly(True); fp.Reference().SetVisible(False); b.Add(fp)
# outline
X0,Y0,X1,Y1=100,100,245,200
for x0,y0,x1,y1 in [(X0,Y0,X1,Y0),(X1,Y0,X1,Y1),(X1,Y1,X0,Y1),(X0,Y1,X0,Y0)]:
    s=pcbnew.PCB_SHAPE(b); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetStart(pcbnew.VECTOR2I_MM(x0,y0)); s.SetEnd(pcbnew.VECTOR2I_MM(x1,y1))
    s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1)); b.Add(s)
# net classes
ns=b.GetDesignSettings().m_NetSettings
dflt=ns.GetDefaultNetclass(); dflt.SetTrackWidth(MM(0.3)); dflt.SetClearance(MM(0.25)); dflt.SetViaDiameter(MM(0.8)); dflt.SetViaDrill(MM(0.4))
def mk(name,w,cl,vd,vdr,pats):
    nc=pcbnew.NETCLASS(name); nc.SetTrackWidth(MM(w)); nc.SetClearance(MM(cl)); nc.SetViaDiameter(MM(vd)); nc.SetViaDrill(MM(vdr))
    ns.SetNetclass(name,nc)
    for pt in pats: ns.SetNetclassPatternAssignment(pt,name)
mk("POWER",1.5,0.3,1.4,0.8,["/+12V_IN","/+12V_FUSED","/+12V_SW","/PGND"])
mk("SOLENOID",1.0,0.3,1.2,0.7,["/SOL?_OUT","/SOL?_NO","/TCU?"])
mk("SUPPLY",0.6,0.25,1.0,0.5,["/GND","/+5V","/+5V_BUCK","/+5V_DISP","/NANO_5V","/K_LOW"])
ns.RecomputeEffectiveNetclasses()
b.BuildConnectivity()
pcbnew.SaveBoard(sys.argv[1],b)
print("saved",len(parts),"parts",len(nets),"nets")
