# AW4 shield – ovladač automatické převodovky Aisin AW4 pro Arduino Nano

Deska (shield) s Arduinem Nano, která:

* při přepínači **READY = vypnuto** nechá původní TCU připojené (převodovka běží jako sériově),
* při **READY = zapnuto** odpojí 3 vodiče solenoidů od původní TCU a převezme je (S1, S2, S3/lockup),
* čte spínače **UP, DOWN, READY, LOCKUP** (5 V z desky, aktivní = HIGH, s ochranou vstupu),
* zobrazuje zařazený stupeň na **displeji Nextion (UART)**, který má vlastní řadič a obraz si obnovuje sám, takže rušení na lince nanejvýš způsobí jeden chybný příkaz.

![PCB](docs/pcb_3d.png)

## Co je ve složce

| Cesta | Obsah |
|---|---|
| `kicad/` | KiCad projekt (schéma + PCB), formát KiCad 8, ověřeno v KiCadu 10.0.6 |
| `fab/aw4_shield_gerber.zip` | Gerbery + vrtání pro výrobu (JLCPCB, PCBWay, …) |
| `fab/drc_report.txt`, `fab/erc_report.txt` | výsledky kontrol z KiCadu |
| `docs/aw4_shield_schema_kicad.pdf` | schéma vyexportované z KiCadu (platné, se všemi referencemi) |
| `docs/aw4_shield.pdf` | přehledové / vysvětlující schéma (3 strany) |
| `docs/aw4_shield_pcb.pdf`, `docs/pcb_*.png` | výkres DPS po vrstvách a 3D náhledy |
| `bom.csv` | seznam součástek vygenerovaný ze schématu |
| `firmware/aw4_controller.ino` | základní firmware (doladí se později) |
| `tools/` | skripty, kterými se schéma i deska generují (viz konec) |

## Podle servisního manuálu AW-4

* Solenoid se měří mezi **držákem (kostra převodovky) a drátem** → druhý konec solenoidu je na kostře, TCU na drát spíná **+12 V (high-side)**. Výstupy desky jsou proto high-side (P-MOSFET).
* Odpor cívky 11–15 Ω → **0,8–1,25 A na solenoid** (při 14,4 V až 1,3 A). Všechny tři najednou max. ~3,9 A.
* Solenoid 1 + 2 řadí stupně, **solenoid 3 = lockup**.
* Ověřit multimetrem na autě: že drátem solenoidu opravdu jde z TCU +12 V a že vodiče S1/S2/S3 jsou správně rozlišené (přehozené S1 a S2 = špatné řazení).

| Poloha / stupeň | Solenoid 1 | Solenoid 2 |
|---|---|---|
| P, R, N | ON | OFF |
| 1. | ON | OFF |
| 2. | ON | ON |
| 3. | OFF | ON |
| 4. (OD) | OFF | OFF |

Lockup (solenoid 3): v originále ve 2. jen v poloze 1-2, ve 3. v poloze 3 a ve 3. a 4. v poloze D. Řadicí páka a manuální ventil zůstávají mechanické, deska řídí jen elektrické solenoidy.

## Jak deska funguje

```
 J1 +12V (zapalování) ─[F1 5A]─[Q1 IRF4905, ochrana proti přepólování]─┬─ +12V_SW ─ cívky relé K1–K3, MOSFETy Q3–Q5
                                    D2 SMBJ24A (TVS), C1 470µF, C2 100nF ┘
                                                                         └─[U1 TSR 1-2450]─[L1]─ +5V ─[JP1]─ Arduino Nano 5V
                                                                                                    └─ J3 spínače, J5 displej

 Vodiče solenoidů (3×) ──► K1/K2/K3 (přepínací kontakty SPDT):
        COM ← vodič k SOLENOIDU (J2: SOL1–3)
        NC  ← původní TCU (J2: TCU1–3) … klid = původní TCU řídí (fail-safe)
        NO  ← náš P-MOSFET Q3/Q4/Q5 (+12 V na solenoid)
```

### Stavy relé (fail-safe)

| READY přepínač | D8 | Relé K1–K3 | Solenoidy řídí |
|---|---|---|---|
| vypnuto | cokoli | odpadlé | původní TCU |
| zapnuto | HIGH / nezapojeno | odpadlé | původní TCU |
| zapnuto | LOW | přitažené | Arduino (Q3–Q5) |
| výpadek 12 V | – | odpadlé | původní TCU |

LED optočlenu relé (U3) je napájená **přes READY přepínač** a spíná ji pin D8 do země: vypnutí přepínače relé shodí i při zamrzlém firmwaru. Při výpadku Arduina drží pull-up 10 kΩ MOSFETy Q3–Q5 zavřené.

## Zapojení Arduina Nano

| Funkce | Pin | Poznámka |
|---|---|---|
| UP / DOWN / READY / LOCKUP | D4 / D5 / D6 / D7 | aktivní HIGH, 1 kΩ série, 10 kΩ pull-down, 100 nF, ESD |
| Relé K1–K3 | D8 | **LOW = přitáhnout** (jen při zapnutém READY) |
| SOL1 / SOL2 / SOL3 (lockup) | D9 / D10 / D11 | HIGH = +12 V na solenoid (opto U4/U5/U6 → Q3/Q4/Q5) |
| Displej Nextion | D3 (TX) / D2 (RX) | SoftwareSerial 9600 Bd, 100 Ω série + ESD |
| Rezerva (J6) | D12, D13, A0–A3 | D0/D1 jsou USB, nepoužité |

## Součástky (hlavní)

| Ref | Součástka | Funkce |
|---|---|---|
| U2 | Arduino Nano | řízení (do 2× 15pin patice) |
| U1 | Traco TSR 1-2450 | spínaný stabilizátor 12 → 5 V / 1 A (formát 7805, vstup 6,5–36 V) |
| Q1 | IRF4905 (TO-220) | ochrana proti přepólování |
| Q3, Q4, Q5 | IRF9540N (TO-220) | high-side spínače solenoidů 1, 2, 3 |
| Q2 | BC337 (TO-92) | spínání cívek relé |
| K1, K2, K3 | Omron G5LE-1-DC12 (nebo pinově kompatibilní Songle SRD-12VDC-SL-C) | přepnutí solenoidu mezi původní TCU a desku |
| U3 / U4–U6 | PC817 | optočlen relé / optočleny solenoidů |
| D12, D14, D16 | SS34 | flyback diody na straně solenoidu (za relé) |
| D2 | SMBJ24A | TVS na napájení |
| R2 | 0 Ω | jediné spojení výkonové (PGND) a logické (GND) země |
| F1 | 5 A, držák 5×20 mm | pojistka |

Kompletní seznam: `bom.csv`.

## Deska plošných spojů

* **145 × 100 mm, 2 vrstvy, 1,6 mm**, 4× díra M3 v rozích. Doporučuji objednat **měď 2 oz** (s 1 oz to také funguje, cesty se víc ohřejí).
* Rozmístění: nahoře napájení (J1 → F1 → Q1 → TVS/kondenzátory → U1), vlevo J2 (solenoidy/TCU) a tři relé s MOSFETy a optočleny pod nimi, vpravo Nano (USB přes pravou hranu) a budič relé, dole J3 (spínače), J5 (displej) a vstupní RC filtry.
* Šířky cest: +12 V a PGND **1,5 mm**, solenoidy/TCU **1,0 mm**, +5V/GND **0,6 mm**, signály 0,3 mm.
* Spodní vrstva: rozlitá měď **PGND** pod výkonovou částí a **GND** pod logikou, spojené jen přes R2.
* Cesty rozvedl autorouter (Freerouting). Kontroly v KiCadu 10.0.6: **DRC 0 chyb, 0 nepropojených, 0 rozdílů proti schématu, ERC 0 chyb / 0 varování.**
* Svorkovnice mají otvory pro vodiče ven z desky, na potisku jsou popsané piny (SOL1…TCU3, +12V/GND, +5V…LOCK, 5V/GND/RX/TX).

### Výroba
Nahrajte `fab/aw4_shield_gerber.zip` k výrobci (2 vrstvy, 1,6 mm, 2 oz Cu, HASL). Součástky jsou kombinace SMD 0805/SOD/SMA a vývodových (relé, TO-220, TO-92, svorkovnice), vše jde pájet ručně.

### Před objednáním zkontrolovat v KiCadu
* Rozvedení je z autorouteru: funkčně kompletní a podle DRC v pořádku, ale stojí za to projít. Řídicí signály SOLn_CTRL vedou po spodní vrstvě přes plochu PGND – pro pomalé signály s filtrem přijatelné.
* Footprint pojistkového držáku (Schurter 0031.8201, 5×20 mm) – případně vyměnit za držák, který máte.
* Relé: číslování pinů je podle KiCad symbolu a footprintu G5LE-1 (cívka 2–5, COM 1, NC 4, NO 3). U jiného relé ověřit pinout.

## Napájení Nana a programování
JP1 spojuje +5 V desky s pinem 5V Nana. **Při programování po USB jumper sundejte** (nebo odpojte 12 V), nikdy nenapájet z obou stran zároveň.

## Původní TCU
Po odpojení (READY zapnuto) vidí původní TCU rozpojený obvod a může si zapsat chybu. Atrapu zátěže na straně TCU nepoužívejte: NC kontakt je v klidu trvale připojený k solenoidu, atrapa by byla neustále paralelně a TCU by viděla poloviční odpor.

## Displej Nextion
Nextion Basic NX3224T024 (2,4″) nebo NX4024T032 (3,2″), 5 V, UART. Má vlastní řadič a obraz si obnovuje sám; firmware stav posílá znovu každých ~0,5 s. Kabel k displeji kroucený nebo stíněný (stínění jen na straně desky).

## Firmware
`firmware/aw4_controller.ino` – základní logika (UP/DOWN, lockup, READY, displej Nextion přes SoftwareSerial). Bude se dolaďovat. Pozor: D8 je aktivní LOW.

## Jak byly schéma a deska vytvořené (`tools/`)
Schéma generuje `tools/kgen.py` (sítě, symboly, footprinty, BOM), desku `tools/build_pcb.py` (rozmístění, netclassy, zóny) přes Python API KiCadu 10, rozvedení Freerouting (`run_all.sh`), úklid a exporty `cleanup.py` a `export.sh`. Cesty ve skriptech odpovídají prostředí, kde vznikly, a je potřeba je upravit. Další úpravy se dají dělat rovnou v KiCadu: schéma i deska jsou běžné KiCad soubory a *Update PCB from Schematic* funguje.
