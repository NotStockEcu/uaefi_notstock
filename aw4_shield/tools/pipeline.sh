#!/bin/bash
# Full board pipeline: schematic -> ERC -> PCB placement -> Freerouting -> zones/cleanup -> DRC -> exports.
# Requires: KiCad 10 rootfs in /opt/kicad10/rootfs (chroot), Freerouting jar in /opt/fr. SKIP_ROUTE=1 reuses /opt/fr/aw4.ses.
set -e
T=$(cd "$(dirname "$0")" && pwd); K=$T/../kicad; W=/opt/kicad10/rootfs/work
SP=/tmp/claude-0/-home-user-uaefi-notstock/e6f7c8f6-6e72-54f1-b6cf-eda4a168ecbb/scratchpad
mkdir -p $W/pcb
(cd $K && python3 $T/kgen.py | tail -1)
cp $SP/parts.json $W/
cp $T/build_pcb.py $T/finish_pcb.py $T/dsn.py $T/refill.py $T/export.sh $T/sexp.py $W/
cp $K/aw4_shield.kicad_sch $K/AW4.kicad_sym $K/sym-lib-table $K/aw4_shield.kicad_pro $W/pcb/
chroot /opt/kicad10/rootfs /bin/sh -c 'cd /work/pcb && export HOME=/tmp; kicad-cli sch erc --severity-all -o erc.rpt aw4_shield.kicad_sch >/dev/null; grep "ERC messages" erc.rpt; kicad-cli sch export netlist --format kicadsexpr -o net.net aw4_shield.kicad_sch; python3 /work/build_pcb.py /work/pcb/aw4_shield.kicad_pcb | tail -1; python3 /work/dsn.py aw4_shield.kicad_pcb /work/pcb/aw4.dsn'
if [ -z "$SKIP_ROUTE" ]; then
  cd /opt/fr && cp $W/pcb/aw4.dsn . && rm -f aw4.ses && (timeout 1500 java -jar freerouting.jar -de aw4.dsn -do aw4.ses -mp ${MP:-60} --gui.enabled=false > route.log 2>&1 || true)
  grep -o '"incomplete_count": [0-9]*' route.log | tail -1
fi
cp /opt/fr/aw4.ses $W/pcb/
chroot /opt/kicad10/rootfs /bin/sh -c 'cd /work/pcb && export HOME=/tmp; python3 /work/finish_pcb.py aw4_shield.kicad_pcb aw4.ses | tail -2'
python3 $T/cleanup.py
chroot /opt/kicad10/rootfs /work/export.sh
echo PIPELINE_DONE
