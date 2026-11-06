# AW4 shield v0.5 – podklady k objednání:
#   fab/jlc_bom.csv, fab/jlc_cpl.csv  ... osazení SMD v JLCPCB (BOM + pozice)
#   bom_objednavka.xlsx               ... listy: JLC SMD, Ruční osazení, Postup, DigiKey
#   digikey_bom.csv                   ... ručně pájené díly pro DigiKey BOM Manager
# Použití: make_bom.py parts.json pozice.csv výstupní_složka
import json, csv, re, sys, collections, urllib.request
from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter

parts = json.load(open(sys.argv[1]))["parts"]
POS, OUT = sys.argv[2], sys.argv[3].rstrip("/") + "/"

def refkey(r):
    m = re.match(r"([A-Z]+)(\d+)", r); return (m.group(1), int(m.group(2))) if m else (r, 0)

# ---------- JLC: info o dílech (typ knihovny, sklad) ----------
def jlc_info(code):
    try:
        body = json.dumps({"keyword": code, "currentPage": 1, "pageSize": 5}).encode()
        req = urllib.request.Request("https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList",
                                     data=body, headers={"Content-Type": "application/json"})
        for x in json.load(urllib.request.urlopen(req, timeout=30))["data"]["componentPageInfo"]["list"] or []:
            if x["componentCode"] == code:
                t = {"base": "Basic", "expand": "Extended"}.get(x["componentLibraryType"], x["componentLibraryType"])
                if t == "Extended" and x.get("preferredComponentFlag"): t = "Preferred ext."
                return t, x["stockCount"], x["componentBrandEn"], x["describe"]
    except Exception as e:
        print("JLC dotaz selhal", code, e)
    return "?", None, "", ""

smt = [p for p in parts if not p.get("hand")]
hand = [p for p in parts if p.get("hand")]
grp = collections.OrderedDict()
for p in sorted(smt, key=lambda p: refkey(p["ref"])):
    grp.setdefault((p["val"], p["fp"], p["lcsc"], p["mpn"]), []).append(p["ref"])

# ---------- fab/jlc_bom.csv ----------
info = {}
with open(OUT + "fab/jlc_bom.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    for (val, fp, lcsc, mpn), refs in grp.items():
        w.writerow([f"{val} ({mpn})" if mpn and mpn != val else val, ",".join(refs), fp.split(":")[1], lcsc])
        info[lcsc] = jlc_info(lcsc)

# ---------- fab/jlc_cpl.csv (z kicad-cli pcb export pos --smd-only) ----------
smt_refs = {p["ref"] for p in smt}
rows = list(csv.DictReader(open(POS)))
with open(OUT + "fab/jlc_cpl.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    seen = set()
    for r in rows:
        if r["Ref"] not in smt_refs: continue
        seen.add(r["Ref"])
        w.writerow([r["Ref"], f'{float(r["PosX"]):.3f}mm', f'{float(r["PosY"]):.3f}mm',
                    "Top" if r["Side"].lower().startswith("top") else "Bottom", f'{float(r["Rot"]):g}'])
missing = smt_refs - seen
assert not missing, f"SMD díly chybí v pozicích: {missing}"

# ---------- ruční díly ----------
HAND = {  # value -> (popis, výrobce, MPN, poznámka k osazení, krok)
 "J1 +12V / PGND": ("Svorkovnice 2 pól, rozteč 3,5 mm", "Phoenix Contact", "1984617", "PT 1,5/2-3,5-H; otvory pro vodiče ven z desky", 4),
 "J4 CAN-H / CAN-L": ("Svorkovnice 2 pól, rozteč 3,5 mm", "Phoenix Contact", "1984617", "PT 1,5/2-3,5-H; otvory pro vodiče ven z desky", 4),
 "J2 SOL1-3 / TCU1-3": ("Svorkovnice 6 pól, rozteč 3,5 mm", "Phoenix Contact", "1984659", "PT 1,5/6-3,5-H; otvory pro vodiče ven z desky", 4),
 "J3 UP DOWN READY LOCKUP": ("Svorkovnice 6 pól, rozteč 3,5 mm", "Phoenix Contact", "1984659", "PT 1,5/6-3,5-H; otvory pro vodiče ven z desky", 4),
 "P1 modulu RP2350-Touch-LCD-2": ("Dutinková lišta 1×14, 2,54 mm", "Sullins", "PPTC141LFBN-RC", "osadit kolmo, nejlépe se zasunutým modulem", 2),
 "P2 modulu RP2350-Touch-LCD-2": ("Dutinková lišta 1×14, 2,54 mm", "Sullins", "PPTC141LFBN-RC", "osadit kolmo, nejlépe se zasunutým modulem", 2),
 "J6 rezerva GPIO5 / 3V3 / GND": ("Kolíková lišta 1×3 (rezerva, volitelné)", "Sullins", "PRPC040SAAN-RC", "odlomit z lišty 1×40", 1),
 "JP2 zakončení CAN 120R": ("Kolíková lišta 1×2 + propojka", "Sullins", "PRPC040SAAN-RC", "odlomit z lišty 1×40; propojku nasadit jen na konci sběrnice", 1),
 "G5LE-1 12V SPDT": ("Relé 12 V SPDT, 10 A", "Omron", "G5LE-1 DC12", "pinově kompatibilní: Songle SRD-12VDC-SL-C", 5),
 "5A": ("Patice pojistky TR5", "Littelfuse", "56000001009", "pojistka viz níže", 3),
 "TSR 1-2450 (12V->5V 1A)": ("Spínaný stabilizátor 12 → 5 V / 1 A", "Traco Power", "TSR 1-2450", "pin 1 = Vin (čtvercový pad), potisk TSR1-2450", 3),
}
EXTRA = [  # (popis, výrobce, MPN, ks/desku, poznámka)
 ("Pojistka TR5 5 A (T)", "Littelfuse", "37215000411", 1, "do patice F1"),
 ("Zkratovací propojka 2,54 mm", "Sullins", "SPC02SYAN", 1, "pro JP2"),
 ("Modul RP2350-Touch-LCD-2", "Waveshare", "RP2350-Touch-LCD-2", 1, "zasouvá se do J7/J8 displejem nahoru, USB-C k horní hraně"),
]
hgrp = collections.OrderedDict()
for p in sorted(hand, key=lambda p: (HAND[p["val"]][4], refkey(p["ref"]))):
    d = HAND[p["val"]]; hgrp.setdefault((d[0], d[1], d[2]), [d, []])[1].append(p["ref"])

# ---------- digikey_bom.csv ----------
with open(OUT + "digikey_bom.csv", "w", newline="") as f:
    w = csv.writer(f); w.writerow(["Quantity", "Manufacturer Part Number", "Manufacturer", "Customer Reference", "Description"])
    dk = collections.OrderedDict()
    for (desc, man, mpn), (d, refs) in hgrp.items():
        if man == "Waveshare": continue
        k = (mpn, man); dk.setdefault(k, [0, [], desc]); dk[k][0] += len(refs); dk[k][1] += refs
    for desc, man, mpn, q, note in EXTRA:
        if man == "Waveshare": continue
        k = (mpn, man); dk.setdefault(k, [0, [], desc]); dk[k][0] += q; dk[k][1] += [note]
    for (mpn, man), (q, refs, desc) in dk.items():
        if mpn == "PRPC040SAAN-RC": q, desc = 1, "Kolíková lišta 1×40 – odlomit 1×3 (J6) a 1×2 (JP2)"   # jedna lišta stačí
        w.writerow([q, mpn, man, " ".join(refs), desc])

# ---------- xlsx ----------
F = "Calibri"; thin = Side(style="thin", color="BBBBBB"); B = Border(left=thin, right=thin, top=thin, bottom=thin)
HF = PatternFill("solid", fgColor="2F5496")
def table(ws, r0, head, data, widths):
    for i, h in enumerate(head, 1):
        c = ws.cell(row=r0, column=i, value=h); c.font = Font(name=F, bold=True, color="FFFFFF"); c.fill = HF
        c.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True); c.border = B
    for j, row in enumerate(data):
        for i, v in enumerate(row, 1):
            c = ws.cell(row=r0 + 1 + j, column=i, value=v); c.font = Font(name=F, size=10); c.border = B
            c.alignment = Alignment(vertical="top", wrap_text=True)
            if j % 2: c.fill = PatternFill("solid", fgColor="F2F2F2")
    for i, wd in enumerate(widths, 1): ws.column_dimensions[get_column_letter(i)].width = wd
    ws.freeze_panes = f"A{r0+1}"
    return r0 + len(data)
wb = Workbook()
ws = wb.active; ws.title = "JLC SMD"
ws["A1"] = "AW4 shield v0.5 – díly, které osadí JLCPCB (SMD, horní strana)"; ws["A1"].font = Font(name=F, bold=True, size=14)
ws["A2"] = ("Nahrát: fab/aw4_shield_gerber.zip (deska), fab/jlc_bom.csv (BOM) a fab/jlc_cpl.csv (pozice). "
            "Basic a Preferred díly jsou bez příplatku, za každý Extended díl si JLC účtuje jednorázový poplatek (cca 3 USD). "
            "V náhledu JLC zkontrolovat natočení diod, optočlenů, tranzistorů, SN65HVD230 a elektrolytu C1.")
ws["A2"].alignment = Alignment(wrap_text=True); ws.merge_cells("A2:H2"); ws.row_dimensions[2].height = 45
data = []
for (val, fp, lcsc, mpn), refs in grp.items():
    t, stock, brand, desc = info[lcsc]
    data.append([", ".join(refs), len(refs), val, mpn, fp.split(":")[1], lcsc, t, stock])
last = table(ws, 4, ["Označení", "Ks/desku", "Hodnota", "Výrobní číslo", "Pouzdro", "LCSC", "Knihovna JLC", "Sklad JLC (při generování)"],
             data, [34, 9, 22, 22, 24, 12, 14, 14])
ext = sorted({d[5] for d in data if d[6] == "Extended"})
ws.cell(row=last + 2, column=1, value=f"Extended díly (příplatek za založení): {len(ext)} – " + ", ".join(ext)).font = Font(name=F, bold=True)
ws.cell(row=last + 3, column=1, value=f"SMD součástek celkem: {len(smt)}, různých druhů: {len(grp)}").font = Font(name=F, size=10)

ws2 = wb.create_sheet("Ruční osazení")
ws2["A1"] = "AW4 shield v0.5 – vývodové díly, které se pájí ručně (po dodání osazené desky z JLC)"; ws2["A1"].font = Font(name=F, bold=True, size=14)
ws2["B2"] = "Počet desek:"; ws2["C2"] = 1; ws2["B2"].font = Font(name=F, bold=True); ws2["C2"].font = Font(name=F, bold=True, color="C00000")
data = []; r = 5
for (desc, man, mpn), (d, refs) in hgrp.items():
    data.append([d[4], ", ".join(refs), desc, man, mpn, len(refs), f"=F{r}*$C$2", d[3]]); r += 1
for desc, man, mpn, q, note in EXTRA:
    data.append(["", "", desc, man, mpn, q, f"=F{r}*$C$2", note]); r += 1
table(ws2, 4, ["Krok", "Označení", "Díl", "Výrobce", "Výrobní číslo", "Ks/desku", "Objednat", "Poznámka"], data, [6, 18, 36, 16, 22, 9, 10, 52])

ws3 = wb.create_sheet("Postup")
steps = ["Desku objednat v JLCPCB s osazením (PCBA, horní strana): gerbery + jlc_bom.csv + jlc_cpl.csv; v náhledu zkontrolovat natočení polarizovaných dílů.",
         "Krok 1 – kolíkové lišty J6 a JP2 (nízké, pájí se první).",
         "Krok 2 – dutinkové lišty J7, J8 pro modul: nejlépe zasunout do modulu, nasadit na desku a teprve pak připájet, ať jsou rovnoběžné.",
         "Krok 3 – měnič TSR 1-2450 (U1) a patice pojistky F1.",
         "Krok 4 – svorkovnice J1–J4 (otvory pro vodiče ven z desky).",
         "Krok 5 – relé K1–K3 (nejvyšší díly, nakonec).",
         "Kontrola před zapnutím: multimetrem odpor +12 V proti PGND (nesmí být zkrat), pak napájet z laboratorního zdroje s omezením proudu (~0,3 A) a změřit +5 V na J3.",
         "Modul zasunout displejem nahoru, USB-C k horní hraně desky. Programovat přes USB jde i při napájení desky (5 V do modulu jde přes Schottkyho diodu D7)."]
for i, t in enumerate(steps, 1):
    c = ws3.cell(row=i, column=1, value=t); c.font = Font(name=F, size=11); c.alignment = Alignment(wrap_text=True, vertical="top")
ws3.column_dimensions["A"].width = 120

ws4 = wb.create_sheet("DigiKey")
rows = list(csv.reader(open(OUT + "digikey_bom.csv")))
table(ws4, 1, rows[0], rows[1:], [10, 26, 18, 30, 40])
wb.save(OUT + "bom_objednavka.xlsx")
print("SMD druhů:", len(grp), "SMD kusů:", len(smt), "ručních:", len(hand), "extended:", ext)
for k, v in info.items(): print(" ", k, v[0], v[1])
