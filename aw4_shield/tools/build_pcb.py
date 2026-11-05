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
# ---- top power strip ----
P["J1"]=(118,106,180); P["F1"]=(126,106,0); P["Q1"]=(138,107,0); P["D2"]=(148,105,0)
P["C1"]=(160,106,0); P["U1"]=(172,108,0); P["L1"]=(183,104,0); P["C4"]=(184,110,0); P["JP1"]=(191,103,0)
P["R1"]=(137,113,0); P["D1"]=(142.5,113,0); P["C2"]=(148,113,0); P["C3"]=(174,115,0); P["R2"]=(180.5,116.5,0)
# ---- solenoid channels ----
P["J2"]=(107.5,119,270)
for n,(K,D,Q,Z,Rpu,Rg,U,Ra) in enumerate([("K1","D12","Q3","D11","R19","R18","U4","R17"),("K2","D14","Q4","D13","R22","R21","U5","R20"),("K3","D16","Q5","D15","R25","R24","U6","R23")]):
    kx=126+18*n
    P[K]=(kx,118,0); P[Q]=(kx-3,143,0); P[D]=(kx-3,149,0)
    P[Z]=(kx-7,154,0); P[Rpu]=(kx-1.5,154,0); P[Rg]=(kx+4,154,0)
    P[U]=(kx-4,159,0); P[Ra]=(kx+6.5,160,0)
# ---- relay driver (right column) ----
P["U3"]=(174,121,0); P["R13"]=(174,129,0); P["R14"]=(180.5,129,0); P["R15"]=(174,134,0); P["D9"]=(180.5,134,0)
P["R16"]=(174,139,0); P["D10"]=(180.5,139,0); P["Q2"]=(174,147,0)
P["Q6"]=(187,121,0); P["R26"]=(187,129,0); P["R27"]=(193.5,129,0)
# ---- CAN ----
P["U7"]=(186,133,0); P["C9"]=(185.5,145.5,0); P["C10"]=(190.5,145.5,0); P["R28"]=(186,150,0); P["JP2"]=(195.5,148.8,0)
P["J4"]=(194,161.5,90)
# ---- RP2350-Touch-LCD-2 module: PCB 35 x 48.2 mm lying at x 151.8-200, y 165-200, USB-C to the right edge ----
# header rows 30.48 mm apart; P1 (J7) top row, P2 (J8) bottom row; pin 1 on the left (assumed 7.59 mm from module edge)
MOD_X0, MOD_Y0 = 151.8, 165.0
P["J7"]=(MOD_X0+7.59, MOD_Y0+2.26, 90); P["J8"]=(MOD_X0+7.59, MOD_Y0+2.26+30.48, 90)
P["J6"]=(143,193,90)
# ---- inputs: row A = 6k8 / 10k, row B = 100n / 3V6 ----
for i,(R1k,R10k,C,D) in enumerate([("R3","R4","C5","D3"),("R5","R6","C6","D4"),("R7","R8","C7","D5"),("R9","R10","C8","D6")]):
    x=108+11*i
    P[R1k]=(x,177,0); P[R10k]=(x+5.5,177,0); P[C]=(x,183,0); P[D]=(x+5.5,183,0)
# ---- bottom connector ----
P["J3"]=(112,194.5,0)
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
SHORT={'330': '330', '1k': '1k', '10k': '10k', '100k': '100k', '220': '220', '2k2': '2k2', '100R': '100R', '0R': '0R', '100nF': '100n', '10uF 25V': '10u', '47uF 10V': '47u', '100uF 10V': '100u', '100uF 16V': '100u', '470uF 25V': '470u', '600R@100MHz': 'FB', 'BZX55C12': '12V', 'BZX55C15': '15V', 'BZX55C5V6': '5V6', '1N4148': '4148', '1N4007': '4007', 'P6KE27A': 'P6KE27', 'IRF4905': 'IRF4905', 'IRF9540N': '9540N', 'BC337': 'BC337', 'PC817': 'PC817', 'zelená READY': 'LED', '5A': '5A', 'TSR 1-2450 (12V->5V 1A)': 'TSR1-2450', 'G5LE-1 12V SPDT': '', 'Arduino Nano': 'ARDUINO NANO', '6k8': '6k8', '4k7': '4k7', '47k': '47k', '120': '120', '470': '470', 'BZX55C3V6': '3V6', 'MCP2562-E/P': 'MCP2562', '22k': '22k'}
for i,t in enumerate(["SOL1","SOL2","SOL3","TCU1","TCU2","TCU3"]): silk(115.4,119+5.08*i,t,0.8,90)
silk(118,113.4,"+12V",0.8); silk(112.9,113.4,"GND",0.8)
for i,t in enumerate(["+5V","GND","UP","DOWN","READY","LOCK"]): silk(112+5.08*i,187.1,t,0.8)
silk(184.4,161.5,"CAN-H",0.8); silk(184.4,156.4,"CAN-L",0.8)
def silkline(x0,y0,x1,y1,layer=pcbnew.F_SilkS,w=0.15):
    sh=pcbnew.PCB_SHAPE(b); sh.SetShape(pcbnew.SHAPE_T_SEGMENT); sh.SetStart(pcbnew.VECTOR2I_MM(x0,y0)); sh.SetEnd(pcbnew.VECTOR2I_MM(x1,y1)); sh.SetLayer(layer); sh.SetWidth(MM(w)); b.Add(sh)
mx0,my0,mx1,my1=MOD_X0,MOD_Y0,MOD_X0+47.7,MOD_Y0+34.5   # obrys modulu (o 0,5 mm kratší, ať nezasahuje do hrany desky)
for x0,y0,x1,y1 in [(mx0,my0,mx1,my0),(mx0,my1,mx1,my1),(mx0,my0,mx0,my1)]: silkline(x0,y0,x1,y1)
silk(175.9,177.0,"RP2350-Touch-LCD-2",1.2,0,True); silk(175.9,180.0,"displejem nahoru, USB-C ->",1.0)
silk(175.9,183.0,"pod modulem jen nizke soucastky",0.8)
silk(150.3,193,"rezerva",0.8,90)
# title on the bottom side
def bsilk(x,y,t,size):
    tx=pcbnew.PCB_TEXT(b); tx.SetText(t); tx.SetPosition(pcbnew.VECTOR2I_MM(x,y)); tx.SetLayer(pcbnew.B_SilkS); tx.SetMirrored(True)
    tx.SetTextSize(pcbnew.VECTOR2I_MM(size,size)); tx.SetTextThickness(MM(size*0.15)); b.Add(tx)
bsilk(142,123.8,"AW4 shield v0.4 (THT, 100x100)",1.8); bsilk(142,126.6,"Aisin AW-4 / RP2350-Touch-LCD-2 / CAN",1.3)
bsilk(142,129.2,"READY vyp. / bez napajeni = puvodni TCU",1.1)
# silkscreen shows WHAT TO FIT (short value) where the reference usually sits; references go to F.Fab (assembly drawing)
for fp in b.GetFootprints():
    ref=fp.GetReference()
    if ref.startswith("H"): continue
    r=fp.Reference(); v=fp.Value()
    v.SetLayer(pcbnew.F_Fab); v.SetVisible(False)
    lab=SHORT.get(fp.GetValue())
    if ref[0] in "JK" or ref=="JP1": lab=ref
    if ref in ("J3","J5"): lab=None
    if ref=="J7": lab="MODUL P1"
    if ref=="J8": lab="MODUL P2"
    if lab:
        t=pcbnew.PCB_TEXT(fp); t.SetText(lab); t.SetLayer(pcbnew.F_SilkS)
        t.SetPosition(r.GetPosition()); t.SetTextAngle(r.GetTextAngle())
        LABEL_POS={"C1":(162.5,112.9,0),"J7":(175.9,170.8,0),"J8":(175.9,194.3,0),"J6":(145.5,190.4,0),"U7":(189.8,137.0,90),
                   "K1":(126,127,0),"K2":(144,127,0),"K3":(162,127,0)}
        if ref in LABEL_POS:
            lx,ly,la=LABEL_POS[ref]; t.SetPosition(pcbnew.VECTOR2I_MM(lx,ly)); t.SetTextAngleDegrees(la)
        t.SetTextSize(pcbnew.VECTOR2I_MM(0.8,0.8)); t.SetTextThickness(MM(0.13)); fp.Add(t)
    r.SetLayer(pcbnew.F_Fab); r.SetTextSize(pcbnew.VECTOR2I_MM(0.8,0.8)); r.SetTextThickness(MM(0.12))
# mounting holes
HOLES=[(103.5,103.5,"MountingHole_3.2mm_M3"),(196.5,103.5,"MountingHole_3.2mm_M3"),(103.5,196.5,"MountingHole_3.2mm_M3")]
# modul drží v dutinkových lištách (bez děr pod modulem – nezávisí na přesné poloze pinů vůči jeho dírám)
for i,(x,y,hname) in enumerate(HOLES):
    fp=pcbnew.FootprintLoad(FPDIR+"MountingHole.pretty",hname)
    fp.SetFPID(pcbnew.LIB_ID("MountingHole",hname))
    fp.SetReference(f"H{i+1}"); fp.SetValue("M3" if "M3" in hname else "M2.5"); fp.SetPosition(pcbnew.VECTOR2I_MM(x,y))
    fp.SetBoardOnly(True); fp.Reference().SetVisible(False); b.Add(fp)
# outline
X0,Y0,X1,Y1=100,100,200,200
for x0,y0,x1,y1 in [(X0,Y0,X1,Y0),(X1,Y0,X1,Y1),(X1,Y1,X0,Y1),(X0,Y1,X0,Y0)]:
    s=pcbnew.PCB_SHAPE(b); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetStart(pcbnew.VECTOR2I_MM(x0,y0)); s.SetEnd(pcbnew.VECTOR2I_MM(x1,y1))
    s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1)); b.Add(s)
b.GetDesignSettings().m_CopperEdgeClearance=MM(0.3)   # typical fab limit (JLCPCB 0.3 mm)
# net classes
ns=b.GetDesignSettings().m_NetSettings
dflt=ns.GetDefaultNetclass(); dflt.SetTrackWidth(MM(0.3)); dflt.SetClearance(MM(0.25)); dflt.SetViaDiameter(MM(0.8)); dflt.SetViaDrill(MM(0.4))
def mk(name,w,cl,vd,vdr,pats):
    nc=pcbnew.NETCLASS(name); nc.SetTrackWidth(MM(w)); nc.SetClearance(MM(cl)); nc.SetViaDiameter(MM(vd)); nc.SetViaDrill(MM(vdr))
    ns.SetNetclass(name,nc)
    for pt in pats: ns.SetNetclassPatternAssignment(pt,name)
mk("POWER",1.5,0.3,1.4,0.8,["/+12V_IN","/+12V_FUSED","/+12V_SW","/PGND"])
mk("SOLENOID",1.0,0.3,1.2,0.7,["/SOL?_OUT","/SOL?_NO","/TCU?"])
mk("SUPPLY",0.6,0.25,1.0,0.5,["/GND","/+5V","/+5V_BUCK","/MOD_5V","/K_LOW"])
ns.RecomputeEffectiveNetclasses()
b.BuildConnectivity()
pcbnew.SaveBoard(sys.argv[1],b)
print("saved",len(parts),"parts",len(nets),"nets")
