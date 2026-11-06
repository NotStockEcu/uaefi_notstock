import subprocess, re
MODELS='''
.model DLED D(Is=1e-18 N=1.8 Rs=2)
.model QOPTO NPN(BF=100 Is=1e-14 VAF=80)
.model QBC337 NPN(BF=150 Is=1.4e-14 VAF=74 Rb=10 Rc=1 Ikf=0.3)   ; MMBT2222A (v0.5 Q2, Q6)
.model D4148 D(Is=4e-9 N=1.9 Rs=0.6 BV=100)
.model DZ3V6 D(Is=1e-9 N=2.5 Rs=20 BV=3.6 IBV=5m)
.model DZ15 D(Is=1e-12 Rs=5 BV=15 IBV=5m)
.model IRF9540 VDMOS(pchan Vto=-2.4 Kp=20 Rd=0.02 Rs=0.01 Cgs=3.4n Cgdmax=1.2n Cgdmin=0.15n Rb=0.01 Is=1e-12)   ; AOD409 (v0.5 Q1, Q3-Q5)
'''
def opto(n,a,k,c,e,ctr):
    return f"D{n} {a} {n}m DLED\nV{n}s {n}m {k} 0\nF{n} 0 {n}b V{n}s {ctr/100.0}\nR{n}b {n}b 0 1e9\nQ{n} {c} {n}b {e} QOPTO\n"
def run(deck, prints):
    open("x.cir","w").write(deck+f".control\nop\nprint {prints}\n.endc\n.end\n")
    out=subprocess.run(["ngspice","-b","x.cir"],capture_output=True,text=True).stdout
    return dict(re.findall(r"^([^\s=]+(?:\([^)]*\))?(?:[-+*/][^\s=]+)*)\s*=\s*([-+0-9.eE]+)",out,re.M)), out
# GPIO model: 3.3 V source with ~60 ohm output resistance (RP2350, 4 mA drive setting -> conservative)
for VB in (12.0, 14.4):
  for CTR in (0.25, 0.5, 6.0):
    for GP in (3.3, 3.0):
        deck=f"* v0.5 checks\n{MODELS}\nVBAT sw 0 {VB}\nV5 v5 0 5\n"
        # input: switch on (5 V) -> 6k8 -> node, 10k pull-down, 3V6 zener, 100 nF
        deck+="R3 v5 in 6.8k\nR4 in 0 10k\nD3 0 in DZ3V6\n"
        # input: hypothetical 14.4 V on the input wire (miswiring) -> clamp check
        deck+=f"VX x14 0 14.4\nRX x14 inx 6.8k\nR4x inx 0 10k\nD3x 0 inx DZ3V6\n"
        # relay drive: READY_5V -> 330 -> LED U3 -> Q6 (base via 4k7 from GPIO, 47k pull-down) -> GND
        deck+=f"VG gpio 0 {GP}\nRG gpio gq 60\nR26 gq q6b 4.7k\nR27 q6b 0 47k\nQ6 rlyk q6b 0 QBC337\nR13 v5 ra 330\n"+opto("U3","ra","rlyk","rc","qb",CTR)
        deck+="R14 sw rc 2.2k\nR15 qb 0 10k\nQ2 kl qb 0 QBC337\nRCOIL sw kl 133\nD9 kl sw D4148\n"
        # solenoid channel: GPIO -> 470 -> LED U4 -> GND; opto pulls gate of IRF9540N
        deck+=f"VS gp2 0 {GP}\nRGS gp2 gs 60\nR17 gs la 470\n"+opto("U4","la","0","sc","0",CTR)
        deck+="R18 sc g1 1k\nR19 sw g1 22k\nD11 g1 sw DZ15\nM3 nod g1 sw IRF9540\nRSOL nod 0 12\n"
        vals,out=run(deck,"v(in) v(inx) v(kl) i(VU3s) i(VU4s) v(g1)-v(sw) i(VG) i(VS) v(nod)")
        g=lambda k: float(vals.get(k,'nan'))
        print(f"VBAT={VB:4} CTR={int(CTR*100):3}% GPIO={GP}V | vstup {g('v(in)'):.2f} V (pri 14,4 V na vodici {g('v(inx)'):.2f} V) | "
              f"Q2 sepnuty: V(civky)={g('v(kl)'):.2f} V, LED U3 {-g('i(vu3s)')*1e3 if g('i(vu3s)')<0 else g('i(vu3s)')*1e3:.1f} mA, proud z GPIO21 {abs(g('i(vg)'))*1e3:.2f} mA | "
              f"LED U4 {abs(g('i(vu4s)'))*1e3:.1f} mA, GPIO sol {abs(g('i(vs)'))*1e3:.2f} mA, Vgs Q3 {g('v(g1)-v(sw)'):.1f} V, solenoid {g('v(nod)'):.2f} V")
# relay OFF check: GPIO low -> Q6 off -> relay must stay off even with READY on
deck=f"* off\n{MODELS}\nVBAT sw 0 14.4\nV5 v5 0 5\nVG gpio 0 0\nR26 gpio q6b 4.7k\nR27 q6b 0 47k\nQ6 rlyk q6b 0 QBC337\nR13 v5 ra 330\n"+opto("U3","ra","rlyk","rc","qb",6.0)+"R14 sw rc 2.2k\nR15 qb 0 10k\nQ2 kl qb 0 QBC337\nRCOIL sw kl 133\nD9 kl sw D4148\n"
vals,out=run(deck,"v(kl) i(VU3s)")
print("GPIO21=0 V, READY zap, CTR 600 pct: V(civky K_LOW)=%.2f V (14,4 = rele odpadla), LED U3 %.3f mA" % (float(vals['v(kl)']), abs(float(vals['i(vu3s)']))*1e3))
# floating GPIO (module removed): base only via 47k -> must be off
deck=deck.replace("VG gpio 0 0\n","RFLOAT gpio 0 1e9\n")
vals,out=run(deck,"v(kl) i(VU3s)")
print("modul vyjmuty (GPIO21 plovouci): V(civky)=%.2f V, LED U3 %.4f mA" % (float(vals['v(kl)']), abs(float(vals['i(vu3s)']))*1e3))
