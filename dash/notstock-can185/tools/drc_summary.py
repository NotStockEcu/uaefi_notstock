#!/usr/bin/env python3
"""Summary of build/drc.rpt. lib_footprint_issues are left out: they only
say the generator ran without KiCad's library table (the footprints are
the standard libraries' own)."""
import re
import sys

s = open(sys.argv[1] if len(sys.argv) > 1 else "build/drc.rpt").read()
items = re.findall(r"\n\[(\w+)\]: (.*)\n(.*)\n(.*)\n(.*)", s)
real = [i for i in items if i[0] != "lib_footprint_issues"]
unconn = re.search(r"\*\* Found (\d+) unconnected", s).group(1)
print("DRC: %d problems, %d unconnected" % (len(real), int(unconn)))
for kind, msg, _, a, b in real:
    print("  [%s] %s | %s | %s" % (kind, msg[:70], a.strip()[:60], b.strip()[:60]))
sys.exit(1 if real or unconn != "0" else 0)
