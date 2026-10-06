import subprocess, re, itertools
MODELS='''
.model DLED D(Is=1e-18 N=1.8 Rs=2)
.model QOPTO NPN(BF=100 Is=1e-14 VAF=80)
.model QBC337 NPN(BF=250 Is=2e-14 VAF=100 Rb=10 Rc=0.5 Ikf=0.8)
.model D4007 D(Is=7e-9 N=1.9 Rs=0.04 BV=1000)
.model D4148 D(Is=4e-9 N=1.9 Rs=0.6 BV=100)
.model DZ15 D(Is=1e-12 Rs=5 BV=15 IBV=5m)
.model DZ12 D(Is=1e-12 Rs=5 BV=12 IBV=5m)
.model DZ5V6 D(Is=1e-12 Rs=10 BV=5.6 IBV=5m)
.model DTVS D(Is=1e-12 Rs=0.05 BV=25.7 IBV=1m)
.model IRF9540 VDMOS(pchan Vto=-3.2 Kp=8 Rd=0.06 Rs=0.03 Cgs=1.3n Cgdmax=0.9n Cgdmin=0.15n Rb=0.02 Is=1e-12)
.model IRF4905 VDMOS(pchan Vto=-3.0 Kp=40 Rd=0.008 Rs=0.006 Cgs=3n Cgdmax=1.5n Cgdmin=0.3n Rb=0.01 Is=1e-12)
'''
def opto(name,a,k,c,e,ctr):
    # LED a->k with sense source, transistor c->e with base current = CTR/BF * I_led
    return f'''D{name} {a} {name}m DLED
V{name}s {name}m {k} 0
F{name} 0 {name}b V{name}s {ctr/100.0}
R{name}b {name}b 0 1e9
Q{name} {c} {name}b {e} QOPTO
'''
def run(deck):
    open("x.cir","w").write(deck)
    out=subprocess.run(["ngspice","-b","x.cir"],capture_output=True,text=True).stdout
    return {m.group(1):float(m.group(2)) for m in re.finditer(r"^(\w+)\s*=\s*([-+0-9.eE]+)",out,re.M)}, out
res=[]
for VB,CTR in itertools.product([9,12,14.4,27],[0.5,6.0]):
    deck=f'''* AW4 channel + relay driver
{MODELS}
VBAT bat 0 {VB}
* reverse-protection Q1 (drain=in, source=+12V_SW), gate via 100k to PGND, zener 12V G-S
M1 bat g0 sw IRF4905
R1 g0 0 100k
D1 g0 sw DZ12
* 5V logic
V5 v5 0 5
* ---- solenoid channel: D9 HIGH -> 330R -> opto LED -> GND
VD9 d9 0 5
R17 d9 la 330
{opto("U4","la","0","sc","0",CTR)}R18 sc g1 1k
R19 sw g1 10k
D11 g1 sw DZ15
M3 nod g1 sw IRF9540
* relay NO closed -> COM -> solenoid 12R+15mH to chassis
RK nod com 0.05
LSOL com sm 15m
RSOL sm 0 12
D12 0 com D4007
* ---- relay driver: READY_5V -> 330R -> LED -> D8(LOW)
VRDY rdy 0 5
VD8 d8 0 0
R13 rdy ra 330
{opto("U3","ra","d8","rc","qb",CTR)}R14 sw rc 2.2k
R15 qb 0 10k
Q2 kl qb 0 QBC337
RCOIL sw kl 133
D9 kl sw D4148
R16 sw la2 2.2k
DLED la2 kl DLED
* ---- input filter: switch on (5V) -> 1k -> node, 10k pulldown, 5V6 zener
R3 v5 in 1k
R4 in 0 10k
D3 0 in DZ5V6
.control
op
print v(sw) v(g1)-v(sw) i(VBAT) v(com) v(kl) v(qb) v(in) v(sw)-v(g0)
print i(vu4s) i(vu3s) v(sw)-v(rc) v(sw)-v(nod)
.endc
.end
'''
    vals,out=run(deck)
    nums=re.findall(r"=\s*([-+0-9.eE]+)",out.split("Operating point")[-1] if "Operating point" in out else out)
    lines=[l for l in out.splitlines() if "=" in l and ("v(" in l or "i(" in l or "/2200" in l)]
    print(f"--- VBAT={VB} V, CTR={int(CTR*100)} %")
    for l in lines: print("   ",l.strip())
