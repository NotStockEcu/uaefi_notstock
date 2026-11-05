// AW4 – ovladač převodovky Aisin AW-4, deska Waveshare RP2350-Touch-LCD-2 na AW4 shield v0.4.
//
// Verze pro auto: čte spínače UP / DOWN / READY / LOCKUP, spíná relé K1–K3 a solenoidy S1–S3.
// Verze simulátor (přeloženo s -DAW4_SIM): spínače se nečtou, ovládá se písmeny ze sériové linky:
//   r = READY, u/+ = nahoru, d/- = dolů, l = LOCKUP spínač, a = automatická ukázka, s = stav, h = nápověda.
//
// Piny na liště P1 modulu (deska v0.4):
//   UP=GPIO7 (P1-1), DOWN=GPIO9 (P1-2), READY=GPIO8 (P1-3), LOCKUP=GPIO22 (P1-4)
//   RELÉ=GPIO21 (P1-5, aktivní HIGH), SOL1=GPIO23 (P1-7), SOL2=GPIO11 (P1-8), SOL3=GPIO6 (P1-9)

#include <Arduino.h>
#include "lcd_st7789.h"
#include "aw4_fonts.h"

const uint8_t PIN_UP = 7, PIN_DOWN = 9, PIN_READY = 8, PIN_LOCKUP = 22;
const uint8_t PIN_RELAYS = 21, PIN_SOL1 = 23, PIN_SOL2 = 11, PIN_SOL3 = 6;

// Solenoidy S1, S2 podle stupně (1..4) – servisní manuál AW-4, Fig. 8.
const bool GEAR_TABLE[4][2] = {
  {true,  false},  // 1.
  {true,  true},   // 2.
  {false, true},   // 3.
  {false, false},  // 4.
};
const uint8_t LOCKUP_MIN_GEAR = 3;
// POZOR: po zapnutí READY nevíme, jaký stupeň zrovna drží původní TCU. Zapínat READY ve stoje.
const uint8_t INITIAL_GEAR = 1;
const uint32_t DEBOUNCE_MS = 30;

#ifdef AW4_SIM
const bool SIM = true;
#else
const bool SIM = false;
#endif

// ---------------- stav ----------------
struct Button { uint8_t pin; bool state; bool last; uint32_t t; };
Button btns[4] = {{PIN_UP, false, false, 0}, {PIN_DOWN, false, false, 0},
                  {PIN_READY, false, false, 0}, {PIN_LOCKUP, false, false, 0}};
enum { B_UP, B_DOWN, B_READY, B_LOCKUP };

uint8_t gear = INITIAL_GEAR;
bool ready = false, lockup = false;
bool simReady = false, simLockup = false, simUp = false, simDown = false, autoDemo = false;

// Aktualizuje stav tlačítka (debounce) a vrátí true v okamžiku sepnutí.
bool pressed(Button &b) {
  bool v = digitalRead(b.pin) == HIGH;
  if (v != b.last) { b.last = v; b.t = millis(); }
  if (millis() - b.t > DEBOUNCE_MS && v != b.state) {
    b.state = v;
    return v;
  }
  return false;
}

bool solOut(uint8_t i) {   // co právě jde na výstup solenoidu i (0..2)
  if (!ready) return false;
  if (i < 2) return GEAR_TABLE[gear - 1][i];
  return lockup;
}

void applyOutputs() {
  if (ready) {
    // nejdřív nastavit výstupy, pak teprve přepnout relé na naše MOSFETy
    digitalWrite(PIN_SOL1, solOut(0));
    digitalWrite(PIN_SOL2, solOut(1));
    digitalWrite(PIN_SOL3, solOut(2));
    digitalWrite(PIN_RELAYS, HIGH);
  } else {
    // nejdřív vrátit relé na původní TCU, pak vypnout naše výstupy
    digitalWrite(PIN_RELAYS, LOW);
    digitalWrite(PIN_SOL1, LOW);
    digitalWrite(PIN_SOL2, LOW);
    digitalWrite(PIN_SOL3, LOW);
  }
}

// ---------------- displej ----------------
// Na výšku 240 x 320, černé pozadí:
//   nahoře  y 0–27:    režim (TCU / MANUAL), vpravo SIM
//   střed   y 34–281:  sedmisegmentová číslice přes celou šířku (zelená, nesvítící segmenty tmavě zelené)
//   dole    y 290–:    LOCKUP vlevo, stav solenoidů S1 S2 S3 vpravo
using namespace lcd;
const int DG_X = 18, DG_Y = 34, DG_W = 204, DG_H = 248, DG_TH = 34;
const uint16_t SEG_ON = GREEN, SEG_DIM = 0x0320, SEG_OFF = 0x00E0;   // svítí / pomlčka v režimu TCU / nesvítí

struct Shown { int mode = -1; uint8_t mask = 0; int sol[3] = {-1, -1, -1}; int lock = -1; } shown;
bool forceDigit = true;

void drawStatic() {
  fillRect(0, 0, W, H, BLACK);
  if (SIM) text(W - 6 - textWidth(fontSmall, "SIM"), 1, "SIM", fontSmall, YELLOW, BLACK);
  shown = Shown();
  forceDigit = true;
}

void updateDisplay(bool force = false) {
  int mode = ready ? 1 : 0;
  bool modeChanged = mode != shown.mode;
  if (force || modeChanged) {
    shown.mode = mode;
    textBox(4, 1, 150, ready ? "MANUAL" : "TCU", fontSmall, ready ? GREEN : GREY, BLACK);
  }
  uint8_t mask = segMask(ready ? gear : 10);
  if (force || forceDigit || mask != shown.mask || modeChanged) {
    // při změně režimu se mění barva svítících segmentů -> překreslit vše
    bool all = force || forceDigit || modeChanged;
    sevenSeg(DG_X, DG_Y, DG_W, DG_H, DG_TH, mask, shown.mask, ready ? SEG_ON : SEG_DIM, SEG_OFF, all);
    shown.mask = mask; forceDigit = false;
  }
  if (force || int(lockup) != shown.lock) {
    shown.lock = lockup;
    textBox(4, 292, 110, "LOCKUP", fontSmall, lockup ? ORANGE : DKGREY, BLACK);
  }
  const char *sl[3] = {"S1", "S2", "S3"};
  for (int i = 0; i < 3; i++) {
    int on = solOut(i);
    if (force || on != shown.sol[i]) { shown.sol[i] = on; textBox(132 + i * 36, 292, 34, sl[i], fontSmall, on ? GREEN : DKGREY, BLACK); }
  }
}

// ---------------- sériová linka ----------------
void printState(const char *what) {
  Serial.printf("%-18s | READY=%s stupen=%c lockup=%s | rele %s | S1=%d S2=%d S3=%d\n", what,
                ready ? "ZAP" : "vyp", ready ? char('0' + gear) : '-', lockup ? "ZAP" : "vyp",
                ready ? "PRITAZENA (ridi AW4)" : "odpadla (ridi TCU)", solOut(0), solOut(1), solOut(2));
}

void help() {
  Serial.println("\n=== AW4 – RP2350 ===");
  if (SIM) {
    Serial.println("SIMULATOR: r = READY, u/+ = nahoru, d/- = dolu, l = LOCKUP spinac, a = ukazka, s = stav, h = napoveda");
    Serial.println("Spinace na pinech se v simulatoru NEctou.");
  } else {
    Serial.println("Verze pro auto: ridi se spinaci UP/DOWN/READY/LOCKUP. s = stav, h = napoveda.");
  }
}

void serialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 's' || c == 'S') printState("STAV");
    else if (c == 'h' || c == 'H' || c == '?') help();
    else if (SIM) {
      if (c == 'r' || c == 'R') simReady = !simReady;
      else if (c == 'l' || c == 'L') simLockup = !simLockup;
      else if (c == 'u' || c == 'U' || c == '+') simUp = true;
      else if (c == 'd' || c == 'D' || c == '-') simDown = true;
      else if (c == 'a' || c == 'A') { autoDemo = !autoDemo; Serial.println(autoDemo ? "ukazka ZAP" : "ukazka VYP"); }
    }
  }
}

void demoStep() {   // READY -> 1 -> 2 -> 3 -> lockup -> 4 -> 3 -> 2 -> 1 -> lockup vyp -> READY vyp
  static uint8_t step = 0; static uint32_t last = 0;
  if (millis() - last < 1500) return;
  last = millis();
  const char seq[] = "ruuludddlr";
  char c = seq[step];
  if (c == 'r') simReady = !simReady; else if (c == 'l') simLockup = !simLockup;
  else if (c == 'u') simUp = true; else if (c == 'd') simDown = true;
  step = (step + 1) % (sizeof(seq) - 1);
}

// ---------------- hlavní smyčka ----------------
void setup() {
  digitalWrite(PIN_RELAYS, LOW);   // relé odpadlá ještě před nastavením výstupu
  pinMode(PIN_RELAYS, OUTPUT); digitalWrite(PIN_RELAYS, LOW);
  pinMode(PIN_SOL1, OUTPUT); pinMode(PIN_SOL2, OUTPUT); pinMode(PIN_SOL3, OUTPUT);
  digitalWrite(PIN_SOL1, LOW); digitalWrite(PIN_SOL2, LOW); digitalWrite(PIN_SOL3, LOW);
  for (auto &b : btns) { pinMode(b.pin, INPUT_PULLDOWN); b.state = b.last = SIM ? false : (digitalRead(b.pin) == HIGH); }
  Serial.begin(115200);
  lcd::begin();
  drawStatic();
  updateDisplay(true);
  lcd::backlight(100);
  help();
}

void loop() {
  serialCommands();
  if (SIM && autoDemo) demoStep();

  bool up, down, readyIn, lockIn;
  if (SIM) {
    up = simUp; down = simDown; simUp = simDown = false;
    readyIn = simReady; lockIn = simLockup;
  } else {
    up = pressed(btns[B_UP]); down = pressed(btns[B_DOWN]);
    pressed(btns[B_READY]); pressed(btns[B_LOCKUP]);
    readyIn = btns[B_READY].state; lockIn = btns[B_LOCKUP].state;
  }

  const char *what = nullptr;
  if (readyIn != ready) { ready = readyIn; if (ready) gear = INITIAL_GEAR; what = ready ? "READY zapnuto" : "READY vypnuto"; }
  if (ready) {
    if (up && gear < 4) { gear++; what = "NAHORU"; }
    if (down && gear > 1) { gear--; what = "DOLU"; }
  }
  bool newLockup = ready && lockIn && gear >= LOCKUP_MIN_GEAR;
  if (newLockup != lockup) { lockup = newLockup; if (!what) what = lockup ? "LOCKUP zap" : "LOCKUP vyp"; }

  applyOutputs();   // výstupy každou smyčku
  updateDisplay();  // kreslí jen to, co se změnilo
  if (what) printState(what);

  static uint32_t lastRegs = 0;
  if (millis() - lastRegs > 5000) { lastRegs = millis(); lcd::refreshRegs(); }
}
