# Přidá do kopie desky 3D model modulu s displejem (jen pro rendery) – spouští se v chrootu s KiCadem.
# render_hero.py vstup.kicad_pcb výstup.kicad_pcb cesta_k_modelu.wrl
import pcbnew, sys
b = pcbnew.LoadBoard(sys.argv[1])
fp = pcbnew.FOOTPRINT(b)
fp.SetReference("MOD1"); fp.Reference().SetVisible(False); fp.Value().SetVisible(False)
fp.SetPosition(pcbnew.VECTOR2I_MM(118.0, 124.1))   # střed modulu (x 100.5-135.5, y 100-148.2)
fp.SetExcludedFromBOM(True); fp.SetExcludedFromPosFiles(True)
fp.SetAttributes(fp.GetAttributes() | pcbnew.FP_EXCLUDE_FROM_POS_FILES)
m = pcbnew.FP_3DMODEL(); m.m_Filename = sys.argv[3]
fp.Add3DModel(m); b.Add(fp)
pcbnew.SaveBoard(sys.argv[2], b)
print("ok")
