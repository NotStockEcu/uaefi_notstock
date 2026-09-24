# Import the Freerouting session back, refill zones, save.
import pcbnew
b = pcbnew.LoadBoard('/prj/PDM.kicad_pcb')
print('import', pcbnew.ImportSpecctraSES(b, '/prj/PDM.ses'))
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard('/prj/PDM.kicad_pcb', b)
print('tracks', len(b.GetTracks()))
