#!/bin/sh
cd /work/pcb
export HOME=/tmp
rm -f gerb2/* aw4_shield_gerber.zip
mkdir -p gerb2
kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,Edge.Cuts -o gerb2/ aw4_shield.kicad_pcb >/dev/null
kicad-cli pcb export drill -o gerb2/ aw4_shield.kicad_pcb >/dev/null
kicad-cli sch export pdf -o aw4_shield_schema_kicad.pdf aw4_shield.kicad_sch >/dev/null
kicad-cli pcb export pdf --mode-multipage --layers F.Cu,B.Cu,F.SilkS,F.Fab --common-layers Edge.Cuts -o aw4_shield_pcb.pdf aw4_shield.kicad_pcb >/dev/null
kicad-cli pcb render --side top -w 1800 --height 1300 --quality high -o pcb_top.png aw4_shield.kicad_pcb >/dev/null
kicad-cli pcb render --side bottom -w 1800 --height 1300 --quality high -o pcb_bottom.png aw4_shield.kicad_pcb >/dev/null
kicad-cli pcb render --perspective --rotate "-40,0,20" --zoom 1.1 -w 1800 --height 1300 --quality high -o pcb_3d.png aw4_shield.kicad_pcb >/dev/null
kicad-cli pcb drc --schematic-parity --severity-all -o drc.rpt aw4_shield.kicad_pcb | tail -3
kicad-cli sch erc --severity-all -o erc.rpt aw4_shield.kicad_sch >/dev/null; grep "ERC messages" erc.rpt
echo EXPORT_DONE
