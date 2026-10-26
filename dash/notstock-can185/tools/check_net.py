#!/usr/bin/env python3
"""Compare a KiCad netlist (kicad-cli sch export netlist) with design.py:
every pin on the net the table says, nothing else connected.

    kicad-cli sch export netlist -o /tmp/can.net notstock-can185.kicad_sch
    python3 tools/check_net.py /tmp/can.net
"""
import re
import sys

from design import P

s = open(sys.argv[1]).read()
got = {}
for m in re.finditer(r'\(net \(code "?\d+"?\) \(name "([^"]*)"\)', s):
    end = s.find("(net (code", m.end())
    chunk = s[m.end(): end if end > 0 else len(s)]
    for n in re.finditer(r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"', chunk):
        got[(n.group(1), n.group(2))] = m.group(1).lstrip("/")
want = {(r, pin): net for r, p in P.items() for pin, net in p["pins"].items()}
bad = [(k, want[k], got.get(k)) for k in sorted(want) if got.get(k) != want[k]]
extra = [(k, v) for k, v in sorted(got.items()) if k not in want
         and not k[0].startswith("#") and not v.startswith("unconnected")]
print("pins checked: %d, wrong: %d, extra: %d" % (len(want), len(bad), len(extra)))
for b in bad + extra:
    print("  ", b)
sys.exit(1 if bad or extra else 0)
