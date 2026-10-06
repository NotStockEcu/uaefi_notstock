# AW4 shield – ovladač automatické převodovky Aisin AW4 pro Arduino Nano

Deska (shield) s Arduinem Nano, která:

* při přepínači **READY = vypnuto** nechá původní TCU připojené (převodovka běží jako sériově),
* při **READY = zapnuto** odpojí 3 vodiče solenoidů od původní TCU a převezme je (S1, S2, S3/lockup),
* čte spínače **UP, DOWN, READY, LOCKUP** (5 V z Arduina, aktivní = HIGH, s ochranou vstupu),
* zobrazuje zařazený stupeň na **displeji Nextion (UART)**, který má vlastní řadič a obraz si obnovuje sám, takže rušení na lince nanejvýš způsobí jeden chybný příkaz.

> **Podle servisního manuálu AW-4 (v zipu, str. 5-6 a Solenoid Testing):**
> * Solenoid se měří mezi **držákem (kostra převodovky) a drátem** → druhý konec solenoidu je na kostře, TCU na drát spíná **+12 V (high-side)**. Výstupy desky jsou proto high-side (P-MOSFET), ne low-side.
> * Odpor cívky 11–15 Ω → **0,8–1,25 A na solenoid** (při 14,4 V až 1,3 A). Všechny tři najednou max. ~3,9 A.
> * Solenoid 1 + 2 řadí stupně, **solenoid 3 = lockup** (kanál tlaku do spojky měniče přes lockup relay valve).
> * Zbývá ověřit multimetrem na vašem autě: že drátem solenoidu opravdu teče z TCU +12 V a že vodiče S1/S2/S3 jsou v konektoru rozlišené (přehodíte-li S1 a S2, bude řazení špatně).

## Stupně a solenoidy (z manuálu, Fig. 8)

| Poloha / stupeň | Solenoid 1 | Solenoid 2 |
|---|---|---|
| P, R, N | ON | OFF |
| 1. | ON | OFF |
| 2. | ON | ON |
| 3. | OFF | ON |
| 4. (OD) | OFF | OFF |

Lockup (solenoid 3): v originále ve 2. jen v poloze 1-2, ve 3. v poloze 3 a ve 3. a 4. v poloze D. Řadicí páka (kabel) a manuální ventil zůstávají mechanické, deska řídí jen elektrické solenoidy. Páka musí být v D (nebo 3), jinak se zařazuje jen to, co dovolí mechanický ventil.

## Blokové schéma

```
 +12V (zapalování) ──[F1 5A]──[Q0 P-MOSFET proti přepólování]──┬─ +12V_SW ── cívky relé K1-K3
                                                                       │
                                  TVS D2 SMBJ24A ─ GND                 │
                                                                       └─[buck 5 V / 1 A]── +5V ── Arduino Nano (pin 5V)
                                                                                                   └─ displej Nextion, optočleny (LED strana)

 Vodiče solenoidů (3×)  ──► K1/K2/K3 (přepínací kontakty):
        COM  ← vodič k SOLENOIDU (druhý konec solenoidu je na kostře převodovky)
        NC   ← původní TCU (klid = serial TCU funguje, fail-safe)
        NO   ← náš P-MOSFET Q1/Q2/Q3 (spíná +12 V na solenoid)
```

Důležité: kontakty relé K1–K3 jsou **přepínací (SPDT)**, takže jeden relé = jeden solenoid přepnutý buď na původní TCU, nebo na náš výkonový stupeň. Když Arduino nebo napájení vypadne, relé odpadnou a převodovku ovládá zase originální TCU.
Tři relé K1–K3 se ovládají **jedním signálem** (`READY_RELAYS`), cívky jsou paralelně (3 × ~40 mA).

## Zapojení Arduina Nano

| Funkce | Pin Nano | Směr | Poznámka |
|---|---|---|---|
| UP | D4 | vstup | aktivní = HIGH, pull-down na desce |
| DOWN | D5 | vstup | aktivní = HIGH, pull-down na desce |
| READY | D6 | vstup | aktivní = HIGH, pull-down na desce |
| LOCKUP | D7 | vstup | aktivní = HIGH, pull-down na desce |
| READY_RELAYS (K1–K3) | D8 | výstup | LOW = převzít solenoidy (a zároveň READY přepínač zapnutý) |
| SOL1 (S1) | D9 | výstup (PWM) | → opto → Q1 |
| SOL2 (S2) | D10 | výstup (PWM) | → opto → Q2 |
| SOL3 (lockup) | D11 | výstup (PWM) | → opto → Q3 |
| Displej RX (Nano TX) | D3 | výstup | SoftwareSerial TX → Nextion RX |
| Displej TX (Nano RX) | D2 | vstup | SoftwareSerial RX ← Nextion TX |
| D12, D13, A0–A3 | – | volné | rezerva (header J6); D0/D1 jsou USB, nepoužít |

## Jednotlivé bloky

### 1. Napájení
* Konektor 12 V → pojistka F1 (5 A) → P-MOSFET Q0 (IRF4905 / AO4407A, zener 12 V gate–source, rezistor 100 kΩ gate–GND) proti přepólování → TVS D2 (SMBJ24A, ochrana proti load dump) → bulk C1 470 µF/25 V + C2 100 nF.
* **Buck 12 → 5 V**: LMR14006 / TPS54302 nebo hotový modul (MP1584), 1 A, výstup 5 V do pinu `5V` Arduina Nano (jumper J1: napájení přes USB / přes desku, **nikdy obojí zároveň**; při programování po USB sundat jumper nebo přidat Schottky OR diodu D3).
* LC filtr 5 V: ferit (BLM21) + C 47 µF + C 100 nF.
* **Zem do hvězdy**: výkonová zem solenoidů (PGND) a logická zem (GND) se na desce spojují v **jednom místě** (pad R0 = 0 Ω / ferit). Tím šum ze solenoidů neteče přes zem Arduina.

### 2. Výkonové výstupy solenoidů (×3) – high-side
Solenoid má jeden konec na kostře, takže kanál musí přivést +12 V na drát (P-MOSFET ve zdroji). Pro každý kanál:

```
 +5V ─[R 330 Ω]─ LED opto (PC817) ─ pin Arduina (D9/D10/D11)           ← logická strana

 +12V_SW ──┬───────────────┬── source Q1 (IRF9540N / AO4407A)
           │               │
        [R 10 kΩ]       Z1 15 V zener (BZX84C15), anoda ke gate
           │               │
           └──── gate Q1 ──┴──[R 1 kΩ]── kolektor fototranzistoru ── emitor ── PGND
 drain Q1 ───────────────────────────────► kontakt NO relé Kx
 D4: Schottky SS34 na straně solenoidu (COM relé): anoda PGND, katoda COM  ← flyback
```

Opto zapnuto → fototranzistor stáhne gate k zemi (Vgs ≈ −11 V) → Q1 vede, na solenoid jde +12 V. Opto vypnuto → pull-up 10 kΩ drží Q1 zavřený (při výpadku Arduina jsou solenoidy bez proudu).
Flyback dioda D4 je záměrně až **za relé na straně solenoidu**, takže chrání i při rozpojení relé pod proudem. Dráhy +12V_SW → Q1 → relé → svorka min. 1,5 mm, měď 2 oz (do 4 A).

Proč MOSFET místo relé pro solenoidy: tlak lockupu se dnes moduluje jen zapnuto/vypnuto, ale relé má životnost ~10⁵ cyklů a spínací čas 5–10 ms, MOSFET je tichý a bez opotřebení.

**Volitelně:** zátěž na straně TCU (NC kontakt) – rezistor 12 Ω / 5 W na každý vodič, ať TCU po odpojení nehlásí „open circuit" a nehází chybu. Topí se (až 12 W na kanál při sepnutí TCU), proto jen jako nepájená varianta.

### 3. Přepínací relé K1–K3 (fail-safe na původní TCU)
* 3× relé SPDT 12 V, kontakty min. 5 A (např. Omron G5LE-14 / Songle SRD-12VDC-SL-C). Cívky paralelně, 3 × ~40 mA.
* **Klid / výpadek napájení / READY vypnuto** → cívky bez proudu → COM–NC → solenoidy řídí původní TCU.
* **READY zapnuto** → COM–NO → solenoidy řídí naše P-MOSFETy, TCU je rozpojená.
* Spínání NPN tranzistorem (BC337) s optočlenem + flyback dioda 1N4148 na cívkách.
* **Hardwarové AND:** LED optočlenu relé je napájená z +5 V **přes READY přepínač** (signál READY_5V z J3) a zároveň zapínaná pinem D8:

```
 READY_5V (z přepínače) ─[R 330 Ω]─ LED opto (PC817) ─ pin D8 (aktivní LOW na straně pinu)
```
  Relé tedy přitáhne jen při zapnutém přepínači **a** HIGH/LOW z Arduina ve správné úrovni. Vypnutí přepínače relé shodí i při zamrzlém firmwaru. (Úroveň pinu D8 se v návrhu otočí: D8 = LOW přitahuje.)
* LED indikace READY (zelená) na cívce relé.

### 4. Vstupy (×4: UP, DOWN, READY, LOCKUP)
Spínače jsou vaše a spínají **+5 V z Arduina** na vstupní pin (aktivní = HIGH). Pro každý vstup:

```
 +5V (z desky) ── spínač ──┬─[R 1 kΩ]──┬── pin Arduina (D4..D7)
                           │           ├─ C 100 nF ─ GND   (debounce, odrušení)
                           └─[R 10 kΩ pull-down]─ GND
                                       └─ ESD/TVS dioda 5V (např. PESD5V0S1BL) pin–GND
```

Pull-down 10 kΩ drží vstup v LOW při rozepnutém spínači (a při odpojeném kabelu), 1 kΩ + 100 nF tvoří RC filtr proti rušení z kabeláže spínačů. Optočleny na vstupech nejsou potřeba, 12 V se k nim nedostane. Kabel ke spínačům vést stíněný nebo kroucený (5 V, GND, signál).

### 5. Zobrazení – Nextion (UART)
* Doporučený typ: Nextion Basic NX3224T024 (2,4″) nebo NX4024T032 (3,2″), 5 V, UART, ~100 až 250 mA.
* Propojení: SoftwareSerial (D3 = TX, D2 = RX, 9600 Bd; D0/D1 zůstávají pro USB) přes 100 Ω v sérii s každým signálem, ESD dioda PESD5V0S1BL k GND na obou signálech, napájení 5 V přes ferit L2 s 100 µF + 100 nF u konektoru.
* Displej si obraz obnovuje sám (vlastní řadič). Rušení na lince způsobí nanejvýš jeden chybný příkaz. Firmware proto stav displeje periodicky (každých ~0,5 s) posílá znovu.
* Kabel ke displeji: stíněný nebo kroucený, stínění jen na straně desky.

### 6. Konektory
| Ref | Typ | Signály |
|---|---|---|
| J1 | 2pin šroubovací 5.08 | +12V, GND |
| J2 | 6pin šroubovací | SOL1, SOL2, SOL3 (k převodovce), TCU1, TCU2, TCU3 (z původní TCU) |
| J3 | 6pin šroubovací | +5V, GND, UP, DOWN, READY, LOCKUP |
| J4 | 2pin | rezerva (např. tlakový solenoid) |
| J5 | 4pin (JST-XH / šroubovací) | displej Nextion: +5V, GND, RX, TX |
| J6 | 6pin header | D12, D13, A0–A3 + GND (rezerva) |

## Schéma (PDF)
`docs/aw4_shield.pdf` – 3 strany: přehled, napájení + vstupy + displej, výstupní stupeň a přepínací relé.

## Seznam součástek (BOM, orientační)
Viz `bom.csv`.

## Doporučená konstrukce PCB
* 2 vrstvy, 1,6 mm, 2 oz měď (kvůli proudům solenoidů); rozměr ≈ 75 × 65 mm (zapadne pod Nano ve spodní vrstvě – Nano se připojuje do 2× 17pin female header).
* Dráhy výkonu min. 1 mm, PGND plocha oddělená od logické zem a spojená jen v bodě hvězdy.
* Optočleny napříč dělicí linkou GND – mezi logickou a výkonovou zemí vyfrézovat mezeru (~2 mm).
* Šroubovací svorky u okraje, ochranné diody (TVS, flyback) fyzicky u svorek.
* Krabička: plastová, odvětraná, kabeláž ke svorkám stíněná (alespoň vodiče tlačítek, kroucená dvojice).

## Firmware
`firmware/aw4_controller.ino` – základní logika (UP/DOWN, lockup, READY, displej Nextion přes SoftwareSerial). Tabulka solenoidů je v `GEAR_TABLE` a **musí se ověřit** na vaší převodovce.

## Co dál
Návrh je zatím popis + BOM + firmware. Dalším krokem je překreslit do KiCadu (schéma + PCB). Řekněte, jestli ho mám připravit, a upřesněte body z úvodu (polarita solenoidů, odpor cívek, typ spínačů).
