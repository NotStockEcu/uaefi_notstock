#!/bin/bash
# Tools for this repository in a fresh cloud container (Claude Code on the
# web): ESP-IDF 5.5 for the ESP32-S3 gauges, LVGL 8.4 (offline, no
# component registry), KiCad 7 with Freerouting for the boards, Python for
# the 3D housings. Each step is skipped when it is there already, so the
# script can run on every container start (the environment's Setup script):
#
#     bash /home/user/uaefi_notstock/tools/cloud-setup.sh
#
# Then, in a shell:  . /root/esp/idfenv.sh   (idf.py on the PATH)
set -u
ESP=/root/esp
mkdir -p "$ESP"
log() { echo "[cloud-setup] $*"; }

# KiCad 7 (Ubuntu 24.04: 7.0.11), its python module pcbnew, the libraries
if ! command -v kicad-cli >/dev/null; then
    log "KiCad"
    apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
        --no-install-recommends kicad kicad-footprints kicad-symbols poppler-utils >/dev/null \
        || log "KiCad FAILED"
fi

# Freerouting 2.0.1 for gen_pcb.py --freerouting (Java 21 is in the image)
if [ ! -f "$ESP/freerouting.jar" ]; then
    log "Freerouting"
    curl -sSfL -o "$ESP/freerouting.jar" \
        https://github.com/freerouting/freerouting/releases/download/v2.0.1/freerouting-2.0.1.jar \
        || log "Freerouting FAILED"
fi

# ESP-IDF 5.5, the ESP32-S3 toolchain, its python env. dl.espressif.com is
# not reachable: tools from GitHub, no constraints file.
if [ ! -d "$ESP/esp-idf" ]; then
    log "ESP-IDF"
    git clone -q --depth 1 -b v5.5 --recursive --shallow-submodules \
        https://github.com/espressif/esp-idf.git "$ESP/esp-idf" || log "ESP-IDF clone FAILED"
fi
if [ ! -d /root/.espressif/tools/xtensa-esp-elf ]; then
    log "ESP-IDF tools"
    (cd "$ESP/esp-idf" && IDF_GITHUB_ASSETS=github.com \
        python3 tools/idf_tools.py install --targets=esp32s3 required >/dev/null \
        && python3 tools/idf_tools.py install-python-env --no-constraints >/dev/null) \
        || log "ESP-IDF tools FAILED"
fi
cat > "$ESP/idfenv.sh" <<'EOF'
export IDF_PATH=/root/esp/esp-idf IDF_PYTHON_CHECK_CONSTRAINTS=no
. $IDF_PATH/export.sh >/dev/null 2>&1
# LVGL from comps/ (idf.py -DEXTRA_COMPONENT_DIRS=/root/esp/comps), not
# from the component registry
export IDF_COMPONENT_MANAGER=0
EOF

# LVGL 8.4.0: an IDF component for the gauges, and the simulator's
# (dash/notstock-round/tools/sim: make LVGL_DIR=/root/esp/comps/lvgl)
if [ ! -d "$ESP/comps/lvgl" ]; then
    log "LVGL"
    git clone -q --depth 1 -b v8.4.0 https://github.com/lvgl/lvgl.git "$ESP/comps/lvgl" \
        && rm -rf "$ESP/comps/lvgl/.git" || log "LVGL FAILED"
fi

# python for the housings (trimesh, manifold3d), STEP export (OCP), fast
# renders (embreex), pictures (pillow)
if ! python3 -c "import trimesh, manifold3d, OCP, embreex, PIL.Image" 2>/dev/null; then
    log "python CAD"
    python3 -m pip install -q --ignore-installed pillow >/dev/null 2>&1
    python3 -m pip install -q trimesh manifold3d rtree scipy cadquery-ocp embreex >/dev/null 2>&1 \
        || log "python CAD FAILED"
fi
log "done"
