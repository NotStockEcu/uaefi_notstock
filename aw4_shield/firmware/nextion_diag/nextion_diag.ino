// AW4 shield – diagnostika spojení s displejem Nextion.
// Zkusí obě zapojení (TX/RX normálně i prohozeně) a všechny běžné rychlosti
// a vypíše, při jaké kombinaci displej odpověděl. Sériový monitor: 115200 Bd.
//
// Zapojení jako u testu: displej TX a RX na Nano D2 a D3 (na pořadí nezáleží, to zjistí test),
// +5V na 5V, GND na GND.

#include <SoftwareSerial.h>

const long BAUDS[] = {9600, 115200, 57600, 38400, 19200, 4800, 2400, 250000};
const uint8_t NBAUDS = sizeof(BAUDS) / sizeof(BAUDS[0]);

void sendCmd(SoftwareSerial &s, const char *cmd) {
  s.print(cmd);
  s.write(0xFF); s.write(0xFF); s.write(0xFF);
}

// Pošle dotaz a vrátí počet přijatých bajtů; vypíše je hexadecimálně.
uint8_t probe(uint8_t rx, uint8_t tx, long baud, bool verbose) {
  SoftwareSerial port(rx, tx);
  SoftwareSerial *s = &port;
  s->begin(baud);
  s->listen();
  delay(5);
  while (s->available()) s->read();
  sendCmd(*s, "");            // ukončí případný rozpracovaný příkaz
  sendCmd(*s, "get dim");     // displej odpoví 0x71 + hodnota jasu
  sendCmd(*s, "aw4?");        // neplatný příkaz – displej odpoví chybou 0x00/0x1A
  uint8_t n = 0, buf[24];
  uint32_t t = millis();
  while (millis() - t < 150) {
    if (s->available()) { uint8_t b = s->read(); if (n < sizeof(buf)) buf[n] = b; n++; }
  }
  if (verbose && n) {
    Serial.print(F("   prijato:"));
    for (uint8_t i = 0; i < n && i < sizeof(buf); i++) { Serial.print(' '); if (buf[i] < 16) Serial.print('0'); Serial.print(buf[i], HEX); }
    Serial.println();
  }
  s->end();
  pinMode(2, INPUT); pinMode(3, INPUT);
  return n;
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println(F("\n=== AW4 – diagnostika displeje Nextion ==="));
  bool found = false;
  for (uint8_t w = 0; w < 2 && !found; w++) {
    uint8_t rx = w == 0 ? 2 : 3, tx = w == 0 ? 3 : 2;
    for (uint8_t i = 0; i < NBAUDS && !found; i++) {
      Serial.print(F("zapojeni ")); Serial.print(w == 0 ? F("normalni  (displej TX->D2, RX->D3)") : F("prohozene (displej TX->D3, RX->D2)"));
      Serial.print(F(", ")); Serial.print(BAUDS[i]); Serial.print(F(" Bd: "));
      uint8_t n = probe(rx, tx, BAUDS[i], false);
      if (n) {
        Serial.println(F("ODPOVED!"));
        probe(rx, tx, BAUDS[i], true);
        found = true;
        Serial.println();
        if (w == 1) Serial.println(F(">> Prohodte vodice na D2 a D3 (TX displeje musi na D2)."));
        if (BAUDS[i] != 9600) {
          Serial.println(F(">> Displej ma jinou rychlost nez 9600 Bd. Nastavuji trvale 9600 (prikaz bauds=9600)..."));
          SoftwareSerial s(rx, tx); s.begin(BAUDS[i]); sendCmd(s, "bauds=9600"); delay(200);
          Serial.println(F(">> Hotovo – vypnete a zapnete displej a spustte test znovu."));
        }
        if (w == 0 && BAUDS[i] == 9600) Serial.println(F(">> Spojeni je v poradku. Pokud display_test hlasi 'nezname pole', chybi nahrany projekt (.tft)."));
      } else {
        Serial.println(F("nic"));
      }
    }
  }
  if (!found) {
    Serial.println(F("\nDisplej neodpovida pri zadne kombinaci. Zkontrolujte:"));
    Serial.println(F(" - sviti displej? (napajeni +5V z pinu 5V, ne 3V3; GND spolecna s Nanem)"));
    Serial.println(F(" - jsou vodice opravdu na D2 a D3 (ne na RX0/TX1 = D0/D1)?"));
    Serial.println(F(" - dobry kontakt v konektoru displeje (zkuste vodice znovu zasunout)"));
  }
  Serial.println(F("\nHotovo. Pro novy test stisknete RESET."));
}

void loop() {}
