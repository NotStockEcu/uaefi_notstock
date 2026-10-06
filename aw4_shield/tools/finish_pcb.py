import pcbnew, sys
MM=pcbnew.FromMM
b=pcbnew.LoadBoard(sys.argv[1])
ok=pcbnew.ImportSpecctraSES(b, sys.argv[2]); print("import SES", ok)
# GND pins of the Nano are connected by tracks; keep them out of the pour (avoids starved thermals)
for fp in b.GetFootprints():
    if fp.GetReference()=="U2":
        for pad in fp.Pads():
            if pad.GetNetname()=="/GND": pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_NONE)
def zone(netname, layer, pts, prio=0):
    z=pcbnew.ZONE(b); z.SetLayer(layer); z.SetNet(b.FindNet("/"+netname))
    ol=z.Outline(); ol.NewOutline()
    for x,y in pts: ol.Append(MM(x),MM(y))
    z.SetLocalClearance(MM(0.4)); z.SetMinThickness(MM(0.3)); z.SetAssignedPriority(prio)
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL); z.SetThermalReliefGap(MM(0.4)); z.SetThermalReliefSpokeWidth(MM(0.6))
    b.Add(z); return z
zone("PGND", pcbnew.B_Cu, [(100.5,100.5),(177.5,100.5),(177.5,199.5),(100.5,199.5)])
zone("GND",  pcbnew.B_Cu, [(179,119),(244.5,119),(244.5,199.5),(179,199.5)])
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard(sys.argv[1], b)
tr=[t for t in b.GetTracks()]
print("tracks", sum(1 for t in tr if t.GetClass()=="PCB_TRACK"), "vias", sum(1 for t in tr if t.GetClass()=="PCB_VIA"))
