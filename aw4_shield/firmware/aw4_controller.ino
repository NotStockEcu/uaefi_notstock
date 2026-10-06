// AW4 controller – Arduino Micro
// UP/DOWN řadí stupně 1-4, LOCKUP přepíná zámek měniče, READY přebírá solenoidy od původní TCU.
// Display: jedna 7segmentovka přes 74HC595 (společná katoda).

const uint8_t PIN_UP = 4, PIN_DOWN = 5, PIN_READY = 6, PIN_LOCKUP = 7;
const uint8_t PIN_RELAYS = 8;
const uint8_t PIN_SOL1 = 9, PIN_SOL2 = 10, PIN_SOL3 = 11;
const uint8_t PIN_SER = 12, PIN_SRCLK = 13, PIN_RCLK = A0;

// Solenoidy S1, S2 podle stupně (1..4). OVĚŘIT na převodovce!
const bool GEAR_TABLE[4][2] = {
  {true,  false},  // 1.
  {true,  true},   // 2.
  {false, true},   // 3.
  {false, false},  // 4.
};

// 7seg, bity: a=0 b=1 c=2 d=3 e=4 f=5 g=6 dp=7 (upravit podle zapojení)
const uint8_t SEG[] = {
  0x00, 0x06, 0x5B, 0x4F, 0x66,  // ' ', 1, 2, 3, 4
};
const uint8_t SEG_DASH = 0x40;
const uint8_t SEG_DP   = 0x80;

uint8_t gear = 1;
bool lockup = false;
bool ready = false;

struct Button { uint8_t pin; bool state; bool last; uint32_t t; };
Button btns[4] = {{PIN_UP}, {PIN_DOWN}, {PIN_READY}, {PIN_LOCKUP}};

bool pressed(Button &b) {           // vrátí true na sestupnou hranu (stisk), debounce 30 ms
  bool v = !digitalRead(b.pin);     // opto: aktivní = LOW
  if (v != b.last) { b.last = v; b.t = millis(); }
  if (millis() - b.t > 30 && v != b.state) {
    b.state = v;
    return v;
  }
  return false;
}

void show(uint8_t value) {
  digitalWrite(PIN_RCLK, LOW);
  shiftOut(PIN_SER, PIN_SRCLK, MSBFIRST, value);
  digitalWrite(PIN_RCLK, HIGH);
}

void apply() {
  digitalWrite(PIN_RELAYS, ready);
  if (!ready) {                              // původní TCU řídí, naše stupně vypnout
    digitalWrite(PIN_SOL1, LOW);
    digitalWrite(PIN_SOL2, LOW);
    digitalWrite(PIN_SOL3, LOW);
    show(SEG_DASH);
    return;
  }
  digitalWrite(PIN_SOL1, GEAR_TABLE[gear - 1][0]);
  digitalWrite(PIN_SOL2, GEAR_TABLE[gear - 1][1]);
  digitalWrite(PIN_SOL3, lockup);
  show(SEG[gear] | (lockup ? SEG_DP : 0));
}

void setup() {
  for (auto &b : btns) { pinMode(b.pin, INPUT_PULLUP); b.state = b.last = !digitalRead(b.pin); }
  pinMode(PIN_RELAYS, OUTPUT);
  pinMode(PIN_SOL1, OUTPUT); pinMode(PIN_SOL2, OUTPUT); pinMode(PIN_SOL3, OUTPUT);
  pinMode(PIN_SER, OUTPUT); pinMode(PIN_SRCLK, OUTPUT); pinMode(PIN_RCLK, OUTPUT);
  apply();
}

void loop() {
  bool up = pressed(btns[0]), down = pressed(btns[1]);
  pressed(btns[2]); pressed(btns[3]);        // aktualizace stavů

  bool newReady = btns[2].state;
  if (newReady != ready) { ready = newReady; if (ready) { gear = 1; lockup = false; } apply(); }

  if (ready) {
    if (up   && gear < 4) gear++;
    if (down && gear > 1) gear--;
    lockup = btns[3].state && gear == 4;     // lockup jen ve 4. a po dobu držení tlačítka
    apply();
  }

  static uint32_t last = 0;
  if (millis() - last > 500) { last = millis(); apply(); }   // periodické obnovení (odolnost proti rušení)
}
