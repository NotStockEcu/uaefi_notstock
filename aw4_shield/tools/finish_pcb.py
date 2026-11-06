import pcbnew, sys
MM=pcbnew.FromMM
b=pcbnew.LoadBoard(sys.argv[1])
ok=pcbnew.ImportSpecctraSES(b, sys.argv[2]); print("import SES", ok)
b.GetDesignSettings().m_CopperEdgeClearance=MM(0.3)   # pravidlo výrobce (JLCPCB)
def zone(netname, layer, pts, prio=0):
    z=pcbnew.ZONE(b); z.SetLayer(layer); z.SetNet(b.FindNet("/"+netname))
    ol=z.Outline(); ol.NewOutline()
    for x,y in pts: ol.Append(MM(x),MM(y))
    z.SetLocalClearance(MM(0.4)); z.SetMinThickness(MM(0.3)); z.SetAssignedPriority(prio)
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL); z.SetThermalReliefGap(MM(0.4)); z.SetThermalReliefSpokeWidth(MM(0.6))
    b.Add(z); return z
# v0.5: PGND (výkonová část vpravo a dole) / GND (logika pod modulem a vlevo dole), spojené jen přes R2
zone("PGND", pcbnew.B_Cu, [(156.5,100.5),(199.5,100.5),(199.5,199.5),(132.8,199.5),(132.8,156),(156.5,156)])
zone("GND",  pcbnew.B_Cu, [(100.5,100.5),(155.9,100.5),(155.9,155.4),(132.2,155.4),(132.2,199.5),(100.5,199.5)])
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard(sys.argv[1], b)
tr=[t for t in b.GetTracks()]
print("tracks", sum(1 for t in tr if t.GetClass()=="PCB_TRACK"), "vias", sum(1 for t in tr if t.GetClass()=="PCB_VIA"))
