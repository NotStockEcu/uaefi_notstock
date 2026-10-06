// AW4 controller – Arduino Nano
// UP/DOWN řadí stupně 1-4, LOCKUP přepíná zámek měniče, READY přebírá solenoidy od původní TCU.
// Display: Nextion (UART, SoftwareSerial: D3 TX / D2 RX; D0/D1 jsou USB).

#include <SoftwareSerial.h>
SoftwareSerial disp(2, 3);   // RX = D2, TX = D3

const uint8_t PIN_UP = 4, PIN_DOWN = 5, PIN_READY = 6, PIN_LOCKUP = 7;
const uint8_t PIN_RELAYS = 8;
const uint8_t PIN_SOL1 = 9, PIN_SOL2 = 10, PIN_SOL3 = 11;

// Solenoidy S1, S2 podle stupně (1..4) – servisní manuál AW-4, Fig. 8.
// Lockup = solenoid 3 (ON = zamčeno). Povolen od 3. stupně (v originále 3. a 4. v poloze D).
const bool GEAR_TABLE[4][2] = {
  {true,  false},  // 1.
  {true,  true},   // 2.
  {false, true},   // 3.
  {false, false},  // 4.
};

uint8_t gear = INITIAL_GEAR;
bool lockup = false;
bool ready = false;

struct Button { uint8_t pin; bool state; bool last; uint32_t t; };
Button btns[4] = {{PIN_UP}, {PIN_DOWN}, {PIN_READY}, {PIN_LOCKUP}};

bool pressed(Button &b) {           // vrátí true na sestupnou hranu (stisk), debounce 30 ms
  bool v = digitalRead(b.pin);     // spínač na +5V: aktivní = HIGH, pull-down 10 kΩ na desce
  if (v != b.last) { b.last = v; b.t = millis(); }
  if (millis() - b.t > 30 && v != b.state) {
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

void show(const char *gearText, bool lockupOn) {
  char buf[40];
  snprintf(buf, sizeof(buf), "t0.txt=\"%s\"", gearText);
  nextionSend(buf);
  nextionSend(lockupOn ? "t1.txt=\"LOCKUP\"" : "t1.txt=\"\"");
}

void apply() {
  digitalWrite(PIN_RELAYS, ready ? LOW : HIGH);   // relé: aktivní LOW (a jen při zapnutém READY přepínači)
  if (!ready) {                              // původní TCU řídí, naše stupně vypnout
    digitalWrite(PIN_SOL1, LOW);
    digitalWrite(PIN_SOL2, LOW);
    digitalWrite(PIN_SOL3, LOW);
    show("-", false);
    return;
  }
  digitalWrite(PIN_SOL1, GEAR_TABLE[gear - 1][0]);
  digitalWrite(PIN_SOL2, GEAR_TABLE[gear - 1][1]);
  digitalWrite(PIN_SOL3, lockup);
  char g[2] = {char('0' + gear), 0};
  show(g, lockup);
}

void setup() {
  for (auto &b : btns) { pinMode(b.pin, INPUT); b.state = b.last = digitalRead(b.pin); }
  digitalWrite(PIN_RELAYS, HIGH);   // nejdřív HIGH, ať relé při startu nepřitáhnou
  pinMode(PIN_RELAYS, OUTPUT);
  pinMode(PIN_SOL1, OUTPUT); pinMode(PIN_SOL2, OUTPUT); pinMode(PIN_SOL3, OUTPUT);
  disp.begin(9600);
  apply();
}

void loop() {
  bool up = pressed(btns[0]), down = pressed(btns[1]);
  pressed(btns[2]); pressed(btns[3]);        // aktualizace stavů

  bool newReady = btns[2].state;
  if (newReady != ready) { ready = newReady; if (ready) { gear = INITIAL_GEAR; lockup = false; } apply(); }

  if (ready) {
    if (up   && gear < 4) gear++;
    if (down && gear > 1) gear--;
    lockup = btns[3].state && gear >= LOCKUP_MIN_GEAR;   // lockup jen od 3. a po dobu držení tlačítka
    apply();
  }

  static uint32_t last = 0;
  if (millis() - last > 500) { last = millis(); apply(); }   // periodické obnovení (odolnost proti rušení)
}
