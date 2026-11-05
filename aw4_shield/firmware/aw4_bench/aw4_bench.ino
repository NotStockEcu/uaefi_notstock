// AW4 shield – SIMULÁTOR NA STOLE (Nano + displej Nextion, bez desky a bez převodovky).
//
// Stejná logika řazení jako hlavní firmware aw4_controller.ino, ale ovládá se písmeny
// ze Serial Monitoru (115200 Bd, "Both NL & CR" nebo "No line ending"):
//   r = READY zap/vyp     u nebo + = nahoru     d nebo - = dolů
//   l = LOCKUP spínač zap/vyp     a = automatická ukázka     s = stav     h = nápověda
//
// Vstupy D4–D7 se NEČTOU – simulátor se v autě nikdy sám nezapne.
// Výstupy D8–D11 se nastavují jako na desce; volitelně na ně můžete dát LED:
//   D9, D10, D11 (solenoidy S1, S2, S3/lockup): pin -> rezistor 1 kΩ -> LED -> GND (svítí = solenoid zapnutý)
//   D8 (relé, aktivní LOW): 5V -> rezistor 1 kΩ -> LED -> D8 (svítí = relé přitažená)
//
// Displej: modrý (TX displeje) -> D2, žlutý (RX displeje) -> D3, červený -> 5V, černý -> GND.

#include <SoftwareSerial.h>
SoftwareSerial disp(2, 3);   // RX = D2, TX = D3

const uint8_t PIN_RELAYS = 8;
const uint8_t PIN_SOL1 = 9, PIN_SOL2 = 10, PIN_SOL3 = 11;

// Solenoidy S1, S2 podle stupně (1..4) – servisní manuál AW-4, Fig. 8.
const bool GEAR_TABLE[4][2] = {
  {true,  false},  // 1.
  {true,  true},   // 2.
  {false, true},   // 3.
  {false, false},  // 4.
};
const uint8_t LOCKUP_MIN_GEAR = 3;
const uint8_t INITIAL_GEAR = 1;
const uint16_t COL_GREY = 33808, COL_GREEN = 2016, COL_YELLOW = 65504;

uint8_t gear = INITIAL_GEAR;
bool ready = false, lockupSwitch = false, lockup = false;
bool autoDemo = false;

void nextionSend(const char *cmd) {
  disp.print(cmd);
  disp.write(0xFF); disp.write(0xFF); disp.write(0xFF);
}

void updateDisplay() {
  char buf[24];
  snprintf(buf, sizeof(buf), "t0.pco=%u", ready ? COL_GREEN : COL_GREY);
  nextionSend(buf);
  if (ready) snprintf(buf, sizeof(buf), "t0.txt=\"%u\"", gear);
  else       snprintf(buf, sizeof(buf), "t0.txt=\"-\"");
  nextionSend(buf);
  nextionSend(lockup ? "t1.txt=\"LOCKUP\"" : "t1.txt=\"\"");
  snprintf(buf, sizeof(buf), "t2.pco=%u", COL_YELLOW);
  nextionSend(buf);
  nextionSend(ready ? "t2.txt=\"MANUAL\"" : "t2.txt=\"TCU\"");   // žlutě = simulátor
}

void applyOutputs() {
  if (ready) {
    digitalWrite(PIN_SOL1, GEAR_TABLE[gear - 1][0]);
    digitalWrite(PIN_SOL2, GEAR_TABLE[gear - 1][1]);
    digitalWrite(PIN_SOL3, lockup);
    digitalWrite(PIN_RELAYS, LOW);
  } else {
    digitalWrite(PIN_RELAYS, HIGH);
    digitalWrite(PIN_SOL1, LOW);
    digitalWrite(PIN_SOL2, LOW);
    digitalWrite(PIN_SOL3, LOW);
  }
}

void printState(const __FlashStringHelper *what) {
  Serial.print(what);
  Serial.print(F("  | READY="));  Serial.print(ready ? F("ZAP") : F("vyp"));
  Serial.print(F("  stupen="));   if (ready) Serial.print(gear); else Serial.print('-');
  Serial.print(F("  LOCKUP spinac=")); Serial.print(lockupSwitch ? F("zap") : F("vyp"));
  Serial.print(F("  ||  rele K1-K3: ")); Serial.print(ready ? F("PRITAZENA (ridi Arduino)") : F("odpadla (ridi puvodni TCU)"));
  Serial.print(F("  S1=")); Serial.print(digitalRead(PIN_SOL1) ? F("ON ") : F("off"));
  Serial.print(F(" S2=")); Serial.print(digitalRead(PIN_SOL2) ? F("ON ") : F("off"));
  Serial.print(F(" S3/lockup=")); Serial.println(digitalRead(PIN_SOL3) ? F("ON") : F("off"));
}

void update(const __FlashStringHelper *what) {
  lockup = ready && lockupSwitch && gear >= LOCKUP_MIN_GEAR;
  applyOutputs();
  updateDisplay();
  printState(what);
}

void help() {
  Serial.println(F("\n=== AW4 simulator na stole ==="));
  Serial.println(F(" r = READY zap/vyp   u/+ = nahoru   d/- = dolu   l = LOCKUP spinac"));
  Serial.println(F(" a = automaticka ukazka (zap/vyp)   s = stav   h = napoveda"));
  Serial.println(F(" Radit jde jen pri READY zap; lockup se zapne jen ve 3. a 4. stupni.\n"));
}

void command(char c) {
  switch (c) {
    case 'r': case 'R':
      ready = !ready;
      if (ready) gear = INITIAL_GEAR;
      update(ready ? F("READY zapnuto") : F("READY vypnuto"));
      break;
    case 'u': case 'U': case '+':
      if (!ready) { Serial.println(F("(radit jde jen pri READY zap – stisknete r)")); break; }
      if (gear < 4) { gear++; update(F("NAHORU")); } else Serial.println(F("(uz je 4. stupen)"));
      break;
    case 'd': case 'D': case '-':
      if (!ready) { Serial.println(F("(radit jde jen pri READY zap – stisknete r)")); break; }
      if (gear > 1) { gear--; update(F("DOLU")); } else Serial.println(F("(uz je 1. stupen)"));
      break;
    case 'l': case 'L':
      lockupSwitch = !lockupSwitch;
      update(lockupSwitch ? F("LOCKUP spinac zap") : F("LOCKUP spinac vyp"));
      if (lockupSwitch && !(ready && gear >= LOCKUP_MIN_GEAR)) Serial.println(F("(lockup se sepne az ve 3. stupni pri READY)"));
      break;
    case 'a': case 'A':
      autoDemo = !autoDemo;
      Serial.println(autoDemo ? F("Automaticka ukazka ZAP (a = vypnout)") : F("Automaticka ukazka VYP"));
      break;
    case 's': case 'S': printState(F("STAV")); break;
    case 'h': case 'H': case '?': help(); break;
    default: break;   // konce radku apod. ignorovat
  }
}

// Automatická ukázka: READY -> 1 -> 2 -> 3 -> lockup zap -> 4 -> 3 -> 2 -> 1 -> lockup vyp -> READY vyp.
void demoStep() {
  static uint8_t step = 0;
  static uint32_t last = 0;
  if (millis() - last < 1500) return;
  last = millis();
  const char seq[] = "ruuludddlr";      // 10 kroků, končí ve výchozím stavu
  command(seq[step]);
  step = (step + 1) % (sizeof(seq) - 1);
}

void setup() {
  digitalWrite(PIN_RELAYS, HIGH);
  pinMode(PIN_RELAYS, OUTPUT);
  pinMode(PIN_SOL1, OUTPUT); pinMode(PIN_SOL2, OUTPUT); pinMode(PIN_SOL3, OUTPUT);
  Serial.begin(115200);
  disp.begin(9600);
  delay(600);
  nextionSend("");
  nextionSend("page 0");
  help();
  update(F("START"));
}

void loop() {
  while (Serial.available()) command(Serial.read());
  if (autoDemo) demoStep();
  static uint32_t lastDisplay = 0;
  if (millis() - lastDisplay > 500) { lastDisplay = millis(); updateDisplay(); }
}
