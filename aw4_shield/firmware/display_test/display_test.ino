// AW4 shield – test displeje Nextion NX3224T024 s Arduinem Nano (bez desky, na stole).
//
// Zapojení (stejné piny jako na desce, konektor J5):
//   Nextion +5V (červený) -> Nano 5V
//   Nextion GND (černý)   -> Nano GND
//   Nextion TX  (žlutý)   -> Nano D2   (Nano přijímá)
//   Nextion RX  (modrý)   -> Nano D3   (Nano vysílá)
//   Barvy vodičů ověřte podle popisu na konektoru displeje (+5V / TX / RX / GND).
//
// Na displeji musí být nahraný projekt podle nextion/README.md (stránka page0, pole t0, t1, t2).
// Sériový monitor: 115200 Bd – vypisuje, jestli displej příkazy přijal.

#include <SoftwareSerial.h>
SoftwareSerial disp(2, 3);   // RX = D2, TX = D3

// barvy Nextion (RGB565)
const uint16_t COL_GREY = 33808, COL_GREEN = 2016, COL_ORANGE = 64512, COL_WHITE = 65535;

void send(const char *cmd) {
  disp.print(cmd);
  disp.write(0xFF); disp.write(0xFF); disp.write(0xFF);
}

// Přečte odpověď displeje (při bkcmd=3 přijde po každém příkazu) a vypíše ji česky.
void report(const char *cmd) {
  uint8_t buf[8]; uint8_t n = 0; uint32_t t0 = millis();
  while (millis() - t0 < 60 && n < sizeof(buf)) {
    if (disp.available()) buf[n++] = disp.read();
    if (n >= 4 && buf[n - 1] == 0xFF && buf[n - 2] == 0xFF && buf[n - 3] == 0xFF) break;
  }
  Serial.print(cmd); Serial.print("  ->  ");
  if (n == 0) { Serial.println("bez odpovědi (zkontrolujte TX/RX, napájení, 9600 Bd)"); return; }
  switch (buf[0]) {
    case 0x01: Serial.println("OK"); break;
    case 0x00: Serial.println("neplatný příkaz"); break;
    case 0x02: Serial.println("neplatná komponenta"); break;
    case 0x03: Serial.println("neplatná stránka (chybí page0?)"); break;
    case 0x1A: Serial.println("neznámé pole/atribut (chybí t0/t1/t2 nebo jsou jinak pojmenované?)"); break;
    case 0x1C: Serial.println("přiřazení selhalo (text je delší než txt_maxl pole?)"); break;
    case 0x24: Serial.println("přetečení bufferu displeje"); break;
    default:   Serial.print("kód 0x"); Serial.println(buf[0], HEX);
  }
}

void cmd(const char *c) { send(c); report(c); }

void showState(const char *gear, bool manual, bool lockup) {
  char b[32];
  snprintf(b, sizeof(b), "t0.pco=%u", manual ? COL_GREEN : COL_GREY);   cmd(b);
  snprintf(b, sizeof(b), "t0.txt=\"%s\"", gear);                         cmd(b);
  snprintf(b, sizeof(b), "t1.pco=%u", COL_ORANGE);                       cmd(b);
  cmd(lockup ? "t1.txt=\"LOCKUP\"" : "t1.txt=\"\"");
  snprintf(b, sizeof(b), "t2.pco=%u", COL_WHITE);                        cmd(b);
  cmd(manual ? "t2.txt=\"MANUAL\"" : "t2.txt=\"TCU\"");
}

void setup() {
  Serial.begin(115200);
  disp.begin(9600);
  delay(600);                 // displej po zapnutí startuje ~0,5 s
  Serial.println(F("AW4 – test displeje Nextion"));
  send(""); delay(20);        // vyčistí případný rozpracovaný příkaz
  while (disp.available()) disp.read();
  cmd("bkcmd=3");             // displej potvrzuje každý příkaz (jen pro test)
  cmd("page 0");
  cmd("dim=100");
}

void loop() {
  static uint8_t step = 0;
  switch (step) {
    case 0: showState("-", false, false); break;   // READY vypnuto, řídí původní TCU
    case 1: showState("1", true,  false); break;
    case 2: showState("2", true,  false); break;
    case 3: showState("3", true,  false); break;
    case 4: showState("3", true,  true);  break;   // lockup
    case 5: showState("4", true,  true);  break;
    case 6: showState("4", true,  false); break;
  }
  Serial.println(F("----"));
  step = (step + 1) % 7;
  delay(1500);
}
