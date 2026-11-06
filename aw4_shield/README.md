# AW4 shield v0.4 – ovladač automatické převodovky Aisin AW4 s modulem RP2350-Touch-LCD-2

Deska 100 × 100 mm (vše vývodové, pájí se ručně), do které se zasune modul **Waveshare RP2350-Touch-LCD-2** (procesor RP2350 + displej 2″ 240×320). Deska:

* při spínači **READY = vypnuto** nechá původní TCU připojené (převodovka běží jako sériově),
* při **READY = zapnuto** odpojí 3 vodiče solenoidů od původní TCU a převezme je (S1, S2, S3/lockup),
* čte spínače **UP, DOWN, READY, LOCKUP** (5 V, na desce převedeno na 3,3 V),
* ukazuje zařazený stupeň na displeji modulu (zelená sedmisegmentovka přes celý displej),
* má **CAN** (MCP2562) pro hodnoty z rusEFI (rychlost, otáčky, TPS …) – firmware se doplní.

![PCB](docs/pcb_3d.png)

## Co je ve složce

| Cesta | Obsah |
|---|---|
| `kicad/` | **KiCad projekt v0.4** (schéma + PCB), formát KiCad 8, ověřeno v KiCadu 10.0.6 |
| `fab/aw4_shield_gerber.zip` | Gerbery + vrtání pro výrobu DPS |
| `fab/drc_report.txt`, `fab/erc_report.txt` | výsledky kontrol z KiCadu |
| `bom_objednavka.xlsx` | seznam k objednání (počet desek se dá změnit), postup osazení, list DigiKey |
| `digikey_bom.csv` | soubor pro DigiKey BOM Manager (1 deska + rezerva u drobných dílů) |
| `docs/aw4_shield_osazovaci_vykres.pdf` | osazovací výkres s označením součástek |
| `docs/aw4_shield_schema_kicad.pdf`, `docs/aw4_shield_pcb.pdf`, `docs/pcb_*.png` | schéma, výkres DPS, 3D náhledy |
| `firmware/rp2350/` | firmware pro modul RP2350 (+ hotové `.uf2` k přetažení) |
| `tools/` | skripty, kterými se schéma, deska a podklady generují (`pipeline.sh`) |
| `firmware/aw4_controller.ino`, `nextion/`, `docs/v0.3_*` | starší verze s Arduino Nano + Nextion (deska v0.3) |

## Podle servisního manuálu AW-4

* Solenoid se měří mezi **držákem (kostra převodovky) a drátem** → druhý konec solenoidu je na kostře, TCU na drát spíná **+12 V (high-side)**. Výstupy desky jsou proto high-side (P-MOSFET).
* Odpor cívky 11–15 Ω → **0,8–1,25 A na solenoid** (při 14,4 V až 1,3 A). Všechny tři najednou max. ~3,9 A.
* Solenoid 1 + 2 řadí stupně, **solenoid 3 = lockup**.
* Ověřit multimetrem na autě: že drátem solenoidu opravdu jde z TCU +12 V a že vodiče S1/S2/S3 jsou správně rozlišené.

| Poloha / stupeň | Solenoid 1 | Solenoid 2 |
|---|---|---|
| P, R, N | ON | OFF |
| 1. | ON | OFF |
| 2. | ON | ON |
| 3. | OFF | ON |
| 4. (OD) | OFF | OFF |

## Jak deska funguje

```
 J1 +12V (zapalování) ─[F1 5A]─[Q1 IRF4905, přepólování]─┬─ +12V_SW ─ cívky relé K1–K3, MOSFETy Q3–Q5
                          D2 P6KE27A (TVS), C1 470µF, C2 ┘
                                                          └─[U1 TSR 1-2450]─[L1]─ +5V ─[JP1]─ modul (P1-14 5V)
                                                                                     └─ J3 spínače, U7 CAN (VDD)

 Vodiče solenoidů (3×) ──► K1/K2/K3 (přepínací kontakty SPDT):
        COM ← vodič k SOLENOIDU (J2: SOL1–3)
        NC  ← původní TCU (J2: TCU1–3) … klid = původní TCU řídí (fail-safe)
        NO  ← náš P-MOSFET Q3/Q4/Q5 (+12 V na solenoid)
```

### Stavy relé (fail-safe)

| READY spínač | GPIO21 (modul) | Relé K1–K3 | Solenoidy řídí |
|---|---|---|---|
| vypnuto | cokoli | odpadlé | původní TCU |
| zapnuto | LOW / modul vyjmutý | odpadlé | původní TCU |
| zapnuto | HIGH | přitažené | AW4 (Q3–Q5) |
| výpadek 12 V | – | odpadlé | původní TCU |

LED optočlenu relé (U3) je napájená **přes READY spínač** a do země ji spíná tranzistor Q6 z GPIO21. Vypnutí spínače relé shodí i při zamrzlém firmwaru; bez modulu drží Q6 zavřený rezistor 47 kΩ. Při výpadku modulu drží pull-up 22 kΩ MOSFETy Q3–Q5 zavřené.

## Modul RP2350-Touch-LCD-2 na desce

Modul se zasouvá **displejem nahoru** do dvou dutinkových lišt: **J7 = lišta P1**, **J8 = lišta P2** (rozteč 30,48 mm), **USB-C k pravé hraně** desky. Deska je čtvercová – v autě ji otočte tak, aby displej byl na výšku. Modul drží v lištách; jeho vlastní díry zůstávají volné (případně podložit sloupkem).

| Lišta P1 (J7) | GPIO | Signál |
|---|---|---|
| 1 / 2 / 3 / 4 | 7 / 9 / 8 / 22 | UP / DOWN / READY / LOCKUP (aktivní HIGH, ~3,0 V) |
| 5 | 21 | relé K1–K3 (HIGH = přitáhnout, jen při READY) |
| 7 / 8 / 9 | 23 / 11 / 6 | SOL1 / SOL2 / SOL3 (HIGH = +12 V na solenoid) |
| 10 | 5 | rezerva (J6) |
| 6 / 11 / 12 | 18 / 20 / 19 | displej modulu – **nezapojeno** |
| 13 / 14 | – | GND / 5 V |

| Lišta P2 (J8) | GPIO | Signál |
|---|---|---|
| 1 / 2, 13 | – | 3V3 (napájí VIO budiče CAN) / GND |
| 8 / 10 | 1 / 0 | CAN TX / CAN RX (U7 MCP2562) |
| ostatní | – | nezapojeno |

**Programování modulu přes USB: sundejte JP1** (jinak by se 5 V z desky a z USB spojily).

## Součástky – všechno vývodové (THT)

Seznam k objednání i s počty: `bom_objednavka.xlsx`, pro DigiKey `digikey_bom.csv`. Hlavní díly:

| Ref | Součástka | Funkce |
|---|---|---|
| J7, J8 | dutinková lišta 1×14 (Sullins PPTC141LFBN-RC) | lišty pro modul RP2350-Touch-LCD-2 |
| U1 | Traco TSR 1-2450 | spínaný stabilizátor 12 → 5 V / 1 A |
| Q1 | IRF4905 | ochrana proti přepólování |
| Q3, Q4, Q5 | IRF9540N | high-side spínače solenoidů 1, 2, 3 |
| Q2, Q6 | BC337 | cívky relé / LED optočlenu relé z GPIO21 |
| K1, K2, K3 | Omron G5LE-1 DC12 | přepnutí solenoidu mezi původní TCU a desku |
| U3 / U4–U6 | PC817 (LTV-817) | optočlen relé / optočleny solenoidů |
| U7 | MCP2562-E/P | CAN budič (5 V / logika 3,3 V), J4 = CAN-H/CAN-L, JP2 = zakončení 120 Ω |
| R3/R5/R7/R9 + R4/R6/R8/R10, D3–D6 | 6k8 + 10k, BZX55C3V6 | dělič a ochrana vstupů 5 V → 3,3 V |
| D12, D14, D16 | 1N4007 | flyback diody na straně solenoidu (za relé) |
| D2 | P6KE27A | transil na napájení |
| R2 | 0 Ω | jediné spojení výkonové (PGND) a logické (GND) země |

## Deska plošných spojů

* **100 × 100 mm, 2 vrstvy, 1,6 mm**, 3× díra M3. **Měď 1 oz (35 µm) stačí** – při max. 2 solenoidech najednou (~2,6 A) se cesta +12 V 1,5 mm ohřeje o ~6 °C, cesta solenoidu 1,0 mm (1,3 A) o ~3 °C (IPC-2221); výkonové cesty nevedou přes žádný prokov.
* Na potisku je u každé součástky hodnota (10k, 6k8, 3V6, 4007, PC817 …), u konektorů názvy pinů, obrys modulu; označení součástek je v osazovacím výkresu.
* Šířky cest: +12 V a PGND **1,5 mm**, solenoidy/TCU **1,0 mm**, +5V/GND **0,6 mm**, signály 0,3 mm. Spodní vrstva: rozlitá měď PGND (výkonová část) a GND (logika), spojené jen přes R2.
* Rozvedeno autorouterem (Freerouting). **KiCad 10.0.6: DRC 0 chyb, 0 nepropojených, 0 rozdílů proti schématu, ERC 0 chyb / 0 varování.**
* Pod modulem jsou jen lišty; sklo displeje přesahuje modul cca o 10 mm doleva, ale je ~16 mm nad deskou (nad nízkými součástkami).

### Před objednáním ověřit
* **Polohu lišt modulu:** výkres Waveshare kótuje rozteč lišt (30,48 mm) a díry, ale ne vzdálenost prvního pinu od kraje modulu – deska počítá s lištami uprostřed (7,59 mm od kraje). Na funkci to nemá vliv (modul drží v lištách), jen na to, jak přesně lícuje obrys na potisku.
* Rozvedení z autorouteru stojí za to projít v KiCadu.
* Relé: pinout podle KiCad symbolu/footprintu G5LE-1 (cívka 2–5, COM 1, NC 4, NO 3).

## Revize zapojení (simulace ngspice, `tools/spice/`)

* **Vstupy:** sepnutý spínač 5 V → **2,97 V** na pinu; i 14,4 V omylem na vodiči vstupu zenerka omezí na 3,5 V.
* **Spínání solenoidů:** optočlen z GPIO přes 470 Ω (2,5–3,1 mA, pin RP2350 nepřetížený); s gate pull-upem 22 kΩ MOSFET spíná naplno (Vgs −11,3 V) i při optočlenu s CTR 25 % a GPIO 3,0 V. Vypnutí solenoidu ~0,9 ms, flyback drží −1 V.
* **Relé:** Q6 z GPIO21 (0,5 mA) sepne LED U3 (9,6 mA); při GPIO21 = 0 nebo vyjmutém modulu relé spolehlivě odpadnou.
* **Napájení 5 V:** modul s displejem + optočleny + CAN ≪ 1 A z TSR.

Co je potřeba ověřit na autě: polarita solenoidů (+12 V na drát), dobrá zem J1 na kostru, READY zapínat ve stoje (firmware začíná 1. stupněm), původní TCU po přepnutí zpět může jet v nouzovém režimu do dalšího otočení klíčkem. Deska zatím nemá rychlost vozidla – po zapojení CANu z rusEFI ji firmware použije k blokování podřazení.

## Firmware (RP2350)

`firmware/rp2350/uf2/aw4_rp2350.uf2` (pro auto) a `aw4_rp2350_sim.uf2` (simulátor na stole, ovládání ze Serial Monitoru). Nahrání: podržet BOOT, připojit USB, přetáhnout `.uf2` na disk RP2350. Zdrojáky v `firmware/rp2350/aw4_rp2350/`, test logiky + emulátor displeje v `firmware/rp2350/test/`. Menu (podsvícení, lockup, otočení) a CAN se doplní.

## Původní TCU
Po odpojení (READY zapnuto) vidí původní TCU rozpojený obvod a může si zapsat chybu. Atrapu zátěže na straně TCU nepoužívejte (NC kontakt je v klidu trvale na solenoidu).

## Jak se podklady generují (`tools/`)
`tools/pipeline.sh`: `kgen.py` (schéma, BOM) → ERC → `build_pcb.py` (rozmístění, netclassy, popisky) → Freerouting → `finish_pcb.py` (zóny) → `cleanup.py` (úklid, tepelné spojky) → `export.sh` (DRC, Gerbery, PDF, rendery). Seznam k objednání: `make_bom.py`. Cesty odpovídají prostředí, kde podklady vznikly.
