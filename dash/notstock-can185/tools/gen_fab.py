#!/usr/bin/env python3
"""Fabrication and documentation files from the KiCad project:

    fab/notstock-can185-gerbers.zip   Gerbers and drill, for any PCB maker
    fab/bom.csv                       parts (JLCPCB assembly columns)
    fab/cpl.csv                       placement (JLCPCB columns)
    docs/schematic.pdf, docs/top.svg, docs/bottom.svg, docs/assembly.svg

    python3 tools/gen_fab.py           (after gen_sch.py and gen_pcb.py)
"""
import csv
import os
import shutil
import subprocess
import zipfile

from design import P

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
NAME = "notstock-can185"
PCB = os.path.join(ROOT, NAME + ".kicad_pcb")
SCH = os.path.join(ROOT, NAME + ".kicad_sch")
FAB = os.path.join(ROOT, "fab")
DOCS = os.path.join(ROOT, "docs")
TMP = os.path.join(ROOT, "build", "fab")


def cli(*args):
    subprocess.run(["kicad-cli"] + list(args), check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main():
    for d in (FAB, DOCS):
        os.makedirs(d, exist_ok=True)
    shutil.rmtree(TMP, ignore_errors=True)
    os.makedirs(TMP)

    # Gerbers and drill
    cli("pcb", "export", "gerbers", "-o", TMP + "/",
        "--layers", "F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,"
        "B.Mask,Edge.Cuts", "--subtract-soldermask", PCB)
    cli("pcb", "export", "drill", "-o", TMP + "/", "--format", "excellon",
        "--excellon-separate-th", "--generate-map", "--map-format", "gerberx2",
        PCB)
    zpath = os.path.join(FAB, NAME + "-gerbers.zip")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(TMP)):
            z.write(os.path.join(TMP, f), f)

    # placement: kicad's CSV into JLCPCB's columns
    pos = os.path.join(TMP, "pos.csv")
    cli("pcb", "export", "pos", "-o", pos, "--format", "csv", "--units", "mm",
        "--use-drill-file-origin", PCB)
    rows = list(csv.DictReader(open(pos)))
    with open(os.path.join(FAB, "cpl.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for r in rows:
            if r["Ref"].startswith(("H", "JP")) or P.get(r["Ref"], {}).get("dnp"):
                continue
            w.writerow([r["Ref"], r["PosX"] + "mm", r["PosY"] + "mm",
                        "Top" if r["Side"] == "top" else "Bottom", r["Rot"]])

    # parts, grouped by value and footprint
    groups = {}
    for ref, p in P.items():
        if ref.startswith(("H", "JP")) or p["dnp"]:
            continue
        key = (p["mpn"], p["fp"].split(":")[1], p["lcsc"])
        groups.setdefault(key, []).append(ref)
    with open(os.path.join(FAB, "bom.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (mpn, fp, lcsc), refs in sorted(groups.items(),
                                            key=lambda g: g[1][0]):
            w.writerow([mpn, ",".join(sorted(refs)), fp, lcsc])

    # documentation
    cli("sch", "export", "pdf", "-o", os.path.join(DOCS, "schematic.pdf"), SCH)
    cli("pcb", "export", "svg", "-o", os.path.join(DOCS, "top.svg"),
        "--layers", "F.Cu,F.Mask,F.SilkS,Edge.Cuts", "--page-size-mode", "2",
        "--exclude-drawing-sheet", PCB)
    cli("pcb", "export", "svg", "-o", os.path.join(DOCS, "bottom.svg"),
        "--layers", "B.Cu,B.Mask,B.SilkS,Edge.Cuts", "--mirror",
        "--page-size-mode", "2", "--exclude-drawing-sheet", PCB)
    cli("pcb", "export", "svg", "-o", os.path.join(DOCS, "assembly.svg"),
        "--layers", "F.Fab,F.CrtYd,Edge.Cuts", "--black-and-white",
        "--page-size-mode", "2", "--exclude-drawing-sheet", PCB)
    # pictures for the README: the SVGs on a dark page, through Chromium
    chrome = os.environ.get("CHROME", "/opt/pw-browsers/chromium-1194/"
                            "chrome-linux/chrome")
    if os.path.exists(chrome):
        for name, bg in (("top", "#1d1d1d"), ("bottom", "#1d1d1d"),
                         ("assembly", "#ffffff")):
            html = os.path.join(TMP, name + ".html")
            open(html, "w").write(
                "<html><body style='margin:0;background:%s'><img src='%s' "
                "style='width:900px;height:900px'></body></html>"
                % (bg, os.path.join(DOCS, name + ".svg")))
            subprocess.run([chrome, "--headless", "--no-sandbox", "--disable-gpu",
                            "--hide-scrollbars", "--window-size=900,1000",
                            "--screenshot=" + os.path.join(DOCS, name + ".png"),
                            "file://" + html], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
            try:                     # the window's height is not all page
                from PIL import Image
                png = os.path.join(DOCS, name + ".png")
                Image.open(png).crop((0, 0, 900, 900)).save(png)
            except ImportError:
                pass
    print("wrote fab/ and docs/")


if __name__ == "__main__":
    main()
