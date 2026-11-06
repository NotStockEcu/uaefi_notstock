// AW4 – ovladač převodovky Aisin AW-4, deska Waveshare RP2350-Touch-LCD-2 na AW4 shield v0.4.
//
// Verze pro auto: čte spínače UP / DOWN / READY / LOCKUP, spíná relé K1–K3 a solenoidy S1–S3.
// Verze simulátor (přeloženo s -DAW4_SIM): spínače se nečtou, ovládá se písmeny ze sériové linky:
//   r = READY, u/+ = nahoru, d/- = dolů, l = LOCKUP spínač, a = automatická ukázka, s = stav, h = nápověda.
//
//   (simulátor navíc: t = stupeň, který drží původní TCU, b = brzda, v = auto jede / stojí)
//
// Piny na liště P1 modulu (deska v0.4/v0.5):
//   UP=GPIO7 (P1-1), DOWN=GPIO9 (P1-2), READY=GPIO8 (P1-3), LOCKUP=GPIO22 (P1-4)
//   RELÉ=GPIO21 (P1-5, aktivní HIGH), SOL1=GPIO23 (P1-7), SOL2=GPIO11 (P1-8), SOL3=GPIO6 (P1-9)
// Lišta P2 (deska v0.5):
//   čtení napětí na vodičích solenoidů SOL1/2/3 = GPIO2/3/4 (P2-7/9/11), BRZDA = GPIO28 (P2-3), VSS = GPIO10 (P2-12)
//   Při READY vyp. čtení solenoidů ukazuje, co spíná původní TCU; při READY zap. kontroluje naše výstupy.

#include <Arduino.h>
#include "lcd_st7789.h"
#include "aw4_fonts.h"

const uint8_t PIN_UP = 7, PIN_DOWN = 9, PIN_READY = 8, PIN_LOCKUP = 22;
const uint8_t PIN_RELAYS = 21, PIN_SOL1 = 23, PIN_SOL2 = 11, PIN_SOL3 = 6;
const uint8_t PIN_SNS[3] = {2, 3, 4};
const uint8_t PIN_BRAKE = 28, PIN_VSS = 10;
const uint32_t VSS_STOP_MS = 1000;     // bez pulzu VSS déle než 1 s = auto stojí
const uint32_t SNS_SETTLE_MS = 100;    // po změně výstupu počkat, než se kontroluje zpětné čtení

// Solenoidy S1, S2 podle stupně (1..4) – servisní manuál AW-4, Fig. 8.
const bool GEAR_TABLE[4][2] = {
  {true,  false},  // 1.
  {true,  true},   // 2.
  {false, true},   // 3.
  {false, false},  // 4.
};
const uint8_t LOCKUP_MIN_GEAR = 3;
// Po zapnutí READY: když auto stojí, začíná se 1. stupněm; za jízdy se převezme stupeň, který právě drží původní TCU.
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
uint8_t simTcuGear = 1; bool simBrake = false, simMoving = false;
bool brake = false;
uint8_t solFault = 0;          // bit i = výstup i nesouhlasí se zpětným čtením
uint32_t outChangedMs = 0;

// ---------------- rychlost (VSS) ----------------
volatile uint32_t vssCount = 0, vssLastMs = 0;
void vssIsr() { vssCount++; vssLastMs = millis(); }
bool vehicleStopped() {
  if (SIM) return !simMoving;
  noInterrupts(); uint32_t n = vssCount, t = vssLastMs; interrupts();
  return n == 0 || millis() - t > VSS_STOP_MS;
}

// ---------------- čtení solenoidů ----------------
bool sensed(uint8_t i);
// stupeň podle stavu S1/S2 (P/R/N hlásí stejně jako 1.)
uint8_t gearFromSol(bool s1, bool s2) {
  for (uint8_t g = 0; g < 4; g++) if (GEAR_TABLE[g][0] == s1 && GEAR_TABLE[g][1] == s2) return g + 1;
  return 1;
}
uint8_t tcuGear() { return gearFromSol(sensed(0), sensed(1)); }

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

bool sensed(uint8_t i) {   // napětí na vodiči solenoidu i (0..2)
  if (SIM) {   // simulátor: bez READY drží stupeň „původní TCU“, s READY se čte náš výstup
    if (!ready) return i < 2 ? GEAR_TABLE[simTcuGear - 1][i] : false;
    return i < 2 ? GEAR_TABLE[gear - 1][i] : lockup;
  }
  return digitalRead(PIN_SNS[i]) == HIGH;
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
const uint16_t SEG_ON = GREEN, SEG_TCU = GREY, SEG_OFF = 0x00E0;   // svítí (MANUAL) / stupeň původní TCU / nesvítí

struct Shown { int mode = -1; uint8_t mask = 0; int sol[3] = {-1, -1, -1}; int lock = -1; int fault = -1; } shown;
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
  if (force || modeChanged || solFault != shown.fault) {
    shown.mode = mode; shown.fault = solFault;
    char t[16] = "CHYBA S";
    if (solFault) { int n = strlen(t); for (int i = 0; i < 3; i++) if (solFault >> i & 1) t[n++] = '1' + i; t[n] = 0; }
    textBox(4, 1, 150, solFault ? t : ready ? "MANUAL" : "TCU", fontSmall, solFault ? RED : ready ? GREEN : GREY, BLACK);
  }
  // READY vyp.: šedě stupeň, který právě drží původní TCU (podle napětí na solenoidech)
  uint8_t mask = segMask(ready ? gear : tcuGear());
  if (force || forceDigit || mask != shown.mask || modeChanged) {
    // při změně režimu se mění barva svítících segmentů -> překreslit vše
    bool all = force || forceDigit || modeChanged;
    sevenSeg(DG_X, DG_Y, DG_W, DG_H, DG_TH, mask, shown.mask, ready ? SEG_ON : SEG_TCU, SEG_OFF, all);
    shown.mask = mask; forceDigit = false;
  }
  int lk = ready ? lockup : sensed(2);
  if (force || lk != shown.lock) {
    shown.lock = lk;
    textBox(4, 292, 110, "LOCKUP", fontSmall, lk ? ORANGE : DKGREY, BLACK);
  }
  const char *sl[3] = {"S1", "S2", "S3"};
  for (int i = 0; i < 3; i++) {
    int on = ready ? solOut(i) : sensed(i);
    if (force || on != shown.sol[i]) { shown.sol[i] = on; textBox(132 + i * 36, 292, 34, sl[i], fontSmall, on ? GREEN : DKGREY, BLACK); }
  }
}

// ---------------- sériová linka ----------------
void printState(const char *what) {
  Serial.printf("%-18s | READY=%s stupen=%c lockup=%s | rele %s | S1=%d S2=%d S3=%d | cteni %d%d%d (TCU %c) brzda=%d %s%s\n", what,
                ready ? "ZAP" : "vyp", ready ? char('0' + gear) : '-', lockup ? "ZAP" : "vyp",
                ready ? "PRITAZENA (ridi AW4)" : "odpadla (ridi TCU)", solOut(0), solOut(1), solOut(2),
                sensed(0), sensed(1), sensed(2), char('0' + tcuGear()), brake, vehicleStopped() ? "stoji" : "jede",
                solFault ? " CHYBA VYSTUPU" : "");
}

void help() {
  Serial.println("\n=== AW4 – RP2350 ===");
  if (SIM) {
    Serial.println("SIMULATOR: r = READY, u/+ = nahoru, d/- = dolu, l = LOCKUP spinac, a = ukazka, s = stav, h = napoveda");
    Serial.println("           t = stupen puvodni TCU (1-4), b = brzda, v = auto jede / stoji");
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
      else if (c == 't' || c == 'T') { simTcuGear = simTcuGear % 4 + 1; printState("TCU stupen"); }
      else if (c == 'b' || c == 'B') { simBrake = !simBrake; }
      else if (c == 'v' || c == 'V') { simMoving = !simMoving; printState(simMoving ? "auto jede" : "auto stoji"); }
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
  for (uint8_t p : PIN_SNS) pinMode(p, INPUT);          // dělič 22k/6k8 na desce
  pinMode(PIN_BRAKE, INPUT);
  pinMode(PIN_VSS, INPUT);                              // výstup komparátoru s pull-upem na desce
  if (!SIM) attachInterrupt(digitalPinToInterrupt(PIN_VSS), vssIsr, FALLING);
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

  brake = SIM ? simBrake : digitalRead(PIN_BRAKE) == HIGH;

  const char *what = nullptr;
  bool before[3] = {solOut(0), solOut(1), solOut(2)};
  if (readyIn != ready) {
    if (readyIn) gear = vehicleStopped() ? INITIAL_GEAR : tcuGear();   // za jízdy převzít stupeň původní TCU
    ready = readyIn; solFault = 0;
    what = ready ? "READY zapnuto" : "READY vypnuto";
  }
  if (ready) {
    if (up && gear < 4) { gear++; what = "NAHORU"; }
    if (down && gear > 1) { gear--; what = "DOLU"; }
  }
  bool newLockup = ready && lockIn && gear >= LOCKUP_MIN_GEAR && !brake;   // brzda lockup vždy rozepne
  if (newLockup != lockup) { lockup = newLockup; if (!what) what = lockup ? "LOCKUP zap" : "LOCKUP vyp"; }
  for (uint8_t i = 0; i < 3; i++) if (solOut(i) != before[i]) outChangedMs = millis();

  applyOutputs();   // výstupy každou smyčku

  // kontrola výstupů zpětným čtením (přerušený vodič, vadný MOSFET, zkrat na +12 V) – zatím jen hlášení
  if (ready && millis() - outChangedMs > SNS_SETTLE_MS) {
    uint8_t f = 0;
    for (uint8_t i = 0; i < 3; i++) if (sensed(i) != solOut(i)) f |= 1 << i;
    if (f != solFault) { solFault = f; if (!what) what = f ? "CHYBA VYSTUPU" : "vystupy OK"; }
  }
  updateDisplay();  // kreslí jen to, co se změnilo
  if (what) printState(what);

  static uint32_t lastRegs = 0;
  if (millis() - lastRegs > 5000) { lastRegs = millis(); lcd::refreshRegs(); }
}
