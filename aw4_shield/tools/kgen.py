import uuid, re, collections, csv, sys
OUT="/home/user/uaefi_notstock/aw4_shield/kicad/"
PROJ="aw4_shield"
ROOT=str(uuid.uuid4())
def U(): return str(uuid.uuid4())
FONT="(effects (font (size 1.27 1.27)))"
FONTH="(effects (font (size 1.27 1.27)) (hide yes))"

# ---------------- symbol library (embedded) ----------------
def pin_el(x,y,ang,name,num,length=2.54,ptype="passive"):
    return (f'(pin {ptype} line (at {x:g} {y:g} {ang}) (length {length}) '
            f'(name "{name}" {FONT}) (number "{num}" {FONT}))')
def props(ref,val,fp=""):
    return (f'(property "Reference" "{ref}" (at 0 0 0) {FONT}) (property "Value" "{val}" (at 0 0 0) {FONT}) '
            f'(property "Footprint" "{fp}" (at 0 0 0) {FONTH}) (property "Datasheet" "" (at 0 0 0) {FONTH}) '
            f'(property "Description" "" (at 0 0 0) {FONTH})')
STROKE='(stroke (width 0.254) (type default))'
def two_pin(name,ref,gfx,n1="1",n2="2"):
    return (f'(symbol "AW4:{name}" (pin_names (offset 0) hide) (exclude_from_sim no) (in_bom yes) (on_board yes) {props(ref,name)} '
            f'(symbol "{name}_0_1" {gfx}) '
            f'(symbol "{name}_1_1" {pin_el(0,3.81,270,n1,"1")} {pin_el(0,-3.81,90,n2,"2")}))')
GFX={
 'R':f'(rectangle (start -1.016 -2.54) (end 1.016 2.54) {STROKE} (fill (type none)))',
 'C':f'(polyline (pts (xy -2.032 0.762) (xy 2.032 0.762)) {STROKE} (fill (type none))) (polyline (pts (xy -2.032 -0.762) (xy 2.032 -0.762)) {STROKE} (fill (type none)))',
 'D':f'(polyline (pts (xy -1.27 -1.27) (xy 1.27 -1.27) (xy 0 1.27) (xy -1.27 -1.27)) {STROKE} (fill (type none))) (polyline (pts (xy -1.27 1.27) (xy 1.27 1.27)) {STROKE} (fill (type none)))',
}
GFX['CP']=GFX['C']+f' (polyline (pts (xy -2.54 1.778) (xy -1.524 1.778)) {STROKE} (fill (type none))) (polyline (pts (xy -2.032 1.27) (xy -2.032 2.286)) {STROKE} (fill (type none)))'
GFX['FUSE']=GFX['R']
GFX['FERRITE']=GFX['R']
def box_sym(name,ref,left,right,fp=""):
    # left/right: list of (num,name)
    n=max(len(left),len(right)); H=n*2.54+2.54; W=15.24
    body=f'(rectangle (start {-W/2:g} {H/2:g}) (end {W/2:g} {-H/2:g}) {STROKE} (fill (type background)))'
    pins=[]
    def ys(k,i): return ((k-1)/2.0-i)*2.54
    for i,(num,nm) in enumerate(left): pins.append(pin_el(-(W/2+2.54),ys(len(left),i),0,nm,num))
    for i,(num,nm) in enumerate(right): pins.append(pin_el((W/2+2.54),ys(len(right),i),180,nm,num))
    return (f'(symbol "AW4:{name}" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes) {props(ref,name)} '
            f'(symbol "{name}_0_1" {body}) (symbol "{name}_1_1" {" ".join(pins)}))')
BOX={}  # name -> (left,right)
def defbox(name,ref,left,right): BOX[name]=(left,right)
defbox('CONN2','J',[("1","1"),("2","2")],[])
defbox('CONN4','J',[("1","1"),("2","2"),("3","3"),("4","4")],[])
defbox('CONN6','J',[(str(i),str(i)) for i in range(1,7)],[])
defbox('CONN7','J',[(str(i),str(i)) for i in range(1,8)],[])
defbox('PFET','Q',[("1","G")],[("2","D"),("3","S")])
defbox('NPN','Q',[("2","B")],[("1","C"),("3","E")])
defbox('PC817','U',[("1","A"),("2","K")],[("4","C"),("3","E")])
defbox('RELAY_SPDT','K',[("2","COIL1"),("5","COIL2")],[("1","COM"),("4","NC"),("3","NO")])
defbox('REG_5V','U',[("1","VIN"),("2","GND")],[("3","VOUT")])
nano_left=[("1","D1/TX"),("2","D0/RX"),("3","RESET"),("4","GND"),("5","D2"),("6","D3"),("7","D4"),("8","D5"),("9","D6"),("10","D7"),("11","D8"),("12","D9"),("13","D10"),("14","D11"),("15","D12")]
nano_right=[("16","D13"),("17","3V3"),("18","AREF"),("19","A0"),("20","A1"),("21","A2"),("22","A3"),("23","A4"),("24","A5"),("25","A6"),("26","A7"),("27","5V"),("28","RESET"),("29","GND"),("30","VIN")]
defbox('ARDUINO_NANO','U',nano_left,nano_right)
LIB=[two_pin('R','R',GFX['R']),two_pin('C','C',GFX['C']),two_pin('CP','C',GFX['CP']),
     two_pin('D','D',GFX['D'],"K","A"),two_pin('LED','D',GFX['D'],"K","A"),two_pin('FUSE','F',GFX['FUSE']),two_pin('FERRITE','L',GFX['FERRITE'])]
for k,(l,r) in BOX.items(): LIB.append(box_sym(k,BOX_REF if False else {'CONN2':'J','CONN4':'J','CONN6':'J','CONN7':'J','PFET':'Q','NPN':'Q','PC817':'U','RELAY_SPDT':'K','REG_5V':'U','ARDUINO_NANO':'U'}[k],l,r))

# pin geometry helper: returns {num:(dx,dy,outward_angle)} in sheet coordinates relative to part origin
def pin_geom(sym):
    g={}
    if sym in ('R','C','CP','D','LED','FUSE','FERRITE'):
        g["1"]=(0,-3.81,90)    # top pin; sheet y inverted (lib +y -> sheet -y); label points up (angle 90)
        g["2"]=(0,3.81,270)
    else:
        l,r=BOX[sym]; n=max(len(l),len(r)); W=15.24
        def ys(k,i): return ((k-1)/2.0-i)*2.54
        for i,(num,nm) in enumerate(l): g[num]=(-(W/2+2.54),-ys(len(l),i),180)
        for i,(num,nm) in enumerate(r): g[num]=((W/2+2.54),-ys(len(r),i),0)
    return g
def pin_names(sym):
    if sym in ('R','C','CP','FUSE','FERRITE'): return {"1":"1","2":"2"}
    if sym in ('D','LED'): return {"1":"K","2":"A"}
    l,r=BOX[sym]; d={}
    for num,nm in l+r: d[num]=nm
    return d

# ---------------- schematic content ----------------
parts=[]; texts=[]
cnt=collections.Counter()
def newref(p): cnt[p]+=1; return f"{p}{cnt[p]}"
def add(sym,val,fp,x,y,nets,pref,dnp=False,note="",ref=None):
    ref=ref or newref(pref)
    x=round(round(x/2.54)*2.54,2); y=round(round(y/2.54)*2.54,2)
    parts.append(dict(ref=ref,sym=sym,val=val,fp=fp,x=x,y=y,nets=nets,dnp=dnp,note=note,uuid=U())); return ref
def text(x,y,t,size=2.5):
    texts.append((x,y,t,size))
R0805="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical"; C0805="Capacitor_THT:C_Disc_D3.0mm_W2.0mm_P2.50mm"
def res(val,x,y,a,b,note=""): return add('R',val,R0805,x,y,{"1":a,"2":b},"R",note=note)
def cap(val,x,y,a,b): return add('C',val,C0805,x,y,{"1":a,"2":b},"C")
def diode(val,fp,x,y,k,a,note=""): return add('D',val,fp,x,y,{"1":k,"2":a},"D",note=note)

# --- Power ---
text(20,25,"NAPÁJENÍ: +12V → F1 → Q0 (ochrana proti přepólování) → +12V_SW; TSR 1-2450 → +5V; PGND ↔ GND jen přes R 0R (hvězda)",3)
add('CONN2',"J1 +12V / PGND","TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",40,50,{"1":"+12V_IN","2":"PGND"},"J",ref="J1")
add('FUSE',"5A","Fuse:Fuseholder_TR5_Littelfuse_No560_No460",75,50,{"1":"+12V_IN","2":"+12V_FUSED"},"F",note="pojistka TR5 5 A (T) v patici TR5")
add('PFET',"IRF4905","Package_TO_SOT_THT:TO-220-3_Vertical",110,50,{"1":"Q0_G","2":"+12V_FUSED","3":"+12V_SW"},"Q",note="ochrana proti přepólování (D na vstupu, S na zátěži)")
res("100k",150,50,"Q0_G","PGND")
diode("BZX55C12","Diode_THT:D_DO-35_SOD27_P2.54mm_Vertical_CathodeUp",170,50,"+12V_SW","Q0_G",note="zener 12 V gate-source Q0")
diode("P6KE27A","Diode_THT:D_DO-15_P5.08mm_Vertical_CathodeUp",190,50,"+12V_SW","PGND",note="TVS 600 W (load dump), VBR 25,7-28,4 V")
add('CP',"470uF 25V","Capacitor_THT:CP_Radial_D10.0mm_P5.00mm",210,50,{"1":"+12V_SW","2":"PGND"},"C")
cap("100nF",230,50,"+12V_SW","PGND")
add('REG_5V',"TSR 1-2450 (12V->5V 1A)","Converter_DCDC:Converter_DCDC_TRACO_TSR-1_THT",275,50,{"1":"+12V_SW","2":"GND","3":"+5V_BUCK"},"U",note="spínaný stabilizátor ve formátu 7805, vstup 6,5-36 V")
add('CP',"10uF 25V","Capacitor_THT:CP_Radial_D5.0mm_P2.00mm",300,50,{"1":"+12V_SW","2":"GND"},"C")
res("0R",320,50,"PGND","GND",note="hvězda PGND-GND (jediné spojení zemí)")
add('FERRITE',"600R@100MHz","Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical",345,50,{"1":"+5V_BUCK","2":"+5V"},"L")
add('CP',"47uF 10V","Capacitor_THT:CP_Radial_D6.3mm_P2.50mm",370,50,{"1":"+5V","2":"GND"},"C")
add('CONN2',"JP1 5V -> Nano (jumper)","Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",405,50,{"1":"+5V","2":"NANO_5V"},"J",note="sundat při programování po USB",ref="JP1")

# --- Arduino + connectors ---
text(20,95,"ARDUINO NANO a konektory",3)
add('ARDUINO_NANO',"Arduino Nano","Module:Arduino_Nano",60,140,{
 "4":"GND","5":"D2_UART_RX","6":"D3_UART_TX","7":"UP_D","8":"DOWN_D","9":"READY_D","10":"LOCKUP_D","11":"K_CTRL","12":"SOL1_CTRL","13":"SOL2_CTRL","14":"SOL3_CTRL","15":"D12",
 "16":"D13","19":"A0","20":"A1","21":"A2","22":"A3","27":"NANO_5V","29":"GND"},"U",note="piny 1-15 / 16-30 dle KiCad footprintu Module:Arduino_Nano, ověřit")
add('CONN7',"J6 rezerva","Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical",130,140,{"1":"D12","2":"D13","3":"A0","4":"A1","5":"A2","6":"A3","7":"GND"},"J",ref="J6")
add('CONN6',"J2 SOL1-3 / TCU1-3","TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-6-5.08_1x06_P5.08mm_Horizontal",195,140,{"1":"SOL1_OUT","2":"SOL2_OUT","3":"SOL3_OUT","4":"TCU1","5":"TCU2","6":"TCU3"},"J",note="1-3 k převodovce, 4-6 z původní TCU",ref="J2")
add('CONN6',"J3 UP DOWN READY LOCKUP","TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-6-5.08_1x06_P5.08mm_Horizontal",255,140,{"1":"+5V","2":"GND","3":"UP_IN","4":"DOWN_IN","5":"READY_IN","6":"LOCKUP_IN"},"J",ref="J3")

# --- Inputs ---
text(20,172,"VSTUPY: 5 V spínače (aktivní HIGH): 1k série, 10k pull-down, 100nF, ESD",3)
x=40
for nm in ("UP","DOWN","READY","LOCKUP"):
    res("1k",x,197,nm+"_IN",nm+"_D")
    res("10k",x+24,197,nm+"_D","GND")
    cap("100nF",x+48,197,nm+"_D","GND")
    diode("BZX55C5V6","Diode_THT:D_DO-35_SOD27_P2.54mm_Vertical_CathodeUp",x+72,197,nm+"_D","GND")
    x+=96

# --- Display ---
text(330,100,"DISPLEJ NEXTION (J5)",3)
add('CONN4',"J5 Nextion +5V GND RX TX","Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical",350,140,{"1":"+5V_DISP","2":"GND","3":"DISP_RX","4":"DISP_TX"},"J",ref="J5")
add('FERRITE',"600R@100MHz","Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical",395,140,{"1":"+5V","2":"+5V_DISP"},"L")
add('CP',"100uF 10V","Capacitor_THT:CP_Radial_D6.3mm_P2.50mm",420,140,{"1":"+5V_DISP","2":"GND"},"C")
cap("100nF",445,140,"+5V_DISP","GND")
res("100R",470,140,"D3_UART_TX","DISP_RX",note="Nano TX -> Nextion RX")
res("100R",500,140,"DISP_TX","D2_UART_RX",note="Nextion TX -> Nano RX")
diode("BZX55C5V6","Diode_THT:D_DO-35_SOD27_P2.54mm_Vertical_CathodeUp",530,140,"DISP_RX","GND")
diode("BZX55C5V6","Diode_THT:D_DO-35_SOD27_P2.54mm_Vertical_CathodeUp",560,140,"DISP_TX","GND")

# --- Relay coil drive ---
text(20,232,"CÍVKY RELÉ K1-K3 (společné): READY přepínač AND D8 (aktivní LOW)",3)
res("330",40,258,"READY_IN","RLY_A")
add('PC817',"PC817","Package_DIP:DIP-4_W7.62mm",75,258,{"1":"RLY_A","2":"K_CTRL","4":"RLY_C","3":"Q4_B"},"U")
res("220",110,258,"+12V_SW","RLY_C")
res("10k",130,258,"Q4_B","PGND")
add('NPN',"BC337","Package_TO_SOT_THT:TO-92_Inline_Wide",160,258,{"2":"Q4_B","1":"K_LOW","3":"PGND"},"Q",note="TO-92 pinout BC337: 1=C 2=B 3=E")
diode("1N4148","Diode_THT:D_DO-35_SOD27_P2.54mm_Vertical_CathodeUp",190,258,"+12V_SW","K_LOW",note="flyback cívek")
res("2k2",210,258,"+12V_SW","LED_A")
add('LED',"zelená READY","LED_THT:LED_D3.0mm",230,258,{"1":"K_LOW","2":"LED_A"},"D")

# --- Solenoid channels ---
text(20,292,"VÝSTUPNÍ KANÁLY SOLENOIDŮ (high-side): opto → P-MOSFET → NO kontakt relé; COM → solenoid; NC → původní TCU",3)
for n in (1,2,3):
    y=345+(n-1)*0
    xb=30+(n-1)*180
    yy=318
    res("330",xb,yy,f"SOL{n}_CTRL",f"S{n}_LEDA")
    add('PC817',"PC817","Package_DIP:DIP-4_W7.62mm",xb+28,yy+25,{"1":f"S{n}_LEDA","2":"GND","4":f"S{n}_C","3":"PGND"},"U")
    res("1k",xb+52,yy,f"S{n}_C",f"G{n}")
    res("10k",xb+70,yy,"+12V_SW",f"G{n}")
    diode("BZX55C15","Diode_THT:D_DO-35_SOD27_P2.54mm_Vertical_CathodeUp",xb+88,yy,"+12V_SW",f"G{n}",note="zener 15 V gate-source")
    add('PFET',"IRF9540N","Package_TO_SOT_THT:TO-220-3_Vertical",xb+118,yy+25,{"1":f"G{n}","2":f"SOL{n}_NO","3":"+12V_SW"},"Q")
    add('RELAY_SPDT',"G5LE-1 12V SPDT","Relay_THT:Relay_SPDT_Omron-G5LE-1",xb+158,yy+25,{"2":"+12V_SW","5":"K_LOW","1":f"SOL{n}_OUT","4":f"TCU{n}","3":f"SOL{n}_NO"},"K",note="čísla pinů relé ověřit podle footprintu")
    diode("1N4007","Diode_THT:D_DO-41_SOD81_P5.08mm_Vertical_CathodeUp",xb+158,yy+60,f"SOL{n}_OUT","PGND",note="flyback na straně solenoidu")

# ---------------- connectivity check ----------------
nets=collections.defaultdict(list)
for p in parts:
    pn=pin_names(p['sym']); g=pin_geom(p['sym'])
    for num in g:
        if num in p['nets']: nets[p['nets'][num]].append((p['ref'],num))
bad=[(n,v) for n,v in nets.items() if len(v)<2]
print("nets:",len(nets),"parts:",len(parts))
for n,v in bad: print("SINGLE-PIN NET:",n,v)
# overlap check of pin points
pts={}
for p in parts:
    g=pin_geom(p['sym'])
    for num,(dx,dy,a) in g.items():
        key=(round(p['x']+dx,2),round(p['y']+dy,2))
        pts.setdefault(key,[]).append((p['ref'],num,p['nets'].get(num)))
for k,v in pts.items():
    if len(v)>1 and len({t[2] for t in v})>1: print("PIN POINT COLLISION",k,v)

# ---------------- emit .kicad_sch ----------------
def esc(s): return s.replace("\\","\\\\").replace('"','\\"')
o=[]
o.append(f'(kicad_sch (version 20231120) (generator "eeschema") (generator_version "8.0") (uuid "{ROOT}") (paper "A2") '
         f'(title_block (title "AW4 shield – ovladač automatické převodovky Aisin AW-4") (rev "0.1") (comment 1 "Arduino Nano + Nextion UART, 3x SPDT relé, 3x high-side P-MOSFET")) ')
o.append("(lib_symbols "+" ".join(LIB)+")")
for (x,y,t,sz) in texts:
    o.append(f'(text "{esc(t)}" (exclude_from_sim no) (at {x} {y} 0) (effects (font (size {sz} {sz}) bold) (justify left bottom)) (uuid "{U()}"))')
nc=0
for p in parts:
    sym=p['sym']; g=pin_geom(sym); X,Y=p['x'],p['y']
    is2=sym in ('R','C','CP','D','LED','FUSE','FERRITE')
    if is2: rx,ry,jx=X+3.5,Y-1.27,"left"
    else:
        l,r=BOX[sym]; n=max(len(l),len(r)); H=n*2.54+2.54
        rx,ry,jx=X,Y-H/2-2.5,"center"
    vy = ry+2.54 if is2 else ry-2.5
    fp_eff=FONTH
    J="" if jx=="center" else f" (justify {jx})"
    o.append(f'(symbol (lib_id "AW4:{sym}") (at {X:g} {Y:g} 0) (unit 1) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp {"yes" if p["dnp"] else "no"}) (uuid "{p["uuid"]}") '
      f'(property "Reference" "{p["ref"]}" (at {rx:g} {ry:g} 0) (effects (font (size 1.27 1.27)){J})) '
      f'(property "Value" "{esc(p["val"])}" (at {rx:g} {vy:g} 0) (effects (font (size 1.27 1.27)){J})) '
      f'(property "Footprint" "{esc(p["fp"])}" (at {X:g} {Y:g} 0) {fp_eff}) '
      f'(property "Datasheet" "" (at {X:g} {Y:g} 0) {fp_eff}) '
      f'(property "Description" "{esc(p["note"])}" (at {X:g} {Y:g} 0) {fp_eff}) '
      + " ".join(f'(pin "{num}" (uuid "{U()}"))' for num in g)
      + f' (instances (project "{PROJ}" (path "/{ROOT}" (reference "{p["ref"]}") (unit 1)))))')
    for num,(dx,dy,a) in g.items():
        px,py=X+dx,Y+dy
        net=p['nets'].get(num)
        if net is None:
            o.append(f'(no_connect (at {px:g} {py:g}) (uuid "{U()}"))'); nc+=1
        else:
            just="right bottom" if a in (180,270) else "left bottom"
            o.append(f'(label "{esc(net)}" (at {px:g} {py:g} {a}) (effects (font (size 1.27 1.27)) (justify {just})) (uuid "{U()}"))')
o.append('(sheet_instances (path "/" (page "1")))')
o.append(")")
s="\n".join(o)
open(OUT+PROJ+".kicad_sch","w").write(s)
open(OUT+PROJ+".kicad_pro","w").write('{\n  "meta": {\n    "filename": "aw4_shield.kicad_pro",\n    "version": 1\n  }\n}\n')
print("written, no_connects:",nc)
libtxt="(kicad_symbol_lib (version 20231120) (generator \"aw4gen\") (generator_version \"8.0\")\n"+"\n".join(l.replace('(symbol "AW4:','(symbol "',1) for l in LIB)+"\n)\n"
open(OUT+"AW4.kicad_sym","w").write(libtxt)
open(OUT+"sym-lib-table","w").write('(sym_lib_table\n  (version 7)\n  (lib (name "AW4")(type "KiCad")(uri "${KIPRJMOD}/AW4.kicad_sym")(options "")(descr "AW4 shield symbols"))\n)\n')
import json
json.dump({"root":ROOT,"parts":parts},open("/tmp/claude-0/-home-user-uaefi-notstock/e6f7c8f6-6e72-54f1-b6cf-eda4a168ecbb/scratchpad/parts.json","w"),ensure_ascii=False,indent=1)
# paren balance
depth=0; instr=False; esc_=False
for ch in s:
    if instr:
        if esc_: esc_=False
        elif ch=="\\": esc_=True
        elif ch=='"': instr=False
        continue
    if ch=='"': instr=True
    elif ch=="(": depth+=1
    elif ch==")":
        depth-=1
        assert depth>=0
print("paren depth end:",depth)
# BOM
grp=collections.OrderedDict()
for p in parts:
    k=(p['val'],p['fp']); grp.setdefault(k,[]).append(p['ref'])
with open("/home/user/uaefi_notstock/aw4_shield/bom.csv","w",newline="") as f:
    w=csv.writer(f); w.writerow(["Ref","Qty","Value","Footprint","Poznámka"])
    for (val,fp),refs in sorted(grp.items(),key=lambda kv:(kv[1][0][0],int(re.sub(r"\D","",kv[1][0])))):
        note=[p['note'] for p in parts if p['ref']==refs[0]][0]
        w.writerow([" ".join(refs),len(refs),val,fp,note])
    w.writerow(["U_DISP",1,"Nextion NX3224T024 (2,4\") nebo NX4024T032","(mimo desku)","displej, UART 5 V"])
