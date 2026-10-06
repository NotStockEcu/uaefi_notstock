// AW4 controller – Arduino Nano (AW4 shield v0.3)
// UP/DOWN řadí stupně 1-4, LOCKUP spínač zamyká měnič, READY přebírá solenoidy od původní TCU.
// Displej: Nextion (UART, SoftwareSerial: D3 TX / D2 RX; D0/D1 zůstávají pro USB).

#include <SoftwareSerial.h>
SoftwareSerial disp(2, 3);   // RX = D2, TX = D3

const uint8_t PIN_UP = 4, PIN_DOWN = 5, PIN_READY = 6, PIN_LOCKUP = 7;   // spínače na +5 V, aktivní HIGH
const uint8_t PIN_RELAYS = 8;                                           // relé K1-K3, aktivní LOW
const uint8_t PIN_SOL1 = 9, PIN_SOL2 = 10, PIN_SOL3 = 11;               // HIGH = +12 V na solenoid

// Solenoidy S1, S2 podle stupně (1..4) – servisní manuál AW-4, Fig. 8.
// Lockup = solenoid 3 (ON = zamčeno). Povolen od 3. stupně (v originále 3. a 4. v poloze D).
const bool GEAR_TABLE[4][2] = {
  {true,  false},  // 1.
  {true,  true},   // 2.
  {false, true},   // 3.
  {false, false},  // 4.
};
const uint8_t LOCKUP_MIN_GEAR = 3;
// POZOR: po zapnutí READY nevíme, jaký stupeň zrovna drží původní TCU. Zapínat READY ve stoje,
// nebo nastavit INITIAL_GEAR podle situace, jinak hrozí přetočení motoru.
const uint8_t INITIAL_GEAR = 1;

const uint32_t DEBOUNCE_MS = 30;
const uint32_t DISPLAY_REFRESH_MS = 500;   // displej se posílá znovu (odolnost proti rušení)

uint8_t gear = INITIAL_GEAR;
bool lockup = false;
bool ready = false;

struct Button { uint8_t pin; bool state; bool last; uint32_t t; };
Button btns[4] = {{PIN_UP, false, false, 0}, {PIN_DOWN, false, false, 0},
                  {PIN_READY, false, false, 0}, {PIN_LOCKUP, false, false, 0}};

// Aktualizuje stav (debounce) a vrátí true v okamžiku sepnutí (náběžná hrana).
bool pressed(Button &b) {
  bool v = digitalRead(b.pin);
  if (v != b.last) { b.last = v; b.t = millis(); }
  if (millis() - b.t > DEBOUNCE_MS && v != b.state) {
    b.state = v;
    return v;
  }
  return false;
}

// Nextion: textové pole "t0" s číslem stupně, "t1" pro lockup; příkaz končí třemi 0xFF.
void nextionSend(const char *cmd) {
  disp.print(cmd);
  disp.write(0xFF); disp.write(0xFF); disp.write(0xFF);
}

void updateDisplay() {
  char buf[24];
  if (ready) snprintf(buf, sizeof(buf), "t0.txt=\"%u\"", gear);
  else       snprintf(buf, sizeof(buf), "t0.txt=\"-\"");
  nextionSend(buf);
  nextionSend(ready && lockup ? "t1.txt=\"LOCKUP\"" : "t1.txt=\"\"");
}

void applyOutputs() {
  if (ready) {
    // nejdřív nastavit výstupy, pak teprve přepnout relé na naše MOSFETy
    digitalWrite(PIN_SOL1, GEAR_TABLE[gear - 1][0]);
    digitalWrite(PIN_SOL2, GEAR_TABLE[gear - 1][1]);
    digitalWrite(PIN_SOL3, lockup);
    digitalWrite(PIN_RELAYS, LOW);
  } else {
    // nejdřív vrátit relé na původní TCU, pak vypnout naše výstupy
    digitalWrite(PIN_RELAYS, HIGH);
    digitalWrite(PIN_SOL1, LOW);
    digitalWrite(PIN_SOL2, LOW);
    digitalWrite(PIN_SOL3, LOW);
  }
}

void setup() {
  digitalWrite(PIN_RELAYS, HIGH);   // nejdřív HIGH, ať relé při startu nepřitáhnou
  pinMode(PIN_RELAYS, OUTPUT);
  pinMode(PIN_SOL1, OUTPUT); pinMode(PIN_SOL2, OUTPUT); pinMode(PIN_SOL3, OUTPUT);
  for (auto &b : btns) { pinMode(b.pin, INPUT); b.state = b.last = digitalRead(b.pin); }
  disp.begin(9600);
  applyOutputs();
  updateDisplay();
}

void loop() {
  bool up = pressed(btns[0]);
  bool down = pressed(btns[1]);
  pressed(btns[2]);
  pressed(btns[3]);

  bool changed = false;
  bool newReady = btns[2].state;
  if (newReady != ready) {
    ready = newReady;
    if (ready) gear = INITIAL_GEAR;
    changed = true;
  }
  if (ready) {
    if (up   && gear < 4) { gear++; changed = true; }
    if (down && gear > 1) { gear--; changed = true; }
  }
  bool newLockup = ready && btns[3].state && gear >= LOCKUP_MIN_GEAR;
  if (newLockup != lockup) { lockup = newLockup; changed = true; }

  applyOutputs();   // výstupy každou smyčku (rychlé, bez blokování)

  static uint32_t lastDisplay = 0;
  if (changed || millis() - lastDisplay > DISPLAY_REFRESH_MS) {
    lastDisplay = millis();
    updateDisplay();
  }
}
