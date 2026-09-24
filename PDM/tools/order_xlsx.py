# Builds docs/PDM_objednavka.xlsx from BOM.csv + tools/parts.py
import csv, re, collections
from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter
exec(open('tools/parts.py').read())
desc_by_mpn = {m: d for (_, _), (_, m, d) in PARTS.items()}
key = lambda r: (re.sub(r'\d', '', r), int(re.sub(r'\D', '', r)))
pkg = lambda fp: re.sub(r'_\d{4}Metric', '', fp.split(':')[1])
agg = collections.OrderedDict()
for r in csv.DictReader(open('BOM.csv')):
    a = agg.setdefault((r['Manufacturer'], r['MPN'], r['DNP']), {'refs': [], 'vals': [], 'pkg': pkg(r['Footprint']), 'fp': r['Footprint']})
    a['refs'] += r['References'].split(); a['vals'].append(r['Value'])
def typ(fp):
    if fp.startswith(('Resistor', 'Capacitor')): return 'Pasivní'
    if fp.startswith(('Connector', 'PDM:', 'Fuse')): return 'Konektor/ostatní'
    return 'Polovodič'
order = {'Polovodič': 0, 'Konektor/ostatní': 1, 'Pasivní': 2}
items = [it for it in agg.items() if it[0][1] != '-']            # the M6 hole is not a part (see Mechanika)
items.sort(key=lambda kv: (order[typ(kv[1]['fp'])], kv[0][2] == 'DNP', kv[0][1]))

A = 'Arial'; bold = Font(name=A, bold=True); norm = Font(name=A); blue = Font(name=A, color='0000FF'); grey = Font(name=A, color='808080', italic=True)
hdr_fill = PatternFill('solid', fgColor='1F3864'); hdr_font = Font(name=A, bold=True, color='FFFFFF'); inp = PatternFill('solid', fgColor='FFFF00')
thin = Side(style='thin', color='BFBFBF'); box = Border(left=thin, right=thin, top=thin, bottom=thin)
wb = Workbook(); ws = wb.active; ws.title = 'Objednávka'
ws['A1'] = 'PDM 8CH rev 0.1 – objednávka součástek'; ws['A1'].font = Font(name=A, bold=True, size=14)
ws['A2'] = 'Žlutá pole jsou vstupy: počet desek, rezerva a ceny. Množství a součty se přepočítají samy.'; ws['A2'].font = grey
for i, (l, v, f) in enumerate([('Počet desek', 2, '0'), ('Rezerva pasivních součástek', 0.10, '0%'),
                                ('Min. kusů navíc u pasivních (ztráty při osazování)', 5, '0')]):
    r = 3 + i; ws.cell(r, 1, l).font = norm; c = ws.cell(r, 3, v); c.font = blue; c.fill = inp; c.number_format = f; c.border = box
ws['D4'] = 'Pasivní součástky se objednávají s rezervou, polovodiče a konektory přesně.'; ws['D4'].font = grey
H = ['Poz.', 'Označení na desce', 'Ks / deska', 'Ks objednat', 'Hodnota', 'Pouzdro', 'Výrobce', 'MPN (objednací číslo)', 'Popis',
     'Typ', 'Osadit', 'Cena / ks (Kč)', 'Cena celkem (Kč)', 'Poznámka']
hr = 7
for j, h in enumerate(H, 1):
    c = ws.cell(hr, j, h); c.font = hdr_font; c.fill = hdr_fill; c.alignment = Alignment(wrap_text=True, vertical='center'); c.border = box
notes = {'EEE-FK1H470P': 'C2 změněn z 22u na 47u/50V (stejné pouzdro jako C1)', 'BTS70021EPPXUMA1': 'Pod pouzdrem tepelné prokovy; měď 2 oz',
         'HCCPHPE24BKA90F': 'Protikus (zásuvka + kontakty) viz list Mechanika', 'LM2901AVQDRQ1': 'U9 je stejný typ (oscilátor a test kontrolky)'}
r = hr
for n, ((man, mpn, dnp), a) in enumerate(items, 1):
    r += 1
    data = [n, ', '.join(sorted(a['refs'], key=key)), len(a['refs']), None, ' / '.join(sorted(set(a['vals']))), a['pkg'], man, mpn,
            desc_by_mpn.get(mpn, ''), typ(a['fp']), 'NE' if dnp else 'ANO', None, None,
            'Neosazovat (místo pro volitelný kondenzátor)' if dnp else notes.get(mpn, '')]
    for j, v in enumerate(data, 1):
        c = ws.cell(r, j, v); c.font = norm; c.border = box; c.alignment = Alignment(vertical='top', wrap_text=(j in (2, 9, 14)))
    ws.cell(r, 4).value = f'=IF(K{r}="NE",0,IF(J{r}="Pasivní",ROUNDUP(C{r}*$C$3*(1+$C$4),0)+$C$5,C{r}*$C$3))'; ws.cell(r, 4).font = bold
    pc = ws.cell(r, 12); pc.fill = inp; pc.font = blue; pc.number_format = '#,##0.00'
    ws.cell(r, 13).value = f'=IF(L{r}="","",D{r}*L{r})'; ws.cell(r, 13).number_format = '#,##0.00'
last = r; r += 2
ws.cell(r, 1, 'Součet').font = bold; ws.cell(r, 2, 'součástek na desku / kusů k objednání / cena').font = grey
for col in (3, 4, 13):
    L = get_column_letter(col); c = ws.cell(r, col, f'=SUM({L}{hr + 1}:{L}{last})'); c.font = bold
ws.cell(r, 13).number_format = '#,##0.00'
r += 2
for t in ['Objednací čísla vybrána pro automobilové použití (AEC-Q, kde existuje). Skladovost a ceny ověřte u distributora (TME, Mouser, LCSC) – při tvorbě nebyl přístup k jejich webu.',
          'Rezistory: všechny Yageo RC0805 1 % (i tam, kde by stačilo 5 %) – jedna řada, stejná cena.',
          'Lze použít ekvivalenty jiných výrobců se stejnými parametry (hodnota, napětí, dielektrikum, pouzdro).']:
    ws.cell(r, 1, t).font = grey; r += 1
for j, w in enumerate([6, 40, 9, 11, 16, 22, 18, 24, 40, 15, 8, 13, 15, 40], 1): ws.column_dimensions[get_column_letter(j)].width = w
ws.freeze_panes = f'A{hr + 1}'; ws.auto_filter.ref = f'A{hr}:N{last}'

p = wb.create_sheet('DPS výroba')
p['A1'] = 'Deska plošných spojů – parametry pro výrobce'; p['A1'].font = Font(name=A, bold=True, size=14)
spec = [('Soubory', 'fab/PDM_gerber.zip (Gerber X2 + vrtání Excellon), fab/PDM-pos.csv (pozice pro osazení)'),
        ('Rozměr', '124 × 116 mm'), ('Počet vrstev', '4 (F.Cu, In1 = GND, In2 = VBAT, B.Cu)'), ('Tloušťka desky', '1,6 mm'),
        ('Měď vnější vrstvy', '2 oz (70 µm) – nutné kvůli kanálům 25 A'),
        ('Měď vnitřní vrstvy', '2 oz doporučeno (VBAT rovina vede proud všech kanálů), minimum 1 oz'),
        ('Min. šířka spoje / mezera', '0,15 mm / 0,2 mm'), ('Prokovy', '0,3 mm vrták / 0,6 mm ploška'),
        ('Povrch', 'ENIG (rovné plošky pro TSDSO-14 a SOT-223), případně HASL bezolovnatý'),
        ('Maska / potisk', 'zelená / bílý'), ('Počet kusů', '=Objednávka!C3'),
        ('Poznámka', 'Tepelné prokovy pod BTS7002 (ploška VBAT): při osazování hlídat, aby pájka nestekla do prokovů (tenting / zaplnění prokovů).')]
for i, (k, v) in enumerate(spec, 3):
    p.cell(i, 1, k).font = bold; c = p.cell(i, 2, v); c.font = norm; c.alignment = Alignment(wrap_text=True, vertical='top', horizontal='left')
p.column_dimensions['A'].width = 28; p.column_dimensions['B'].width = 95

m = wb.create_sheet('Mechanika')
m['A1'] = 'Mechanika a příslušenství (není ve schématu)'; m['A1'].font = Font(name=A, bold=True, size=14)
for j, h in enumerate(['Položka', 'Ks / deska', 'Ks objednat', 'Specifikace', 'Poznámka'], 1):
    c = m.cell(3, j, h); c.font = hdr_font; c.fill = hdr_fill; c.border = box
mitems = [('Propojka (jumper) 2,54 mm', 3, 'Würth 60900213421 nebo ekvivalent', 'JP1–JP3: 1-2 = vstup spínaný k zemi (LS), 2-3 = aktivní +12 V'),
          ('Šroub M6 pro VBAT (J2)', 1, 'M6 × 20 + 2× podložka M6 + pružná podložka + matice M6', 'Kabelové oko VBAT pod hlavu šroubu; ploška J2 je VBAT'),
          ('Kabelové oko M6', 1, 'Lisovací oko M6 pro průřez VBAT kabelu (např. 16–25 mm²)', 'Průřez podle celkového proudu'),
          ('Hlavní pojistka VBAT', 1, 'MIDI/ANL pojistka + držák, hodnota podle průřezu VBAT kabelu', 'Co nejblíž baterii; PDM nemá vlastní hlavní pojistku'),
          ('Protikus konektoru J1', 1, 'SICMA 24-pin zásuvka k HCCPHPE24BKA90F + kontakty 1,5 mm (18×) a 2,8 mm (6×) + záslepky/těsnění',
           'Přesné objednací číslo protikusu dohledat v katalogu Amphenol FCI / Aptiv (neověřeno)'),
          ('Montážní šrouby M3', 4, 'Šroub M3 + distanční sloupek podle krabičky', 'Otvory H1–H4 v rozích desky')]
for i, (a, q, s, nn) in enumerate(mitems, 4):
    for j, v in enumerate([a, q, f'=B{i}*Objednávka!$C$3', s, nn], 1):
        c = m.cell(i, j, v); c.font = norm; c.border = box; c.alignment = Alignment(wrap_text=True, vertical='top')
for j, w in enumerate([30, 11, 12, 60, 60], 1): m.column_dimensions[get_column_letter(j)].width = w
wb.save('docs/PDM_objednavka.xlsx'); print('lines', len(items), 'rows', hr + 1, '-', last)
