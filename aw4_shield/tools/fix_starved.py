# Pads with an incomplete thermal relief (they are connected by tracks): exclude them from the pour.
# Usage (inside the KiCad chroot): fix_starved.py board.kicad_pcb uuid1 uuid2 ...
import pcbnew, sys
b = pcbnew.LoadBoard(sys.argv[1]); want = set(sys.argv[2:]); n = 0
for fp in b.GetFootprints():
    for pad in fp.Pads():
        if pad.m_Uuid.AsString() in want:
            pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_NONE); n += 1
            print("no zone connection:", fp.GetReference(), pad.GetNumber(), pad.GetNetname())
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard(sys.argv[1], b)
print("pads changed:", n)
