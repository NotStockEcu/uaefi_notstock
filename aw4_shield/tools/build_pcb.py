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
# placement: ref -> (x, y, rot)   v0.5: 76 x 82 mm, modul na výšku vlevo nahoře (USB-C k horní hraně), svorkovnice dole
X0,Y0,X1,Y1=100,100,176,182
P={}
# ---- modul RP2350-Touch-LCD-2: deska 35 x 48,2 mm na x 100.5-135.5, y 100-148.2; P1 = levá řada, P2 = pravá řada, pin 1 dole
MOD_X0, MOD_Y0 = 100.5, 100.0
P["J7"]=(MOD_X0+2.26, MOD_Y0+40.61, 180); P["J8"]=(MOD_X0+32.74, MOD_Y0+40.61, 180)
# ---- pod modulem (jen nízké SMD díly) ----
P["U4"]=(112,104,0); P["U5"]=(125,104,0); P["U6"]=(112,110,0); P["U3"]=(125,110,0)
for i,r in enumerate(["R17","R20","R23","R13","R26","R27"]): P[r]=(106.8+3.5*i,114.6,0)
P["Q6"]=(128.2,114.6,0)
P["Q2"]=(108.5,120.5,0); P["R14"]=(114,120.5,0); P["R15"]=(119,120.5,0); P["D9"]=(113,125.5,0)
P["U7"]=(127,139,90); P["C9"]=(127,145.2,0)
for i,(Ra,Rb,C,D) in enumerate([("R3","R4","C5","D3"),("R5","R6","C6","D4"),("R7","R8","C7","D5"),("R9","R10","C8","D6")]):
    y=131.0+3.7*i
    P[Ra]=(107.2,y,0); P[Rb]=(111.0,y,0); P[C]=(114.8,y,0); P[D]=(119.4,y,0)
# ---- relé + výkonové kanály (vpravo) ----
for n,(K,D,Q,Z,Rpu,Rg) in enumerate([("K1","D12","Q3","D11","R19","R18"),("K2","D14","Q4","D13","R22","R21"),("K3","D16","Q5","D15","R25","R24")]):
    yc=109.0+17.5*n
    P[K]=(141,yc,90)
    P[Q]=(169.4,yc+4.5,0)
    P[Rg]=(164.6,yc-1.5,0); P[Rpu]=(168.4,yc-1.5,180); P[Z]=(172.8,yc-1.5,0)
    P[D]=(166,yc-5.5,0) if n else (172.1,yc-5.6,0)
# ---- napájení (pás nad svorkovnicemi) ----
P["F1"]=(168.5,164,180); P["R29"]=(140.6,155.6,0); P["Q1"]=(152,164,0); P["D2"]=(152,156.6,0); P["C2"]=(146,156.6,0)
P["R1"]=(152.5,169.9,0); P["D1"]=(147.6,169.9,0)
P["C1"]=(137,163.5,0)
P["U1"]=(117,166,0); P["C3"]=(115,170.6,0); P["L1"]=(122,170.6,0); P["C4"]=(127.6,157,0); P["R2"]=(132.3,170.9,90)
P["D7"]=(110,157,0)
P["J6"]=(102.4,162,0); P["JP2"]=(127.9,162,0); P["R28"]=(127.9,168.8,90)
P["D10"]=(106.6,166,0); P["R16"]=(110.2,166,0)
# ---- svorkovnice u dolní hrany ----
PY=Y1-5.6
P["J3"]=(103,PY,0); P["J4"]=(126,PY,0); P["J2"]=(136,PY,0); P["J1"]=(168.5,PY,0)
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
    for fname,fval in (("LCSC",p.get("lcsc","")),("MPN",p.get("mpn",""))):
        fp.SetField(fname,fval)
        f=fp.GetField(fname); f.SetVisible(False); f.SetLayer(pcbnew.F_Fab)
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
def fab(x,y,t,size=1.0):
    tx=pcbnew.PCB_TEXT(b); tx.SetText(t); tx.SetPosition(pcbnew.VECTOR2I_MM(x,y)); tx.SetLayer(pcbnew.F_Fab)
    tx.SetTextSize(pcbnew.VECTOR2I_MM(size,size)); tx.SetTextThickness(MM(size*0.15)); b.Add(tx)
# názvy svorek nad svorkovnicemi
for j,(x0,names) in {"J3":(103,["+5V","GND","UP","DOWN","RDY","LOCK"]),"J4":(126,["CANH","CANL"]),
                     "J2":(136,["SOL1","SOL2","SOL3","TCU1","TCU2","TCU3"]),"J1":(168.5,["+12V","GND"])}.items():
    for i,t in enumerate(names): silk(x0+3.5*i,PY-4.3,t,0.8)
def silkline(x0,y0,x1,y1,layer=pcbnew.F_SilkS,w=0.15):
    sh=pcbnew.PCB_SHAPE(b); sh.SetShape(pcbnew.SHAPE_T_SEGMENT); sh.SetStart(pcbnew.VECTOR2I_MM(x0,y0)); sh.SetEnd(pcbnew.VECTOR2I_MM(x1,y1)); sh.SetLayer(layer); sh.SetWidth(MM(w)); b.Add(sh)
mx0,my0,mx1,my1=MOD_X0+0.1,MOD_Y0+0.5,MOD_X0+34.9,MOD_Y0+48.1   # obrys modulu
for x0,y0,x1,y1 in [(mx0,my0,mx0,my1),(mx1,my0,mx1,my1),(mx0,my1,mx1,my1)]: silkline(x0,y0,x1,y1)
for x0,y0,x1,y1 in [(mx0,my0,mx0,my1),(mx1,my0,mx1,my1),(mx0,my1,mx1,my1),(mx0,my0,mx1,my0)]: silkline(x0,y0,x1,y1,pcbnew.F_Fab,0.1)
fab(118,146.5,"MODUL RP2350-Touch-LCD-2 (displejem nahoru, USB-C nahoru)",0.9)
# title on the bottom side
def bsilk(x,y,t,size):
    tx=pcbnew.PCB_TEXT(b); tx.SetText(t); tx.SetPosition(pcbnew.VECTOR2I_MM(x,y)); tx.SetLayer(pcbnew.B_SilkS); tx.SetMirrored(True)
    tx.SetTextSize(pcbnew.VECTOR2I_MM(size,size)); tx.SetTextThickness(MM(size*0.15)); b.Add(tx)
bsilk(119,150.6,"AW4 shield v0.5",1.6); bsilk(119,153.2,"SMD: JLC, THT: rucne",1.0)
bsilk(119,155.2,"Aisin AW-4 / RP2350-Touch-LCD-2 / CAN",0.9); bsilk(119,157.1,"READY vyp. = puvodni TCU",0.9)
# potisk: u ručně pájených dílů hodnota/označení, SMD díly (osazuje JLC) jen ve výkresu (F.Fab)
SHORT={'5A':'5A','TSR 1-2450 (12V->5V 1A)':'TSR1-2450'}
LABEL_POS={"K1":(150,109,0),"K2":(150,126.5,0),"K3":(150,144,0),"F1":(166,158.2,0),"U1":(119.5,159.2,0),
           "J6":(102.4,159.0,0),"JP2":(127.9,159.4,0)}
for fp in b.GetFootprints():
    ref=fp.GetReference()
    if ref.startswith("H"): continue
    r=fp.Reference(); v=fp.Value()
    v.SetLayer(pcbnew.F_Fab); v.SetVisible(False)
    lab=None
    p=parts.get(ref,{})
    if p.get("hand"): lab=SHORT.get(fp.GetValue(), ref)
    if ref in ("J1","J2","J3","J4","J7","J8"): lab=None
    if ref=="D10": lab="READY"
    if lab:
        t=pcbnew.PCB_TEXT(fp); t.SetText(lab); t.SetLayer(pcbnew.F_SilkS)
        t.SetPosition(r.GetPosition()); t.SetTextAngle(r.GetTextAngle())
        if ref in LABEL_POS:
            lx,ly,la=LABEL_POS[ref]; t.SetPosition(pcbnew.VECTOR2I_MM(lx,ly)); t.SetTextAngleDegrees(la)
        if ref=="D10": t.SetPosition(pcbnew.VECTOR2I_MM(106.6,168.2)); t.SetTextAngleDegrees(0)
        sz=1.2 if ref[0]=="K" else 0.8
        t.SetTextSize(pcbnew.VECTOR2I_MM(sz,sz)); t.SetTextThickness(MM(sz*0.16)); fp.Add(t)
    r.SetLayer(pcbnew.F_Fab); r.SetTextSize(pcbnew.VECTOR2I_MM(0.7,0.7)); r.SetTextThickness(MM(0.11))
# mounting holes
HOLES=[(165.0,102.9,"MountingHole_3.2mm_M3"),(103.5,152,"MountingHole_3.2mm_M3"),(172.5,155.5,"MountingHole_3.2mm_M3")]
for i,(x,y,hname) in enumerate(HOLES):
    fp=pcbnew.FootprintLoad(FPDIR+"MountingHole.pretty",hname)
    fp.SetFPID(pcbnew.LIB_ID("MountingHole",hname))
    fp.SetReference(f"H{i+1}"); fp.SetValue("M3"); fp.SetPosition(pcbnew.VECTOR2I_MM(x,y))
    fp.SetBoardOnly(True); fp.Reference().SetVisible(False); b.Add(fp)
# outline
for x0,y0,x1,y1 in [(X0,Y0,X1,Y0),(X1,Y0,X1,Y1),(X1,Y1,X0,Y1),(X0,Y1,X0,Y0)]:
    s=pcbnew.PCB_SHAPE(b); s.SetShape(pcbnew.SHAPE_T_SEGMENT); s.SetStart(pcbnew.VECTOR2I_MM(x0,y0)); s.SetEnd(pcbnew.VECTOR2I_MM(x1,y1))
    s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1)); b.Add(s)
b.GetDesignSettings().m_CopperEdgeClearance=MM(0.5)   # pro autorouter; finish_pcb.py vrátí 0,3 mm (JLC)   # typical fab limit (JLCPCB 0.3 mm)
# net classes
ns=b.GetDesignSettings().m_NetSettings
dflt=ns.GetDefaultNetclass(); dflt.SetTrackWidth(MM(0.3)); dflt.SetClearance(MM(0.25)); dflt.SetViaDiameter(MM(0.8)); dflt.SetViaDrill(MM(0.4))
def mk(name,w,cl,vd,vdr,pats):
    nc=pcbnew.NETCLASS(name); nc.SetTrackWidth(MM(w)); nc.SetClearance(MM(cl)); nc.SetViaDiameter(MM(vd)); nc.SetViaDrill(MM(vdr))
    ns.SetNetclass(name,nc)
    for pt in pats: ns.SetNetclassPatternAssignment(pt,name)
mk("POWER",1.5,0.3,1.4,0.8,["/+12V_IN","/+12V_FUSED","/+12V_SW"])
mk("SOLENOID",1.0,0.3,1.2,0.7,["/SOL?_OUT","/SOL?_NO","/TCU?"])
mk("SUPPLY",0.6,0.25,1.0,0.5,["/GND","/PGND","/+12V_AUX","/+5V","/+5V_BUCK","/MOD_5V","/K_LOW"])
ns.RecomputeEffectiveNetclasses()
b.BuildConnectivity()
pcbnew.SaveBoard(sys.argv[1],b)
print("saved",len(parts),"parts",len(nets),"nets")
