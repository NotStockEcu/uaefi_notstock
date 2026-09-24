# Assign net classes, fill zones and export Specctra DSN for Freerouting.
import pcbnew
MM = pcbnew.FromMM
b = pcbnew.LoadBoard('/prj/PDM.kicad_pcb')
ns = b.GetDesignSettings().m_NetSettings
def cls(name, w, clr, vd, vdr, prio):
    c = pcbnew.NETCLASS(name); c.SetTrackWidth(MM(w)); c.SetClearance(MM(clr))
    c.SetViaDiameter(MM(vd)); c.SetViaDrill(MM(vdr)); c.SetPriority(prio); ns.SetNetclass(name, c)
cls('Supply', 0.6, 0.2, 0.6, 0.3, 1)
cls('HighCurrent', 1.5, 0.2, 0.6, 0.3, 0)
ns.ClearNetclassPatternAssignments()
for p in ['/VKEY', '/VKEY_RAW', '/+5V', '/+5V_DISP', '/SW5', '/GND', '/LAMP', '/PGND*', '/VCC5']:
    ns.SetNetclassPatternAssignment(p, 'Supply')
for p in ['/VBAT', '/OUT*']:
    ns.SetNetclassPatternAssignment(p, 'HighCurrent')
ns.ClearAllCaches(); b.SynchronizeNetsAndNetClasses(True)
print('VKEY ->', b.FindNet('/VKEY').GetNetClassName(), ' OUT1 ->', b.FindNet('/OUT1').GetNetClassName())
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard('/prj/PDM.kicad_pcb', b)
# copper fills on the signal layers are poured after routing, the router only sees planes + output copper
for z in list(b.Zones()):
    if z.GetZoneName() in ('GND_FILL', 'VBAT_B') and z.GetLayer() in (pcbnew.F_Cu, pcbnew.B_Cu): b.Remove(z)
print('export', pcbnew.ExportSpecctraDSN(b, '/prj/PDM.dsn'))
# inner layers are the GND / VBAT planes: mark them as power layers so the router keeps signals off them
s = open('/prj/PDM.dsn').read()
for l in ('In2.Cu',):                          # In1 (GND) may carry signals, In2 (VBAT) stays solid
    s = s.replace(f'(layer {l}\n      (type signal)', f'(layer {l}\n      (type power)')
open('/prj/PDM.dsn', 'w').write(s)
