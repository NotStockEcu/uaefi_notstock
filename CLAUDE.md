# Working in this repository

## The owner

- Answer in Czech: short, technical, no flattery, no em dashes.
- When something cannot be checked from here, ask for a photo or a log.
- No model names in commits, PRs or code. No car makers' logos.
- Files for the owner: single files (a `.bin` for Wi-Fi, STL, pictures);
  the phone cannot unzip. A zip only as an extra, `FLASH.txt` in it with
  CRLF line ends.

## Nothing lives outside git

The cloud container is thrown away when the session sits idle: the
scratchpad, installed tools and every file not pushed are gone. So:

- Everything that is part of the work goes into the repository and is
  pushed when it is made, not at the end: sources, and what is built from
  them for the owner.
  - Firmware releases: `dash/<project>/release/fwN/` (the bins and
    `FLASH.txt`, `git add -f`).
  - 3D housings: `cad/out/` (STL and STEP).
  - Boards: `fab/` and `docs/` (gen_fab.py).
  - Files the owner sends (a 3MF to build on, a vendor STEP, a
    datasheet): into the project's `cad/inputs/` or `docs/`, at once.
- Experiments that led nowhere need not be kept.
- The scratchpad is for scratch only.

## Tools

`tools/cloud-setup.sh` installs what a fresh container lacks: ESP-IDF 5.5
(`/root/esp/esp-idf`), LVGL 8.4.0 (`/root/esp/comps/lvgl`), KiCad 7 with
pcbnew, Freerouting (`/root/esp/freerouting.jar`), trimesh, manifold3d,
OCP. It skips what is there; run it when `idf.py` or `kicad-cli` is
missing.

- ESP32 gauges: `. /root/esp/idfenv.sh`, then in the project
  `idf.py -DEXTRA_COMPONENT_DIRS=/root/esp/comps -B build build`.
- Round gauge simulator: `dash/notstock-round/tools/sim`,
  `make LVGL_DIR=/root/esp/comps/lvgl`.
- CAN boards: `dash/notstock-can185/README.md` ("Regenerate"),
  Freerouting jar as above.

## The gauges

- **1.85" (ESP32-S3-Touch-LCD-1.85):**
  - Flash layout: the updater (`dash/notstock-updater185`) in factory at
    0x20000, the gauge in ota_0 at 0x200000.
  - Wi-Fi update: AP NOTSTOCK-185, password notstock, http://192.168.4.1.
  - Current firmware: fw17 (`dash/notstock-round-lcd185/release/fw17`).
- **1.75" AMOLED:** the same layout (`dash/notstock-updater175`), AP
  NOTSTOCK-175.
- **The owner's COM ports:** COM3 2.1", COM7 AMOLED 1.75, COM9 A4 1.32,
  COM13 / COM14 1.85.
