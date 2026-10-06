#!/bin/bash
set -e
SP=/tmp/claude-0/-home-user-uaefi-notstock/e6f7c8f6-6e72-54f1-b6cf-eda4a168ecbb/scratchpad
K=/home/user/uaefi_notstock/aw4_shield/kicad
W=/opt/kicad10/rootfs/work
MP=${1:-60}
cd $K && python3 $SP/kgen.py | tail -1
cp $SP/parts.json $W/
cp $K/aw4_shield.kicad_sch $K/AW4.kicad_sym $K/sym-lib-table $W/pcb/
cp $K/aw4_shield.kicad_pro $W/pcb/aw4_shield.kicad_pro
chroot /opt/kicad10/rootfs /bin/sh -c 'cd /work/pcb && HOME=/tmp kicad-cli sch erc -o erc.rpt aw4_shield.kicad_sch | tail -1; HOME=/tmp kicad-cli sch export netlist --format kicadsexpr -o net.net aw4_shield.kicad_sch; HOME=/tmp python3 /work/build_pcb.py /work/pcb/aw4_shield.kicad_pcb && HOME=/tmp python3 /work/dsn.py aw4_shield.kicad_pcb /work/pcb/aw4.dsn'
cd $SP/fr
if [ -z "$SKIP_ROUTE" ]; then cp $W/pcb/aw4.dsn . && rm -f aw4.ses && timeout 1200 java -jar freerouting.jar -de aw4.dsn -do aw4.ses -mp $MP --gui.enabled=false > route.log 2>&1 || true; fi
grep -o '"incomplete_count": [0-9]*' route.log | tail -1
cp aw4.ses $W/pcb/
chroot /opt/kicad10/rootfs /bin/sh -c 'cd /work/pcb && HOME=/tmp python3 /work/finish_pcb.py aw4_shield.kicad_pcb aw4.ses && HOME=/tmp kicad-cli pcb drc --schematic-parity --severity-all -o drc.rpt aw4_shield.kicad_pcb | tail -2; grep -o "^\[[a-z_]*\]" drc.rpt | sort | uniq -c'
echo PIPELINE_DONE
