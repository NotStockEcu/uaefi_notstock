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
defbox('NPN','Q',[("1","B")],[("3","C"),("2","E")])
defbox('PC817','U',[("1","A"),("2","K")],[("4","C"),("3","E")])
defbox('RELAY_SPDT','K',[("2","COIL1"),("5","COIL2")],[("1","COM"),("4","NC"),("3","NO")])
defbox('REG_5V','U',[("1","VIN"),("2","GND")],[("3","VOUT")])
defbox('MOD_P1','J',[("1","GPIO7 UP"),("2","GPIO9 DOWN"),("3","GPIO8 READY"),("4","GPIO22 LOCKUP"),("5","GPIO21 RELE"),("6","GPIO18 (LCD)"),("7","GPIO23 SOL1"),("8","GPIO11 SOL2"),("9","GPIO6 SOL3"),("10","GPIO5"),("11","GPIO20 (LCD)"),("12","GPIO19 (LCD)"),("13","GND"),("14","5V")],[])
defbox('MOD_P2','J',[("1","3V3"),("2","GND"),("3","GPIO28"),("4","GPIO29"),("5","GPIO13"),("6","GPIO12"),("7","GPIO2"),("8","GPIO1 CAN_TX"),("9","GPIO3"),("10","GPIO0 CAN_RX"),("11","GPIO4"),("12","GPIO10"),("13","GND"),("14","VBAT")],[])
defbox('CONN3','J',[("1","1"),("2","2"),("3","3")],[])
defbox('BAV99','D',[("3","K1/A2 (vstup)")],[("2","K2 (3V3)"),("1","A1 (GND)")])
defbox('SN65HVD230','U',[("1","D (TXD)"),("4","R (RXD)"),("8","RS"),("5","VREF")],[("3","VCC"),("2","GND"),("7","CANH"),("6","CANL")])
nano_left=[("1","D1/TX"),("2","D0/RX"),("3","RESET"),("4","GND"),("5","D2"),("6","D3"),("7","D4"),("8","D5"),("9","D6"),("10","D7"),("11","D8"),("12","D9"),("13","D10"),("14","D11"),("15","D12")]
nano_right=[("16","D13"),("17","3V3"),("18","AREF"),("19","A0"),("20","A1"),("21","A2"),("22","A3"),("23","A4"),("24","A5"),("25","A6"),("26","A7"),("27","5V"),("28","RESET"),("29","GND"),("30","VIN")]
defbox('ARDUINO_NANO','U',nano_left,nano_right)
LIB=[two_pin('R','R',GFX['R']),two_pin('C','C',GFX['C']),two_pin('CP','C',GFX['CP']),
     two_pin('D','D',GFX['D'],"K","A"),two_pin('LED','D',GFX['D'],"K","A"),two_pin('FUSE','F',GFX['FUSE']),two_pin('FERRITE','L',GFX['FERRITE'])]
for k,(l,r) in BOX.items(): LIB.append(box_sym(k,BOX_REF if False else {'CONN2':'J','CONN4':'J','CONN6':'J','CONN7':'J','PFET':'Q','NPN':'Q','PC817':'U','RELAY_SPDT':'K','REG_5V':'U','MOD_P1':'J','MOD_P2':'J','CONN3':'J','SN65HVD230':'U','BAV99':'D','ARDUINO_NANO':'U'}[k],l,r))

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
# lcsc: LCSC/JLC part number (osadí JLC); hand=True: vývodový díl, pájí se ručně
def add(sym,val,fp,x,y,nets,pref,dnp=False,note="",ref=None,lcsc="",mpn="",hand=False):
    ref=ref or newref(pref)
    x=round(round(x/2.54)*2.54,2); y=round(round(y/2.54)*2.54,2)
    parts.append(dict(ref=ref,sym=sym,val=val,fp=fp,x=x,y=y,nets=nets,dnp=dnp,note=note,uuid=U(),lcsc=lcsc,mpn=mpn,hand=hand)); return ref
def text(x,y,t,size=2.5):
    texts.append((x,y,t,size))
R0805="Resistor_SMD:R_0805_2012Metric"; C0805="Capacitor_SMD:C_0805_2012Metric"
SOD123="Diode_SMD:D_SOD-123"; SMA="Diode_SMD:D_SMA"; SOT23="Package_TO_SOT_SMD:SOT-23-3"; DPAK="Package_TO_SOT_SMD:TO-252-2"
OPTO="Package_DIP:SMDIP-4_W9.53mm"
RES={"100k":("C149504","0805W8F1003T5E"),"0R":("C17477","0805W8F0000T5E"),"6k8":("C17772","0805W8F6801T5E"),"10k":("C17414","0805W8F1002T5E"),
     "330":("C17630","0805W8F3300T5E"),"2k2":("C17520","0805W8F2201T5E"),"4k7":("C17673","0805W8F4701T5E"),"47k":("C17713","0805W8F4702T5E"),
     "470":("C17710","0805W8F4700T5E"),"1k":("C17513","0805W8F1001T5E"),"22k":("C17560","0805W8F2202T5E"),"120":("C17437","0805W8F1200T5E")}
def res(val,x,y,a,b,note="",ref=None): return add('R',val,R0805,x,y,{"1":a,"2":b},"R",note=note,ref=ref,lcsc=RES[val][0],mpn=RES[val][1])
def cap(val,x,y,a,b,ref=None): return add('C',val,C0805,x,y,{"1":a,"2":b},"C",ref=ref,lcsc="C49678",mpn="CC0805KRX7R9BB104")
DI={"MMSZ5227B":(SOD123,"C22395553"),"MMSZ5242B":(SOD123,"C19077426"),"MMSZ5245B":(SOD123,"C19077428"),"1N4148W":(SOD123,"C81598"),
    "M7":(SMA,"C95872"),"SS14":(SMA,"C2480"),"SMBJ24A":("Diode_SMD:D_SMB","C123819")}
def diode(val,x,y,k,a,note="",ref=None): return add('D',val,DI[val][0],x,y,{"1":k,"2":a},"D",note=note,ref=ref,lcsc=DI[val][1],mpn=val)
NPN_=("MMBT2222A",SOT23,"C8512"); PFET_=("AOD409",DPAK,"C36220"); PC817_=("LTV-817S-TA1-C",OPTO,"C109227")
def npn(x,y,b,c,e,note="",ref=None): return add('NPN',NPN_[0],NPN_[1],x,y,{"1":b,"3":c,"2":e},"Q",note=note,ref=ref,lcsc=NPN_[2],mpn=NPN_[0])
def pfet(x,y,g,d,s_,note="",ref=None): return add('PFET',PFET_[0],PFET_[1],x,y,{"1":g,"2":d,"3":s_},"Q",note=note,ref=ref,lcsc=PFET_[2],mpn=PFET_[0])
def opto(x,y,a,k,c,e,ref): return add('PC817',"LTV-817S",PC817_[1],x,y,{"1":a,"2":k,"4":c,"3":e},"U",ref=ref,lcsc=PC817_[2],mpn=PC817_[0])
PT=lambda n:f"TerminalBlock_Phoenix:TerminalBlock_Phoenix_PT-1,5-{n}-3.5-H_1x{n:02d}_P3.50mm_Horizontal"

# --- Power ---
text(20,25,"NAPÁJENÍ: +12V → F1 → Q1 (ochrana proti přepólování) → +12V_SW; TSR 1-2450 → +5V; PGND ↔ GND jen přes R2 0R (hvězda)",3)
add('CONN2',"J1 +12V / PGND",PT(2),40,50,{"1":"+12V_IN","2":"PGND"},"J",ref="J1",hand=True,mpn="Phoenix PT 1,5/2-3,5-H (1984617)")
add('FUSE',"5A","Fuse:Fuseholder_TR5_Littelfuse_No560_No460",75,50,{"1":"+12V_IN","2":"+12V_FUSED"},"F",note="pojistka TR5 5 A (T) v patici TR5",ref="F1",hand=True,mpn="Littelfuse 56000001009 + 37215000411")
pfet(110,50,"Q0_G","+12V_FUSED","+12V_SW",note="ochrana proti přepólování (D na vstupu, S na zátěži)",ref="Q1")
res("100k",150,50,"Q0_G","PGND",ref="R1")
diode("MMSZ5245B",170,50,"+12V_AUX","Q0_G",note="zener 15 V gate-source Q1",ref="D1")
add('D',"SMBJ24A",DI["SMBJ24A"][0],190,50,{"1":"+12V_SW","2":"PGND"},"D",note="TVS 600 W (load dump), VBR 26,7-29,5 V; pájí se ručně",ref="D2",lcsc="C123819",mpn="SMBJ24A",hand=True)
add('CP',"470uF 25V","Capacitor_SMD:CP_Elec_10x10.5",210,50,{"1":"+12V_SW","2":"PGND"},"C",note="SMD elektrolyt, pájí se ručně",ref="C1",lcsc="C248607",mpn="EEE-FK1E471P",hand=True)
cap("100nF",230,50,"+12V_AUX","PGND",ref="C2")
add('R',"0R","Resistor_SMD:R_1206_3216Metric",250,50,{"1":"+12V_SW","2":"+12V_AUX"},"R",note="odbočka +12 V pro cívky relé, měnič a gate (jen kvůli šířkám cest)",ref="R29",lcsc="C17888",mpn="1206W4F0000T5E")
add('REG_5V',"TSR 1-2450 (12V->5V 1A)","Converter_DCDC:Converter_DCDC_TRACO_TSR-1_THT",275,50,{"1":"+12V_AUX","2":"GND","3":"+5V_BUCK"},"U",note="spínaný stabilizátor ve formátu 7805, vstup 6,5-36 V",ref="U1",hand=True,mpn="TSR 1-2450")
add('C',"10uF 25V",C0805,300,50,{"1":"+12V_AUX","2":"GND"},"C",ref="C3",lcsc="C15850",mpn="CL21A106KAYNNNE")
res("0R",320,50,"PGND","GND",note="hvězda PGND-GND (jediné spojení zemí)",ref="R2")
add('FERRITE',"120R@100MHz 2A","Inductor_SMD:L_0603_1608Metric",345,50,{"1":"+5V_BUCK","2":"+5V"},"L",ref="L1",lcsc="C14709",mpn="BLM18PG121SN1D")
add('C',"100uF 6.3V","Capacitor_SMD:C_1206_3216Metric",370,50,{"1":"+5V","2":"GND"},"C",ref="C4",lcsc="C15008",mpn="CL31A107MQHNNNE")
diode("SS14",405,50,"MOD_5V","+5V",note="5 V do modulu přes Schottky – USB modulu a napájení desky se nepřetahují (JP1 už není potřeba)",ref="D7")

# --- RP2350-Touch-LCD-2 module + connectors ---
text(20,95,"MODUL RP2350-Touch-LCD-2 (zasunutý do J7 = lišta P1, J8 = lišta P2) a konektory",3)
SOCK="Connector_PinSocket_2.54mm:PinSocket_1x14_P2.54mm_Vertical"
add('MOD_P1',"P1 modulu RP2350-Touch-LCD-2",SOCK,60,140,{"1":"UP_D","2":"DOWN_D","3":"READY_D","4":"LOCKUP_D","5":"K_CTRL","7":"SOL1_CTRL","8":"SOL2_CTRL","9":"SOL3_CTRL","10":"GPIO5_SPARE","13":"GND","14":"MOD_5V"},"J",note="dutinková lišta 1x14 pro lištu P1 modulu; piny 6, 11, 12 = displej modulu, nezapojovat",ref="J7",hand=True,mpn="Sullins PPTC141LFBN-RC")
add('MOD_P2',"P2 modulu RP2350-Touch-LCD-2",SOCK,120,140,{"1":"+3V3_MOD","2":"GND","8":"CAN_TX","10":"CAN_RX","13":"GND"},"J",note="dutinková lišta 1x14 pro lištu P2 modulu",ref="J8",hand=True,mpn="Sullins PPTC141LFBN-RC")
add('CONN3',"J6 rezerva GPIO5 / 3V3 / GND","Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",165,140,{"1":"GPIO5_SPARE","2":"+3V3_MOD","3":"GND"},"J",ref="J6",hand=True,mpn="kolíková lišta 1x3 2,54 mm")
add('CONN6',"J2 SOL1-3 / TCU1-3",PT(6),195,140,{"1":"SOL1_OUT","2":"SOL2_OUT","3":"SOL3_OUT","4":"TCU1","5":"TCU2","6":"TCU3"},"J",note="1-3 k převodovce, 4-6 z původní TCU",ref="J2",hand=True,mpn="Phoenix PT 1,5/6-3,5-H (1984659)")
add('CONN6',"J3 UP DOWN READY LOCKUP",PT(6),255,140,{"1":"+5V","2":"GND","3":"UP_IN","4":"DOWN_IN","5":"READY_IN","6":"LOCKUP_IN"},"J",ref="J3",hand=True,mpn="Phoenix PT 1,5/6-3,5-H (1984659)")

# --- Inputs ---
text(20,172,"VSTUPY: 5 V spínače -> 3,3 V logika: dělič 6k8 / 10k (~3,0 V), 100nF, BAV99 svede přepětí do 3V3 / GND",3)
x=40
for i,nm in enumerate(("UP","DOWN","READY","LOCKUP")):
    res("6k8",x,197,nm+"_IN",nm+"_D",ref=f"R{3+2*i}")
    res("10k",x+24,197,nm+"_D","GND",ref=f"R{4+2*i}")
    cap("100nF",x+48,197,nm+"_D","GND",ref=f"C{5+i}")
    add('BAV99',"BAV99",SOT23,x+72,197,{"1":"GND","2":"+3V3_MOD","3":nm+"_D"},"D",note="ochranné diody vstupu do 3V3 a GND",ref=f"D{3+i}",lcsc="C2500",mpn="BAV99,215")
    x+=96

# --- Relay coil drive ---
text(20,232,"CÍVKY RELÉ K1-K3 (společné): READY přepínač AND GPIO21 (aktivní HIGH přes Q6)",3)
res("330",40,258,"READY_IN","RLY_A",ref="R13")
opto(75,258,"RLY_A","RLY_K","RLY_C","Q4_B",ref="U3")
res("2k2",110,258,"+12V_AUX","RLY_C",ref="R14")
res("10k",130,258,"Q4_B","PGND",ref="R15")
npn(160,258,"Q4_B","K_LOW","PGND",note="spíná cívky K1-K3 (3x 33 mA)",ref="Q2")
diode("1N4148W",190,258,"+12V_AUX","K_LOW",note="flyback cívek",ref="D9")
res("2k2",210,258,"+12V_AUX","LED_A",ref="R16")
add('LED',"zelená READY","LED_SMD:LED_0805_2012Metric",230,258,{"1":"K_LOW","2":"LED_A"},"D",ref="D10",lcsc="C2297",mpn="KT-0805G")
npn(280,258,"Q6_B","RLY_K","GND",note="spíná LED optočlenu U3 z GPIO21 (3,3 V)",ref="Q6")
res("4k7",255,258,"K_CTRL","Q6_B",ref="R26")
res("47k",305,258,"Q6_B","GND",note="bez modulu drží Q6 zavřený (relé odpadlá)",ref="R27")

# --- Solenoid channels ---
text(20,292,"VÝSTUPNÍ KANÁLY SOLENOIDŮ (high-side): opto → P-MOSFET → NO kontakt relé; COM → solenoid; NC → původní TCU",3)
for n in (1,2,3):
    xb=30+(n-1)*180
    yy=318
    rb=17+3*(n-1)
    res("470",xb,yy,f"SOL{n}_CTRL",f"S{n}_LEDA",ref=f"R{rb}")
    opto(xb+28,yy+25,f"S{n}_LEDA","GND",f"S{n}_C","PGND",ref=f"U{3+n}")
    res("1k",xb+52,yy,f"S{n}_C",f"G{n}",ref=f"R{rb+1}")
    res("22k",xb+70,yy,"+12V_AUX",f"G{n}",note="gate pull-up (22k: optočlen stačí ~0,5 mA i z 3,3 V GPIO)",ref=f"R{rb+2}")
    diode("MMSZ5245B",xb+88,yy,"+12V_AUX",f"G{n}",note="zener 15 V gate-source",ref=f"D{9+2*n}")
    pfet(xb+118,yy+25,f"G{n}",f"SOL{n}_NO","+12V_SW",ref=f"Q{2+n}")
    add('RELAY_SPDT',"G5LE-1 12V SPDT","Relay_THT:Relay_SPDT_Omron-G5LE-1",xb+158,yy+25,{"2":"+12V_AUX","5":"K_LOW","1":f"SOL{n}_OUT","4":f"TCU{n}","3":f"SOL{n}_NO"},"K",note="čísla pinů relé ověřit podle footprintu",ref=f"K{n}",hand=True,mpn="Omron G5LE-1 DC12")
    diode("M7",xb+158,yy+60,f"SOL{n}_OUT","PGND",note="flyback na straně solenoidu",ref=f"D{10+2*n}")

# --- CAN (rusEFI) ---
text(20,392,"CAN (rusEFI): SN65HVD230 (3,3 V z modulu), TX/RX = GPIO1/GPIO0 (P2), RS na GND = plná rychlost; zakončení 120R přes JP2",3)
add('SN65HVD230',"SN65HVD230DR","Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",60,415,{"1":"CAN_TX","4":"CAN_RX","8":"GND","3":"+3V3_MOD","2":"GND","7":"CANH","6":"CANL"},"U",note="CAN budič 3,3 V; VREF (5) nezapojeno",ref="U7",lcsc="C12084",mpn="SN65HVD230DR")
cap("100nF",110,415,"+3V3_MOD","GND",ref="C9")
res("120",160,415,"CANH","CAN_TERM",note="zakončení sběrnice (osadit JP2, jen když je deska na konci sběrnice)",ref="R28")
add('CONN2',"JP2 zakončení CAN 120R","Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",190,415,{"1":"CAN_TERM","2":"CANL"},"J",ref="JP2",hand=True,mpn="kolíková lišta 1x2 + jumper")
add('CONN2',"J4 CAN-H / CAN-L",PT(2),230,415,{"1":"CANH","2":"CANL"},"J",ref="J4",hand=True,mpn="Phoenix PT 1,5/2-3,5-H (1984617)")

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
         f'(title_block (title "AW4 shield – ovladač automatické převodovky Aisin AW-4") (rev "0.5") (comment 1 "RP2350-Touch-LCD-2 modul, 3x SPDT relé, 3x high-side P-MOSFET, CAN; SMD osazuje JLC")) ')
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
      f'(property "LCSC" "{esc(p["lcsc"])}" (at {X:g} {Y:g} 0) {fp_eff}) '
      f'(property "MPN" "{esc(p["mpn"])}" (at {X:g} {Y:g} 0) {fp_eff}) '
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
    w.writerow(["MODUL",1,"Waveshare RP2350-Touch-LCD-2","(zasunuto do J7/J8)","řídicí modul s displejem"])
