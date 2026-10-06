import json,subprocess,re,sys
PCB="/opt/kicad10/rootfs/work/pcb/aw4_shield.kicad_pcb"
def drc():
    subprocess.run(["chroot","/opt/kicad10/rootfs","/bin/sh","-c","cd /work/pcb && HOME=/tmp kicad-cli pcb drc --schematic-parity --severity-all --format json -o drc.json aw4_shield.kicad_pcb >/dev/null 2>&1"])
    return json.load(open("/opt/kicad10/rootfs/work/pcb/drc.json"))
def blocks(s,kw):
    i=0
    while True:
        i=s.find("("+kw,i)
        if i<0: return
        d=0
        for j in range(i,len(s)):
            if s[j]=="(": d+=1
            elif s[j]==")":
                d-=1
                if d==0: break
        yield i,j+1; i=j+1
for it in range(8):
    r=drc()
    dang=set()
    for v in r.get("violations",[]):
        if v["type"] in ("track_dangling","via_dangling"):
            for item in v["items"]: dang.add(item["uuid"])
    if not dang: break
    s=open(PCB).read(); out=[]; last=0; n=0
    for kw in ("segment","via"):
        pass
    spans=[]
    for kw in ("segment","via"):
        for a,b in blocks(s,kw):
            m=re.search(r'\(uuid "([^"]+)"\)',s[a:b])
            if m and m.group(1) in dang: spans.append((a,b))
    spans.sort()
    for a,b in spans: out.append(s[last:a]); last=b
    out.append(s[last:]); open(PCB,"w").write("".join(out))
    print("pass",it,"removed",len(spans))
    subprocess.run(["chroot","/opt/kicad10/rootfs","/bin/sh","-c","cd /work/pcb && HOME=/tmp python3 /work/refill.py aw4_shield.kicad_pcb >/dev/null 2>&1"])
r=drc()
from collections import Counter
print(Counter(v["type"] for v in r.get("violations",[])+r.get("schematic_parity",[]))," unconnected:",len(r.get("unconnected_items",[])))
