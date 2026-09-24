# Draws the J1 connector pinout (docs/PDM_pinout.svg/.png/.pdf) from the schematic netlist.
# Usage: kicad-cli sch export netlist -o fab/.n.net PDM.kicad_sch ; python3 tools/pinout.py fab/.n.net
import sys, html
exec(open('tools/net.py').read().replace("'/prj/pdm.net'", repr(sys.argv[1] if len(sys.argv) > 1 else 'fab/.n.net')))
pin_net = {p: n for n, v in nets.items() for r, p in v if r == 'J1'}

CH = {1: ('palivové čerpadlo', 25), 2: ('rezerva', 25), 3: ('rezerva', 25), 4: ('rezerva', 25),
      5: ('vstřikovače', 15), 6: ('cívky', 15), 7: ('ECU (12V_KEY / 12V_RELAY)', 15), 8: ('příslušenství', 15)}
COL = {'out25': ('#C0392B', '#fff'), 'out15': ('#E67E22', '#fff'), 'out7': ('#D35400', '#fff'), 'in': ('#2E86C1', '#fff'),
       'gnd': ('#1C1C1C', '#fff'), 'key': ('#F1C40F', '#1C1C1C'), 'lamp': ('#8E44AD', '#fff'), 'disp': ('#1E8449', '#fff')}
INP = {'/IN1_RAW': ('IN1', 'Řízení CH1 (čerpadlo), spíná ECU k GND'),
       '/IN_MAIN_RAW': ('MAIN', 'Řízení CH5, CH6, CH8, spíná ECU k GND (hlavní relé)'),
       '/IN2_RAW': ('IN2', 'Řízení CH2, propojka JP1: 1-2 = k GND, 2-3 = +12 V'),
       '/IN3_RAW': ('IN3', 'Řízení CH3, propojka JP2: 1-2 = k GND, 2-3 = +12 V'),
       '/IN4_RAW': ('IN4', 'Řízení CH4, propojka JP3: 1-2 = k GND, 2-3 = +12 V')}

def info(pin):
    n = pin_net[pin]
    if n.startswith('/OUT'):
        ch = int(n[4:]); name, amps = CH[ch]
        mates = [p for p, m in pin_net.items() if m == n and p != pin]
        cat = 'out7' if ch == 7 else ('out25' if amps == 25 else 'out15')
        return cat, f'CH{ch}', f'OUT{ch}', f'Výstup CH{ch}: {name}' + (f' (spolu s {mates[0]})' if mates else ''), f'{amps} A'
    if n in INP: s, d = INP[n]; return 'in', s, ('IN_MAIN' if s == 'MAIN' else s), d, 'signál'
    if n == '/GND': return 'gnd', 'GND', 'GND', 'Zem logiky PDM (kostra / zem ECU)', '1 A'
    if n == '/VKEY_RAW': return 'key', 'T15', 'KEY +12V', 'Klíček T15: napájí logiku, bez klíčku je PDM vypnuté', '1 A'
    if n == '/LAMP': return 'lamp', 'LAMP', 'FAULT LAMP', 'Kontrolka poruchy, spíná k GND (2. pól na +12 V)', '0,4 A'
    if n == '/+12V_DISP': return 'disp', '12V', '+12V DISP', '+12 V pro displej, s klíčkem, jištěno PTC 0,5 A', '0,5 A'
    return 'gnd', '?', n, n, ''

big = {p for p in pin_net if p[1:] in ('1', '8')}          # columns 1 and 8 carry the 2.8 mm terminals
W, Hh = 1684, 1191
o = []
T = lambda x, y, s, size=16, w='normal', fill='#1C1C1C', anchor='start': o.append(
    f'<text x="{x}" y="{y}" font-family="DejaVu Sans" font-size="{size}" font-weight="{w}" fill="{fill}" text-anchor="{anchor}">{html.escape(s)}</text>')
o.append(f'<rect width="{W}" height="{Hh}" fill="#ffffff"/>')
o.append(f'<rect width="{W}" height="86" fill="#1C1C1C"/><rect y="86" width="{W}" height="6" fill="#C0392B"/>')
T(40, 56, 'PDM 8CH', 38, 'bold', '#fff'); T(250, 56, 'Pinout konektoru J1', 30, 'normal', '#fff')
T(W - 40, 40, 'Aptiv / FCI SICMA 24 pin, HCCPHPE24BKA90F', 18, 'normal', '#ddd', 'end')
T(W - 40, 66, 'NotStockECU · rev 0.1', 18, 'normal', '#ddd', 'end')

# ---- connector face (view into the PDM header = wire side of the harness plug) ----
fx, fy, pitch_x, pitch_y = 90, 200, 86, 96
cols = [1, 2, 3, 4, 5, 6, 7, 8]                           # col 1 on the left when looking into the header
xs = {c: fx + 60 + (c - 1) * pitch_x + (14 if c == 8 else 0) - (14 if c == 1 else 0) for c in cols}
ys = {'A': fy + 84, 'B': fy + 84 + pitch_y, 'C': fy + 84 + 2 * pitch_y}
o.append(f'<rect x="{fx - 10}" y="{fy}" width="{8 * pitch_x + 60}" height="{3 * pitch_y + 74}" rx="26" fill="#2B2B2B"/>')
o.append(f'<rect x="{fx + 4}" y="{fy + 14}" width="{8 * pitch_x + 32}" height="{3 * pitch_y + 46}" rx="18" fill="#3A3A3A"/>')
o.append(f'<rect x="{fx + 4 * pitch_x - 10}" y="{fy - 16}" width="60" height="18" rx="4" fill="#2B2B2B"/>')   # key
T(fx + 4 * pitch_x + 20, fy - 22, 'zámek', 13, 'normal', '#666', 'middle')
for c in cols: T(xs[c], fy + 36, str(c), 15, 'bold', '#aaa', 'middle')
for r in 'ABC': T(fx + 22, ys[r] + 6, r, 16, 'bold', '#aaa', 'middle')
for r in 'ABC':
    for c in cols:
        p = f'{r}{c}'; cat, short, *_ = info(p); bg, fg = COL[cat]; rad = 33 if p in big else 27
        o.append(f'<circle cx="{xs[c]}" cy="{ys[r]}" r="{rad}" fill="{bg}" stroke="#fff" stroke-width="3"/>')
        T(xs[c], ys[r] - 3, p, 15, 'bold', fg, 'middle'); T(xs[c], ys[r] + 15, short, 12, 'normal', fg, 'middle')
by = fy + 3 * pitch_y + 74
o.append(f'<rect x="{fx - 30}" y="{by + 18}" width="{8 * pitch_x + 100}" height="10" fill="#1E8449" opacity="0.55"/>')
T(fx + 4 * pitch_x + 20, by + 50, 'deska PDM (DPS) pod konektorem', 14, 'normal', '#1E8449', 'middle')
T(fx - 20, by + 84, 'Pohled zepředu do konektoru na PDM (strana zasouvání)', 16, 'bold')
T(fx - 20, by + 106, '= pohled na zásuvku svazku ze strany vodičů.', 16)
T(fx - 20, by + 128, 'Velké kruhy = kontakty 2,8 mm (sloupce 1 a 8), malé = 1,5 mm.', 16)
T(fx - 20, by + 150, 'Označení A1–C8 odpovídá desce; před výrobou svazku porovnejte', 14, 'normal', '#666')
T(fx - 20, by + 168, 's číslováním komor na zásuvce (výkres Aptiv/FCI).', 14, 'normal', '#666')

# legend
ly = by + 202
leg = [('out25', 'Výstup 25 A (CH1–CH4)'), ('out15', 'Výstup 15 A (CH5, CH6, CH8)'), ('out7', 'Výstup ECU 15 A (CH7)'),
       ('in', 'Vstup řízení z ECU'), ('key', 'Klíček T15 (+12 V)'), ('gnd', 'Zem logiky'), ('lamp', 'Kontrolka poruchy'),
       ('disp', 'Výstup +12 V displej')]
for i, (k, t) in enumerate(leg):
    x = fx - 20 + (i % 2) * 380; y = ly + (i // 2) * 30
    o.append(f'<rect x="{x}" y="{y - 18}" width="26" height="24" rx="5" fill="{COL[k][0]}"/>'); T(x + 38, y, t, 16)

# VBAT stud box
vy = ly + 4 * 30 - 4
o.append(f'<rect x="{fx - 20}" y="{vy}" width="740" height="100" rx="10" fill="#FDEDEC" stroke="#C0392B" stroke-width="2"/>')
T(fx, vy + 30, 'VBAT: šroub M6 (J2) na desce', 20, 'bold', '#C0392B')
T(fx, vy + 56, 'Přímo z baterie přes hlavní pojistku u baterie. Napájí všech 8 výstupů', 16)
T(fx, vy + 80, '(součet proudů všech kanálů). Kabelové oko M6, průřez dle celkového proudu.', 16)

# indication box
iy = vy + 112
o.append(f'<rect x="{fx - 20}" y="{iy}" width="740" height="136" rx="10" fill="#F4F6F7" stroke="#BFBFBF" stroke-width="2"/>')
T(fx, iy + 28, 'Indikace (LED u každého kanálu a kontrolka C5)', 18, 'bold')
for k, (a, b) in enumerate([('zelená LED svítí', 'výstup kanálu je zapnutý'),
                            ('červená LED / kontrolka svítí', 'přetížení: proud nad 25 A (CH1–4) / 15 A (CH5–8)'),
                            ('červená LED / kontrolka bliká', 'porucha: zkrat, přehřátí, kanál vypnutý ochranou'),
                            ('kontrolka 2 s po zapnutí klíčku', 'test žárovky a vedení')]):
    T(fx, iy + 56 + k * 22, a, 15, 'bold'); T(fx + 300, iy + 56 + k * 22, b, 15)

# ---- pin table ----
tx, ty = 860, 130
cw = [58, 58, 140, 430, 70]
hdr = ['Pin', 'Kont.', 'Signál', 'Funkce', 'Max.']
o.append(f'<rect x="{tx}" y="{ty}" width="{sum(cw)}" height="36" fill="#1C1C1C"/>')
x = tx
for w, h in zip(cw, hdr): T(x + 10, ty + 24, h, 16, 'bold', '#fff'); x += w
rows = [f'{r}{c}' for r in 'ABC' for c in cols]
rh = 38
for i, p in enumerate(rows):
    cat, short, sig, desc, mx = info(p); y = ty + 36 + i * rh
    o.append(f'<rect x="{tx}" y="{y}" width="{sum(cw)}" height="{rh}" fill="{"#F4F6F7" if i % 2 else "#ffffff"}"/>')
    o.append(f'<rect x="{tx}" y="{y}" width="8" height="{rh}" fill="{COL[cat][0]}"/>')
    vals = [p, '2,8' if p in big else '1,5', sig, desc, mx]
    x = tx
    for j, (w, v) in enumerate(zip(cw, vals)):
        T(x + 14 if j == 0 else x + 10, y + 25, v, 15 if j != 3 else 14, 'bold' if j in (0, 2) else 'normal'); x += w
o.append(f'<rect x="{tx}" y="{ty}" width="{sum(cw)}" height="{36 + len(rows) * rh}" fill="none" stroke="#BFBFBF"/>')

open('docs/PDM_pinout.svg', 'w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{Hh}" viewBox="0 0 {W} {Hh}">' + ''.join(o) + '</svg>')
import cairosvg
cairosvg.svg2png(url='docs/PDM_pinout.svg', write_to='docs/PDM_pinout.png', output_width=W * 2)
cairosvg.svg2pdf(url='docs/PDM_pinout.svg', write_to='docs/PDM_pinout.pdf')
print('ok', len(pin_net), 'pins')
