# AW4 shield – ovladač automatické převodovky Aisin AW4 pro Arduino Micro

Deska (shield) s Arduinem Micro, která:

* při přepínači **READY = vypnuto** nechá původní TCU připojené (převodovka běží jako sériově),
* při **READY = zapnuto** odpojí 3 vodiče solenoidů od původní TCU a převezme je (S1, S2, S3/lockup),
* čte tlačítka **UP, DOWN, READY, LOCKUP** (12 V, přes optočleny),
* zobrazuje zařazený stupeň na **7segmentovce řízené přímo na desce** (74HC595, bez dlouhých vodičů).

> **Co ověřit multimetrem před výrobou** (celý návrh na tom stojí):
> 1. Solenoidy AW4 jsou běžně napájené spínaným +12 V (zapalování) a TCU je spíná **na zem** (low-side). Návrh s tím počítá. Pokud je to u vás jinak, napište mi.
> 2. Odpor cívek (typicky 10–15 Ω → cca 1 A na solenoid). Pokud je lockup spínaný PWM, drží se návrh MOSFETů, ne relé.
> 3. Zda jsou vstupy UP/DOWN/READY/LOCKUP spínače na +12 V, nebo na zem (viz sekce Vstupy, přepíná se pájecím jumperem).

## Blokové schéma

```
 +12V (zapalování) ──[F1 3A]──[D1 SS34 / ochrana proti přepólování]──┬─ +12V_SW ── cívky relé K1-K3
                                                                       │
                                  TVS D2 SMBJ24A ─ GND                 │
                                                                       └─[buck 5 V / 1 A]── +5V ── Arduino Micro (pin 5V)
                                                                                                   └─ 74HC595, optočleny (LED strana)

 Vodiče solenoidů (3×)  ──► K1/K2/K3 (přepínací kontakty):
        COM  ← vodič ke SOLENOIDU (strana, kde TCU spíná zem)
        NC   ← původní TCU (klid = serial TCU funguje, fail-safe)
        NO   ← náš MOSFET Q1/Q2/Q3 (spíná zem)
```

Důležité: kontakty relé K1–K3 jsou **přepínací (SPDT)**, takže jeden relé = jeden solenoid přepnutý buď na původní TCU, nebo na náš výkonový stupeň. Když Arduino nebo napájení vypadne, relé odpadnou a převodovku ovládá zase originální TCU.
Tři relé K1–K3 se ovládají **jedním signálem** (`READY_RELAYS`), cívky jsou paralelně (3 × ~40 mA).

## Zapojení Arduina Micro

| Funkce | Pin Micro | Směr | Poznámka |
|---|---|---|---|
| UP | D4 | vstup | opto, aktivní = LOW |
| DOWN | D5 | vstup | opto, aktivní = LOW |
| READY | D6 | vstup | opto, aktivní = LOW |
| LOCKUP | D7 | vstup | opto, aktivní = LOW |
| READY_RELAYS (K1–K3) | D8 | výstup | HIGH = převzít solenoidy |
| SOL1 (S1) | D9 | výstup (PWM) | → opto → Q1 |
| SOL2 (S2) | D10 | výstup (PWM) | → opto → Q2 |
| SOL3 (lockup) | D11 | výstup (PWM) | → opto → Q3 |
| 595 SER (DS) | D12 | výstup | zobrazení |
| 595 SRCLK (SHCP) | D13 | výstup | zobrazení |
| 595 RCLK (STCP) | A0 | výstup | zobrazení |
| D2/D3 | – | volné | I2C na rezervu (header J6) |

## Jednotlivé bloky

### 1. Napájení
* Konektor 12 V → pojistka F1 (3 A) → Schottky D1 (SS34) proti přepólování → TVS D2 (SMBJ24A, ochrana proti load dump) → bulk C1 470 µF/25 V + C2 100 nF.
* **Buck 12 → 5 V**: LMR14006 / TPS54302 nebo hotový modul (MP1584), 1 A, výstup 5 V do pinu `5V` Arduina Micro (jumper J1: napájení přes USB / přes desku, **nikdy obojí zároveň**; při programování po USB sundat jumper nebo přidat Schottky OR diodu D3).
* LC filtr 5 V: ferit (BLM21) + C 47 µF + C 100 nF.
* **Zem do hvězdy**: výkonová zem solenoidů (PGND) a logická zem (GND) se na desce spojují v **jednom místě** (pad R0 = 0 Ω / ferit). Tím šum ze solenoidů neteče přes zem Arduina.

### 2. Výkonové výstupy solenoidů (×3)
Pro každý kanál:

```
 +5V ─[R 330 Ω]─ LED opto (PC817 / TLP185) ─ D9 (Arduino)           ← logická strana
                       │ (optočlen)
 +12V_SW ─[R 1 kΩ]─ fototranzistor ─ gate Q1 ─[R 10 kΩ]─ PGND       ← výkonová strana
                                      └─[R 100 Ω, gate]
 Q1 = IRLZ44N (TO-220) nebo AO3400A (SMD, ≤ 3 A pro 1 A solenoid je stačí)
 D4 = flyback SS34 (anoda na drain Q1, katoda na +12V_SW) + TVS SMBJ30A drain–PGND
 NO kontakt K1 ← drain Q1
```

Proč MOSFET místo relé pro solenoidy: lockup se často PWM-uje, relé na tom umírá (mechanická životnost ~10⁵ cyklů, spínací čas 5–10 ms). Jestli přesto chcete relé, výstup lze nahradit modulem s optočlenem a diodou – schéma zůstává stejné.

### 3. Přepínací relé K1–K3
* 3× relé SPDT 12 V, kontakty min. 5 A (např. Omron G5LE-14 / Songle SRD-12VDC-SL-C).
* Cívky z +12V_SW, spínání NPN tranzistorem (BC337/2N2222) s optočlenem z D8 + flyback dioda 1N4148 na každé cívce.
* LED indikace READY (zelená).

### 4. Vstupy (×4: UP, DOWN, READY, LOCKUP)
```
 IN (12 V z přepínače) ─[R 4,7 kΩ]─ LED opto (PC817) ─ GND_IN      ← dioda 1N4148 antiparalelně na LED
                                                          └ TVS SMAJ15A IN–GND
 Opto výstup: kolektor → pin Arduina (INPUT_PULLUP), emitor → GND
 C 100 nF na pinu Arduina (debounce hardware)
```
Pájecí jumper JP_IN: přepnutí na vstup „spíná na zem" (pull-up 4,7 kΩ na +12V_SW, přepínač do GND), pokud jsou vaše spínače zapojené jako zemnící.

### 5. Zobrazení
* 74HC595 (SOIC-16 / DIP-16) + 1× 7segmentovka 0,56" (společná katoda), 8× 330 Ω.
* Zobrazuje stupeň **1–4**, tečka (DP) = lockup aktivní. Při ztrátě READY zobrazí `-`.
* Pár desítek mm od Arduina → odolné proti rušení; lze oddělit na header J5 (8 pinů) k vyvedení na palubní desku.
* C 100 nF + 10 µF u 595, SRCLR na 5 V, OE na GND.

### 6. Konektory
| Ref | Typ | Signály |
|---|---|---|
| J1 | 2pin šroubovací 5.08 | +12V, GND |
| J2 | 6pin šroubovací | SOL1, SOL2, SOL3 (k převodovce), TCU1, TCU2, TCU3 (z původní TCU) |
| J3 | 4pin šroubovací | UP, DOWN, READY, LOCKUP |
| J4 | 2pin | +12V_SW pro solenoidy (společné napájení) |
| J5 | 8pin header | displej mimo desku (volitelné) |
| J6 | 4pin header | D2/D3 + 5V/GND rezerva |

## Seznam součástek (BOM, orientační)
Viz `bom.csv`.

## Doporučená konstrukce PCB
* 2 vrstvy, 1,6 mm, 2 oz měď (kvůli proudům solenoidů); rozměr ≈ 75 × 65 mm (zapadne pod Micro ve spodní vrstvě – Micro se připojuje do 2× 17pin female header).
* Dráhy výkonu min. 1 mm, PGND plocha oddělená od logické zem a spojená jen v bodě hvězdy.
* Optočleny napříč dělicí linkou GND – mezi logickou a výkonovou zemí vyfrézovat mezeru (~2 mm).
* Šroubovací svorky u okraje, ochranné diody (TVS, flyback) fyzicky u svorek.
* Krabička: plastová, odvětraná, kabeláž ke svorkám stíněná (alespoň vodiče tlačítek, kroucená dvojice).

## Firmware
`firmware/aw4_controller.ino` – základní logika (UP/DOWN, lockup, READY, 595 displej). Tabulka solenoidů je v `GEAR_TABLE` a **musí se ověřit** na vaší převodovce.

## Co dál
Návrh je zatím popis + BOM + firmware. Dalším krokem je překreslit do KiCadu (schéma + PCB). Řekněte, jestli ho mám připravit, a upřesněte body z úvodu (polarita solenoidů, odpor cívek, typ spínačů).
